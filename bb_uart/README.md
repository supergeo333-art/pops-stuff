# STC15W104 bit-bang UART (19200 8N1, interrupt driven)

| File | |
|---|---|
| `uart_drv_orig.asm` | driver as received |
| `uart_drv.asm` | fixed + optimized driver (same API, same echo `main`) |
| `stc15w-sdcc.inc` | minimal SFR header (only the symbols the driver uses) — replace with your full header |
| `sim/sim8051.py` | cycle-counted 8051 simulator, STC15 1T timings, Timer0/Timer2 auto-reload, INT4, 2-level priority |
| `sim/tb.py` | test bench: drives P3.0, decodes P3.1, checks the echo |

```
sudo apt install sdcc          # sdas8051 + sdld
cd sim && python3 tb.py ../uart_drv.asm ../uart_drv_orig.asm
```

## Bugs found in the original

1. **Crash on framing error / BREAK.** The RX state table's last entry is
   `SWRX_S10 - SWTX_STATE_TABLE` (should be `SWRX_STATE_TABLE`). It assembles to `0xAD`, so
   `JMP @A+DPTR` lands at 0x258, the middle of `POP B` in the Timer2 ISR. It executes
   `MOVX @DPTR,A` and `RETI`s with 6 bytes pushed and 2 popped. Any bit error, BREAK or
   lost start bit triggers it.
2. **Back-to-back frames are lost.** `TX_TRANSMIT` only starts Timer2, so the start bit
   goes out one bit time later: each echoed byte takes 10 bits + ISR latency. At the
   same baud rate that debt accumulates. After about 12 bytes the main loop is still
   spinning on `JC` while INT4 is disabled (`RX_BLIND` disables it on return), and a
   start edge is missed. With the table bug fixed alone, only 5/64 back-to-back bytes echo correctly.
3. **No noise rejection.** The start bit is re-checked about 50 cycles (4% of a bit) after the edge, so a
   0.15-bit glitch is accepted as a start bit and corrupts the following bytes.
4. `SWTI` is set at the *start* bit and again at the end, so it can't be used as
   "transmit complete".
5. Minor: `UART_INIT` loads Timer2 with `RXBAUD`, `BAUD_RATE` is truncated (1151, not 1152),
   `SETB EA` / `ORL AUXR,#T0x12` inside the ISR are redundant, and `MOV TMOD,#0` clobbers Timer1.
   The comment "CLR TR0 clears any pending interrupt" is also wrong: `CLR TR0` does not clear TF0.

## What changed in `uart_drv.asm`

* RX and TX run independently (full duplex). INT4 stays armed; `RX_OPENBUFF` is called once.
* 16-byte ring at 0x40 (15 usable), overflow → `SWBFF`. `RX_POLL` (non-blocking) and
  `RX_BLIND` (blocking; `C = SWFE|SWBRK|SWBFF`) read from it.
* INT4 ISR only restarts Timer0 (no context save). First overflow is at mid start bit,
  which validates the start bit; then one overflow per bit.
* The jump tables are replaced by down counters. Timer0 and Timer2 save only PSW/ACC.
  The line is sampled or driven as the 2nd instruction of the ISR, so timing doesn't depend on state.
* Stop-bit tick handles framing error and BREAK directly, with no waiting state.
  INT4 is falling-edge, so re-arming it while the line is low is safe.
* TX start bit is driven by `TX_TRANSMIT`. The first Timer2 period is shortened by
  `TX_CORR`, and `STXBSY` clears only after the full stop bit.
* Timer0 is high priority (`PT0`); Timer2 and INT4 are fixed low on STC15.
* `RX_PROBE = 1` toggles P3.2 at each RX sample instant, for trimming `THALF_CORR` on a scope.

## Results (simulator)

| check | original | orig + table fix | new |
|---|---|---|---|
| echo, 2-bit idle gaps | pass | pass | pass |
| echo, back-to-back, host baud 0 / −2 / +2 % | crash / crash / pass | 5/64, 2/64, pass | pass ×3 |
| 0.15-bit glitch on idle line | crash | garbage bytes | rejected |
| framing error, 25-bit BREAK | crash | pass | pass |
| 400 bytes with host 4 % fast | crash | 109 lost, desync | 3 dropped, `SWBFF`=1, rest in order |
| RX sample point (idle traffic) | 0.544–0.556 bit | same | 0.498–0.503 bit |
| RX baud tolerance (gapped) | −5.0 / +5.5 % | same | ±5.5 % |
| worst TX edge error, full duplex | — | 7.4 % | 5.7 % |
| ISR length INT4 / T0 / T2 (cycles) | 111 / 72–91 / 74–77 | | 41 / 44–66 / 40–49 |
| CPU load while echoing | 9.6 % | | 5.6 % |
| code (main + driver) | 396 B | | 299 B |

Remaining TX jitter: the RX stop-bit ISR (66 cycles) can delay a TX edge when they
coincide, because Timer2 cannot be made high priority on STC15. 5.7% of a bit is
well inside a receiver's margin.

## Model assumptions (confirm on hardware)

* Instruction cycle counts are from the STC15 datasheet table. Interrupt entry is modelled as
  6 clocks; tests also pass with 4 and 9.
* STC15 timer write semantics: TLx/THx written while stopped load the counter and reload; written
  while running, only the reload register. Timer2 is assumed to behave like Timer0 mode 0.
* INT4's hidden request flag: tests pass whether or not it latches edges while EX4=0.
* `THALF_CORR` (32) and `TX_CORR` (13) come from this model. Verify with `RX_PROBE=1`: the
  P3.2 toggles should be centred in the RX bit cells. For TX, the start bit should be
  one bit long (52.08 µs).
