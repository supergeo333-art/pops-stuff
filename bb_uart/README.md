# STC15W104 bit-bang UART (19200 8N1, interrupt driven)

| File | |
|---|---|
| `uart_drv_orig.asm` | driver as received |
| `uart_drv.asm` | fixed + optimized driver (same API, same echo `main`) |
| `stc15w-sdcc.inc` | your STC15W SFR header |
| `sim/sim8051.py` | cycle-counted 8051 simulator, STC15 1T timings, Timer0/Timer2 auto-reload, INT4, 2-level priority |
| `sim/tb.py` | test bench: drives P3.0, decodes P3.1, checks the echo |

```
sudo apt install sdcc          # sdas8051 + sdld
cd sim && python3 tb.py ../uart_drv.asm ../uart_drv_orig.asm
python3 tb.py ../uart_drv.asm --fosc 33000000 --baud 38400      # any clock / baud
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
| worst TX edge error, full duplex | — | 7.4 % (at +2 %) | 5.7 % |
| ISR length INT4 / T0 / T2 (cycles) | 111 / 72–91 / 74–77 | | 41 / 44–66 / 40–49 |
| CPU load while echoing | 9.6 % | | 5.6 % |
| code (main + driver) | 396 B | | 299 B |

Remaining TX jitter: the RX stop-bit ISR (66 cycles) can delay a TX edge when they
coincide, because Timer2 cannot be made high priority on STC15. 5.7% of a bit is
well inside a receiver's margin.

## 33 MHz analysis

To run at 33 MHz, change one line: `MPU_CLK_HZ .EQU 33000000`. The correction constants are in
clock cycles, so they don't change with the clock.

* Divisor: 33 000 000 / 19200 = 1718.75, rounded to 1719 (+0.015 % baud error; truncating gives 1718, −0.044 %).
  All timer reload values fit in 16 bits (half bit = 859).
* **Your own header comment gives 30.041 MHz as the maximum.** Check that the STC-ISP tool
  and your part's datasheet allow 33 MHz for the IRC before relying on it.

New driver at 33 MHz (simulator):

| baud | cycles/bit | checks | RX sample range | worst TX edge (full duplex) | notes |
|---|---|---|---|---|---|
| 19200 | 1718.75 | 17/17 | 0.498–0.503 | 42 cyc = 2.4 % (5.7 % at 22.1 MHz) | CPU load 3.8 % (5.6 % at 22.1 MHz) |
| 38400 | 859.4 | 17/17 | 0.492–0.504 | < 10 % | clean |
| 57600 | 572.9 | 16/17 | 0.494–0.506 | < 10 % | fails only the 4 %-fast back-to-back stress test (20 of 400 dropped; ±2 % passes) |
| 76800 | 429.7 | 15/17 | 0.495–0.518 | 15.9 % | echo still correct; jitter over budget |
| 115200 | 286.5 | 10/17 | 0.474–0.635 | 21.8 % | too fast for a bit-bang UART on this core |

The limit comes from the fixed ISR lengths (INT4 41, Timer0 44–66, Timer2 40–49 cycles).
When two ISRs collide, one delays the other by up to that much. Keep that under
about 10 % of a bit, i.e. ≥ 600 cycles/bit. **At 33 MHz, 38400 is fully clean and 57600
is usable** with normal ±2 % clocks. 19200 has large margins.

The original driver at 33 MHz / 19200 behaves the same as at 22.1 MHz: it crashes at 0x258 in 9 of 15
tests, and with the table fix it still loses back-to-back bytes (7/64). Its fixed `THALF_CORR = 51` also
drifts the sample point later as the baud rate rises (0.60 at 57600, 0.70 at 115200).

## Header (`stc15w-sdcc.inc`) findings

* **`PT0` (and all of `IP`, `IP2`, `IE2`, `AUXR`, …) are byte masks, not bit addresses.**
  `SETB PT0` assembles silently to `SETB 0x02`, which sets bit 2 of IRAM 0x20 (`SWBRK` here).
  The driver now uses `ORL IP,#PT0`. The same trap applies to any `SETB`/`CLR` on these names.
  The bench now checks that no stray flags are set after init.
* `GATE`, `M1`, `M0` are defined twice (Timer1 then Timer0 values). The last definition wins, so
  the Timer1 masks are lost.
* `PS2` is defined twice (IP2 = 0x01, WDT_CONTR = 0x04). The last wins, so `ORL IP2,#PS2` would set the wrong bit.
* `IAP_ADDRH` is defined as both 0xC3 and 0xC4, and `IAP_ADDRL` is missing. 0xC4 should be
  `IAP_ADDRL`/`ISP_ADDRL`. As written, IAP code writes the high address to the low register.
* `S3REN .equ 0x20` duplicates `S3SM2`. It should be 0x10. The S3CON bit comments are also numbered backwards.
* Cosmetic: P1/P2/P3/P5 comments say "I/O port 0". `BRT` (0x9C) is not an STC15 register.

## Model assumptions (confirm on hardware)

* Instruction cycle counts are from the STC15 datasheet table. Interrupt entry is modelled as
  6 clocks; tests also pass with 4 and 9.
* STC15 timer write semantics: TLx/THx written while stopped load the counter and reload; written
  while running, only the reload register. Timer2 is assumed to behave like Timer0 mode 0.
* INT4's hidden request flag: tests pass whether or not it latches edges while EX4=0.
* `THALF_CORR` (32) and `TX_CORR` (13) come from this model. Verify with `RX_PROBE=1`: the
  P3.2 toggles should be centred in the RX bit cells. For TX, the start bit should be
  one bit long (52.08 µs).
