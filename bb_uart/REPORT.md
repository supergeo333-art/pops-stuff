# Bit-Bang UART for the STC15W104: Review, Optimization and Verification Report

**Target:** STC15W104 (8051-compatible, STC-Y5 "1T" core). SDCC `sdas8051` assembler.
**Function:** software UART, 8N1, 19200 baud, interrupt driven, echo test program.
**Clocks analysed:** 22.118 MHz (the original setting) and 33 MHz.

This report explains *what* was changed, *why*, and the general technique behind each
change, so it can be reused in other firmware. Every number in it was measured in the
cycle-counted simulator that is part of this package (`sim/`), not estimated by hand,
unless the text says otherwise.

---

## Contents

1. [What is in the package](#1-what-is-in-the-package)
2. [How the work was done](#2-how-the-work-was-done)
3. [How the original driver works](#3-how-the-original-driver-works)
4. [Bugs found, and the lesson from each](#4-bugs-found-and-the-lesson-from-each)
5. [Optimization techniques used in the new driver](#5-optimization-techniques-used-in-the-new-driver)
6. [Timing analysis: where the correction constants come from](#6-timing-analysis-where-the-correction-constants-come-from)
7. [The 33 MHz analysis](#7-the-33-mhz-analysis)
8. [Header file review](#8-header-file-review)
9. [Verification: the simulator and the test bench](#9-verification-the-simulator-and-the-test-bench)
10. [Mistakes I made along the way (and what they teach)](#10-mistakes-i-made-along-the-way-and-what-they-teach)
11. [Results summary](#11-results-summary)
12. [Hardware bring-up checklist](#12-hardware-bring-up-checklist)
13. [Glossary](#13-glossary)

---

## 1. What is in the package

| Path | What it is |
|---|---|
| `uart_drv_orig.asm` | Your driver exactly as uploaded |
| `uart_drv.asm` | The fixed and optimized driver. Same entry points, same echo `main` |
| `stc15w-sdcc.inc` | Your SFR header (unchanged; its defects are listed in section 8) |
| `README.md` | Short summary and result tables |
| `REPORT.md` | This document |
| `sim/sim8051.py` | Cycle-counted 8051 simulator with an STC15 peripheral model |
| `sim/tb.py` | Test bench: generates RX waveforms, decodes TX, checks the echo |
| `listings/*.lst` | Assembler listings (addresses, machine code, cycle column) for both drivers at 22.118 and 33 MHz |
| `listings/*.ihx` | Intel-HEX images, ready to flash with STC-ISP |

To reproduce everything:

```
sudo apt install sdcc                          # provides sdas8051 and sdld
cd sim
python3 tb.py ../uart_drv.asm ../uart_drv_orig.asm            # 22.1184 MHz, 19200
python3 tb.py ../uart_drv.asm --fosc 33000000                 # 33 MHz
python3 tb.py ../uart_drv.asm --fosc 33000000 --baud 57600    # any combination
```

---

## 2. How the work was done

The approach had four steps. The order matters and is itself a useful technique.

1. **Assemble the original before changing anything.** This proves the toolchain works and
   gives a listing file (`.lst`). The listing shows the *actual bytes* the assembler
   produced. Reading it is how the first bug was found: the value `0xAD` in a jump table where
   something around `0x60` was expected.
2. **Build a way to measure.** A UART is a timing problem. "It works on my bench" says
   little about the worst case. I wrote a simulator that executes the real machine code, counts
   clock cycles with STC15 instruction timings, and models Timer0, Timer2, INT4 and interrupt
   priorities. This makes worst cases repeatable: two interrupts colliding, a sender with a
   2% fast clock, a glitch, a BREAK.
3. **Test the original first.** Before writing new code I measured the original. That gives a
   baseline and confirms that the tests can detect real bugs. A test that passes everything
   has not shown that it can find anything.
4. **Change, then re-measure.** Every claim in this report ("sample point is centred",
   "survives a BREAK") is a test result, and the test bench reproduces all of them.

> **Technique:** separate *finding* bugs (reading code, reading listings) from *proving* fixes
> (automated tests). Reading finds bugs you already suspect. Tests find the ones you don't.

---

## 3. How the original driver works

A UART frame at 19200 baud is 10 bit cells of 52.08 µs each:

```
 idle  start  b0  b1  b2  b3  b4  b5  b6  b7  stop  idle
 ‾‾‾‾‾|_____|‾‾‾|___|‾‾‾|...                |‾‾‾‾‾‾‾‾‾‾‾‾
       ^ falling edge = frame starts
```

The line idles high (MARK). A frame starts with a low start bit (SPACE), then 8 data bits
LSB first, then a high stop bit. The receiver must detect the falling edge and then read
the line **in the middle of each bit cell**, where it is most stable.

The original driver is organised like this:

* **Receiving.**
  - INT4 (falling edge on P3.0) detects the start bit.
  - The INT4 handler falls through into the Timer0 handler with state 0.
  - State 0 starts Timer0 with 1.5 bit times, so the next interrupt is in the middle of b0,
    then reloads 1 bit time.
  - States 1–8 shift one bit each into `RXBIN`. State 9 checks the stop bit. State 10 waits
    out a BREAK.
  - The state is dispatched with a **jump table** (`MOVC A,@A+DPTR` followed by `JMP @A+DPTR`).
* **Transmitting.**
  - `TX_TRANSMIT` starts Timer2.
  - Each Timer2 interrupt runs the next state from a second jump table: start bit, b0..b7, stop bit.
* **Main loop.**
  - `RX_BLIND` enables INT4, waits for a byte, then disables INT4.
  - `TX_TRANSMIT` echoes the byte.

This is a sound, classic design, and it did work for isolated bytes (44 of 44 in the
gapped test). The problems appear with errors, with continuous traffic, and with noise.

---

## 4. Bugs found, and the lesson from each

### Bug 1: Wrong table base: crash on any framing error or BREAK

```asm
SWRX_STATE_TABLE:
	.DB		SWRX_S0 - SWRX_STATE_TABLE
	...
	.DB		SWRX_S9 - SWRX_STATE_TABLE
	.DB		SWRX_S10 - SWTX_STATE_TABLE		; <-- TX table used as base
```

The dispatcher computes `target = SWRX_STATE_TABLE + table[state]`, so every entry must be
relative to `SWRX_STATE_TABLE`. The last entry is relative to the *TX* table. The listing shows
what the assembler produced:

```
0001B5 AD     .DB  SWRX_S10 - SWTX_STATE_TABLE
```

`0x1AB + 0xAD = 0x258`. That is not `SWRX_S10` (0x20B). It is the second byte of `POP B`
inside the Timer2 handler. The CPU executes `F0` (`MOVX @DPTR,A`, a write to external RAM that
this chip does not have), then `POP ACC`, `POP PSW`, `RETI`. The Timer0 handler pushed 6
bytes, but only 2 are popped, so `RETI` jumps to an address made of saved register values.
The program crashes.

**When it triggers:** state 10 is reached after any frame whose stop bit reads 0. That covers a
line error, a BREAK, or the receiver syncing on a data edge after missing a start bit. The
simulator showed it in 9 of 15 tests.

**Lessons**

* The assembler cannot catch this. `SWRX_S10 - SWTX_STATE_TABLE` is a legal expression, and it
  happens to fit in a byte.
* **Read the listing file.** A jump-table entry that is much larger than its neighbours
  (`0x0B, 0x37, …, 0x42, 0xAD`) stands out immediately.
* **Remove the need for the table** where possible (section 5.1). With no table, this whole
  class of error cannot happen.
* **Test the error paths.** This path only runs on a line error, and normal clean traffic
  never exercises it.

### Bug 2: Continuous traffic loses bytes

This bug has two parts that combine.

**(a) The transmitter wastes one bit time per byte.** `TX_TRANSMIT` only *starts* Timer2. The
start bit is driven by the *first Timer2 interrupt*, one full bit time later. It releases
`STXBSY` at the *beginning* of the stop bit (state S9). The byte therefore occupies the
transmitter for 10 bit times plus interrupt latency (about 50 cycles).

**(b) Reception is switched off while waiting to transmit.** `RX_BLIND` disables INT4 before
returning. The main loop then spins in `JC (.-2)` until the transmitter is free, and only after
that does `RX_BLIND` re-enable INT4.

The next start bit arrives half a bit after the stop-bit sample. When the sender runs at
exactly your baud rate, each echoed byte costs about 50 cycles more than a received byte. That
debt grows by 50 cycles per byte. After about 12 bytes the spin exceeds half a bit, so a start
edge arrives while INT4 is disabled and is missed. The receiver then synchronises on a data
edge in the middle of the frame, which produces garbage and a framing error (and Bug 1).

Measured with only Bug 1 fixed: **5 of 64** back-to-back bytes echoed correctly at matched
baud. With a sender 2% *slower* than you it worked, because the slack absorbs the debt. That
explains why it can look fine on a bench.

**Lessons**

* **Never let a slow consumer disable a producer.** Receive and transmit are independent
  hardware functions and should be independent in software. The receiver should always be armed
  and put bytes in a buffer. The main loop takes them out when it can.
* **Throughput bugs need sustained tests.** One byte at a time never shows a debt that grows by
  a tiny amount per byte.

### Bug 3: No real noise rejection

The start bit is re-checked with `JB SW_RX` about 50–80 cycles after the edge, which is 4–7% into
the bit. Any glitch longer than that is accepted as a start bit. The simulator injected a
0.15-bit (about 170 cycles) glitch on the idle line. The original received a phantom byte and
the next three bytes came out corrupted (`0x0B, 0x27, 0x37, 0xFF` instead of `0x41, 0x42, 0x43`).

**Fix and lesson:** check the start bit **in its middle**, exactly like a hardware UART does. A
glitch shorter than half a bit has ended by then, and the receiver quietly returns to idle.

### Bug 4: `SWTI` means two different things

`SWTX_S0` sets `SWTI` at the start bit ("TX buffer full") and `SWTX_S9` sets it again at the end
("transmit complete"). `TX_TRANSMIT` clears it. Code that polls `SWTI` for "done" sees it set
almost immediately.

**Lesson:** give each flag exactly one meaning, write the meaning next to its definition, and
make every writer respect it.

### Smaller issues

| Issue | Why it matters |
|---|---|
| `BAUD_RATE = (22118000/19200)` truncates to 1151 | Integer division rounds down. Use `(F + BAUD/2) / BAUD` to round to nearest. Here the error is small (0.09%), but at higher baud rates it grows |
| `UART_INIT` loads Timer2 with `RXBAUD` | Harmless because RX and TX values are equal, but it would bite when they differ |
| Comment "CLR TR0 … clears any pending interrupt" | It doesn't. `TF0` stays set. The new code clears `TF0` explicitly where needed |
| `MOV TMOD,#0x00` | Also reconfigures Timer1. Use `ANL TMOD,#0xF0` to touch only Timer0's half |
| `SETB EA` and `ORL AUXR,#T0x12` inside the ISR | Already set; costs cycles in time-critical code |
| `PUSH B`, `PUSH 0x00`, `PUSH DPH/DPL` in the Timer0 handler | B and R0 are never used, and DPTR is only needed because of the jump table. That is 4 pushes and 4 pops, about 20 cycles per interrupt |

---

## 5. Optimization techniques used in the new driver

Each subsection shows the technique, the code, and why it helps. Cycle counts use the STC15
instruction table (for example `PUSH` 3, `POP` 2, `MOV A,dir` 2, `JB` 5, `RETI` 4).

### 5.1 Replace the jump table with a counter

**Before** (every Timer0 interrupt):

```asm
PUSH PSW / ACC / B / 0x00 / DPH / DPL    ; 6 x 3 = 18 cycles
MOV  A, SURXST                           ; 2
MOV  DPTR, #SWRX_STATE_TABLE             ; 3
MOVC A, @A+DPTR                          ; 5
JMP  @A+DPTR                             ; 5
...  state code ...
AJMP TIMER0_ISR_RXEND                    ; 3
POP  x 6                                 ; 12
RETI                                     ; 4
```

**After:**

```asm
_TIMER0_ISR:
	PUSH  PSW
	MOV   C, SW_RX          ; sample the line
	PUSH  ACC
	MOV   A, SURXST         ; 9 = start check, 8..1 = data bits, 0 = stop bit
	JZ    SWRX_STOP
	DEC   SURXST
	XRL   A, #9
	JZ    SWRX_START
	MOV   A, RXBIN          ; data bit: shift it in
	RRC   A
	MOV   RXBIN, A
TIMER0_ISR_RXEND:
	POP   ACC
	POP   PSW
	RETI
```

**Why it works:** eight of the ten states (the data bits) are *identical*. A table with
ten entries, eight of them pointing at the same code, is really just a counter. The counter
needs only two comparisons (`JZ`, `XRL #9 / JZ`) and no DPTR, so there is nothing to save
except PSW and ACC.

**Result:** Timer0 handler went from 72–91 cycles to 44–66 cycles (entry included).

> **Technique:** state machines where most states do the same thing should be a *counter plus
> special cases*, not a table. Use tables when states really differ.

### 5.2 Do the time-critical instruction first

```asm
_TIMER0_ISR:
	PUSH  PSW               ; C is about to be used, so PSW must be saved first
	MOV   C, SW_RX          ; <- the sample: always the 2nd instruction
```

In the original, the sample (`MOV C,SW_RX`) came after the pushes and the table dispatch, and
the start/stop states read the pin at different points. That made the sampling instant depend
on the state.

Here the pin is read at a **fixed, state-independent** number of cycles after the timer
overflow. A constant delay can be corrected once with a timer preload (section 6). A variable
delay cannot.

The TX handler uses the same idea for its output edge:

```asm
_TIMER2_ISR:
	PUSH  PSW
	MOV   C, TXBIN0         ; bit prepared by the previous interrupt
	MOV   SW_TX, C          ; <- edge: always at the same delay after overflow
	PUSH  ACC
	...                     ; now compute the bit for the *next* interrupt
```

> **Technique:** in a timing ISR, put the I/O first and the bookkeeping after it. When the
> output depends on a computation, do the computation one period *ahead* and store the result
> where a single instruction can emit it.

### 5.3 Instructions that do not touch the flags

The sampled bit is held in the carry flag `C` while the handler decides what to do with it. So
the decision code must not change `C`. On the 8051:

| Changes C | Leaves C alone |
|---|---|
| `CJNE`, `ADD`, `SUBB`, `RRC`, `RLC`, `CLR C`… | `MOV`, `INC`, `DEC`, `XRL`, `ANL`/`ORL` (byte forms), `JZ`, `JNZ`, `DJNZ` |

That is why the code uses `XRL A,#9` + `JZ` instead of `CJNE A,#9,…`. `CJNE` would destroy
the sample.

The same knowledge makes the **INT4 handler need no context saving at all**:

```asm
_INT4_ISR:
	MOV   TL0, #(RXBAUD_HLF & 0xFF)
	MOV   TH0, #(RXBAUD_HLF >> 8)
	SETB  TR0
	MOV   TL0, #(RXBAUD & 0xFF)
	MOV   TH0, #(RXBAUD >> 8)
	CLR   TF0
	ANL   AUXR2, #(~EX4)
	MOV   SURXST, #9
	SETB  SRXBSY
	RETI
```

None of these instructions uses A, and none changes PSW flags (`ANL direct,#data` does not
change C). So no `PUSH`/`POP` is needed, and it is 41 cycles instead of 111. It also makes the
edge-to-timer-start delay short and constant.

> **Technique:** know which instructions change which flags. It lets you skip saving
> registers you don't actually disturb.

### 5.4 Validate the start bit in its middle

INT4 starts Timer0 with **half** a bit time, so the first Timer0 interrupt lands in the middle
of the start bit. That interrupt checks that the line is still low:

```asm
SWRX_START:
	JNC   TIMER0_ISR_RXEND     ; still SPACE: genuine start bit, continue
	CLR   TR0                  ; line went back high: it was a glitch
	CLR   SRXBSY
	ORL   AUXR2, #EX4          ; wait for the next edge
	SJMP  TIMER0_ISR_RXEND
```

Every following interrupt comes exactly one bit later, which is the middle of b0, b1, and so on.
The cost is one extra, short interrupt per byte. The benefit is that any disturbance shorter than
half a bit is rejected.

### 5.5 Framing error and BREAK without an extra state

The original needed state 10 to poll the line every bit time until a BREAK ended. The new
driver handles everything at the stop-bit sample:

```asm
SWRX_STOP:
	CLR   TR0                  ; frame done
	ORL   AUXR2, #EX4          ; re-arm start detection
	CLR   SRXBSY
	JC    SWRX_PUT             ; stop bit = 1: good frame
	SETB  SWFE                 ; stop bit = 0: framing error
	MOV   A, RXBIN
	JNZ   SWRX_PUT
	SETB  SWBRK                ; all zeros and no stop bit: BREAK
```

**Why it is safe to re-arm INT4 while the line is still low:** INT4 is *edge*-triggered. If the
line is held low by a BREAK, there is no falling edge until it has first returned high. The
hardware therefore "waits out" the BREAK for free.

> **Technique:** before adding polling code, check whether the hardware already implements the
> waiting you need. Edge-triggered interrupts are natural "wait for the next change" detectors.

### 5.6 Transmit: shift register with a MARK sentinel

```asm
	MOV   A, TXBIN
	SETB  C                   ; a 1 (MARK) enters at the top
	RRC   A                   ; bit 0 falls out, everything moves right
	MOV   TXBIN, A
```

Each shift moves the next data bit into position 0 and pushes a `1` in at the top. After the 8
data bits, the register is `0xFF`, so the 9th output is automatically the stop bit (`1`). The
same instructions produce data and stop bits, with no special case.

`TXBIN` was moved to address 0x21, which is in the bit-addressable area (0x20–0x2F). Its bit 0
(bit address 0x08) can then be read with the single-bit instruction `MOV C,TXBIN0`. That is
what makes "output first" (5.2) possible with only 2 instructions.

A down counter (`DJNZ SUTXST`) ends the frame after the stop bit has lasted a full bit time.
`STXBSY` is released only then. This fixes the original's release at the *start* of the stop
bit.

### 5.7 Start the start bit immediately

```asm
	MOV   T2L, #(TXBAUD_1ST & 0xFF)   ; Timer2 stopped: loads counter AND reload
	MOV   T2H, #(TXBAUD_1ST >> 8)
	CLR   EA                          ; begin critical section
	ORL   AUXR, #T2R                  ; timer starts...
	CLR   SW_TX                       ; ...and the start bit begins, 3 cycles later
	SETB  EA                          ; end critical section
	MOV   T2L, #(TXBAUD & 0xFF)       ; Timer2 running: loads ONLY the reload register
	MOV   T2H, #(TXBAUD >> 8)
```

Three techniques are combined here:

1. **The start bit is driven by the caller**, not by the first interrupt. That removes the
   wasted bit time (Bug 2a), so each byte takes exactly 10 bit times.
2. **STC15 reload-register semantics.** On STC15 16-bit auto-reload timers, writing `TLx/THx`
   while the timer is *stopped* sets both the counter and the hidden reload register. Writing
   while it is *running* sets only the reload register. So the first period (start bit,
   shortened by `TX_CORR` to compensate for ISR latency) and all later periods can be set up
   before the first interrupt. Your original already used this trick for Timer0. It is used here
   for both timers.
3. **A tiny critical section.** Interrupts are disabled for 2 instructions so that nothing can
   come between "timer starts" and "start bit begins". Otherwise a Timer0 interrupt could land
   there and shorten the start bit by up to 66 cycles. The cost is that RX sampling may be
   delayed by at most about 6 cycles.

> **Technique:** keep critical sections (`CLR EA … SETB EA`) as short as possible, and only
> around instructions whose *relative* timing matters.

### 5.8 Lock-free ring buffer (single producer, single consumer)

```
RXBUFFER = 0x40..0x4F (16 bytes, 16-byte aligned)
RXHEAD   = written only by the Timer0 ISR   (kept in bank-1 R0)
RXTAIL   = written only by RX_POLL           (main program)
empty:  head == tail
full:   (head + 1) wrapped == tail           -> 15 usable bytes
```

Rules that make it safe **without disabling interrupts**:

1. **Each pointer has exactly one writer.** The ISR only writes the head, and the main program
   only writes the tail. Each is a single byte, so a write cannot be half-done.
2. **Write the data, then publish it.** The ISR stores the byte (`MOV @R0,RXBIN`) *before*
   advancing the head (`MOV R0,A`). The reader can never see a head that points past unwritten
   data.
3. **Read the data, then release the slot.** `RX_POLL` reads the byte into A *before* storing
   the new tail (`POP RXTAIL`). The ISR can never overwrite a byte that is being read.
4. **Power-of-two wrap with `ANL`.** Because the buffer is 16 bytes at a 16-byte boundary,
   `(ptr + 1) AND 0x4F` wraps 0x4F → 0x50 → 0x40 in one instruction, with no compare and branch.
5. **When full, drop the new byte and set a flag (`SWBFF`)** instead of overwriting old
   data. Losing a byte is then visible to the application, not silent.

The flag `SWRI` ("data available") uses the same ordering idea. `RX_POLL` clears it
*first* and then checks the buffer. If a byte arrives after the clear, the ISR sets it again,
so no arrival is ever lost.

> **Technique:** most ISR/main data sharing can be made safe by *ordering* rather than by
> locking. One writer per variable; write data before the index that publishes it.

### 5.9 A private register bank for the ISR

The ISR needs an indirect pointer (`@R0`) for the buffer. Instead of saving and restoring R0,
it switches to register bank 1, whose R0 is permanently the head pointer:

```asm
	MOV   PSW, #0x08          ; bank 1 (PSW was pushed on entry and is restored on exit)
	MOV   A, R0               ; R0 of bank 1 = RXHEAD, lives at IRAM 0x08
```

The pointer then stays in a register between interrupts, with no load or store. The switch
happens only after `C` (the sample) has been used, because writing PSW overwrites C.

### 5.10 Interrupt priority

On STC15, Timer2 and INT4 have a fixed *low* priority, and Timer0 can be set to *high*
(`ORL IP,#PT0`). RX sampling is given high priority because a late sample is directly a timing
error. The consequence, measured in section 7, is that a TX edge can be delayed while an RX
handler runs. That is why every handler is kept short.

---

## 6. Timing analysis: where the correction constants come from

Interrupts are not instantaneous. Between the event (edge or timer overflow) and the
instruction that acts on it, the CPU must:

1. finish the instruction in progress (0–5 cycles; call this *w*),
2. push the return address and vector (modelled as 6 cycles),
3. execute the `LJMP` at the vector (4 cycles),
4. execute everything before the critical instruction.

### RX: `THALF_CORR = 32`

```
edge ──(w + 6 entry + 4 LJMP + 3 MOV TL0 + 3 MOV TH0)──> Timer0 starts          = w  + 16
overflow ──(w' + 6 entry + 4 LJMP + 3 PUSH PSW + 1)───> pin sampled             = w' + 14
```

For the first sample to fall exactly at half a bit, the half-bit period must be shortened
by `16 + 14 + w + w'` ≈ 30 + average waiting ≈ **32 cycles**.

All later samples come exactly one timer period apart. Their latency is identical each
time, so it cancels out, and `RX_CORR` stays 0.

Measured result: samples fall at **0.498–0.503 of the bit** (the ideal is 0.500). The original
measured 0.544–0.556.

### TX: `TX_CORR = 13`

```
timer starts → CLR SW_TX done:          6 cycles   (start bit begins)
overflow → MOV SW_TX,C done:            w + 6 + 4 + 3 + 2 + 3 = w + 18
start bit length = period + 18 + w − 6  = period + 12 + w
```

To make the start bit exactly one bit long, the first period is shortened by about **13**
cycles. After that, every edge has the same 18-cycle delay, so the remaining bits are exact.
Measured worst edge error on isolated frames: **3 cycles (0.3%)**, against 12 cycles (1.0%)
in the original.

### How much clock mismatch can a UART receiver take?

The receiver re-synchronises on every start edge, then free-runs. If the two clocks differ by
*e*, the sampling point drifts by *e* per bit. The last sample must stay inside its bit:

```
max drift allowed = 0.5 bit (from the centre to the edge)
last sample (stop bit) is 9.5 bits after the edge
→ |e| < 0.5 / 9.5 ≈ 5.3 %   (minus any latency jitter)
```

The simulator measured **±5.5%** with idle gaps. It is slightly better than 5.3% because, with
a gap after the frame, a late stop-bit sample still reads idle (1). The binding limit is then b7
at 8.5 bits. In practice, budget about ±2% total between the two ends, which leaves margin for
jitter.

---

## 7. The 33 MHz analysis

### What changes

The only line to edit is `MPU_CLK_HZ .EQU 33000000`.

* **Divisor:** 33 000 000 / 19200 = 1718.75, rounded to **1719** (+0.015% error). Truncating
  would give 1718 (−0.044%). Either is negligible.
* **All reload values still fit in 16 bits.** The largest is 1719 cycles, far below 65536.
* **The correction constants do not change.** `THALF_CORR` and `TX_CORR` are in *clock
  cycles*, and the handlers take the same number of cycles at any clock. As a *fraction of a bit*
  they shrink, because a bit now has more cycles.

### Measured at 33 MHz (new driver)

| Baud | Cycles/bit | Tests | RX sample position | Worst TX edge (full duplex) |
|---|---|---|---|---|
| 19200 | 1718.75 | 17/17 | 0.498–0.503 | 42 cycles = 2.4% |
| 38400 | 859.4 | 17/17 | 0.492–0.504 | < 10% |
| 57600 | 572.9 | 16/17 | 0.494–0.506 | < 10% |
| 76800 | 429.7 | 15/17 | 0.495–0.518 | 15.9% |
| 115200 | 286.5 | 10/17 | 0.474–0.635 | 21.8% |

CPU time spent in interrupts while echoing at 19200: **3.8%** at 33 MHz, against 5.6% at 22.118 MHz.

### Why higher baud rates fail

Each handler has a fixed length: INT4 41 cycles, Timer0 44–66, Timer2 40–49 (entry included).
When two events happen at nearly the same time, one handler waits for the other. The waiting
time is fixed in cycles, while a bit gets shorter as the baud rate rises:

```
worst delay ≈ 66 cycles (longest handler, the RX stop bit)
keep delay ≤ 10 % of a bit   →   bit ≥ 660 cycles   →   baud ≤ 33 MHz / 660 ≈ 50 000
```

That matches the measurements. **38400 is fully clean. 57600 is usable** with normal clocks
(±2%): only the harshest test (sender 4% fast, back to back) fails, because re-arming INT4 at
the stop sample then comes too close to the next start edge. **115200 is beyond what this
software approach can do** on this core. For that, use the hardware UART.

### The original at 33 MHz

The original's bugs don't depend on the clock: 6/15 tests pass, and it crashes at 0x258 in the rest. Its
fixed correction `THALF_CORR = 51` also makes the sample point drift later as the baud rate
rises (0.53 at 19200, 0.60 at 57600, 0.70 at 115200). That is because a fixed number of
cycles becomes a larger part of a shorter bit.

### Caution about the clock itself

Your header's own comment gives **30.041 MHz** as the maximum. Confirm in the datasheet and in
STC-ISP that the internal RC oscillator can be trimmed to 33 MHz for your exact part and
supply voltage before using it.

---

## 8. Header file review

### The one that caused a bug: masks versus bit addresses

The 8051 has two kinds of names for bits:

| Kind | Example | Used with |
|---|---|---|
| **Bit address** (0x00–0xFF) | `TR0 .equ 0x88+4` | `SETB TR0`, `CLR TR0`, `JB TR0` |
| **Byte mask** | `PT0 .equ 0x02` | `ORL IP,#PT0`, `ANL IP,#(~PT0)` |

Your header defines the bits of bit-addressable SFRs (TCON, IE, P3, PSW…) as bit addresses.
It defines the bits of IP, IP2, IE2, AUXR, INT_CLKO… as **masks**.

I originally wrote `SETB PT0`. With your header this assembles to `SETB 0x02`. Bit address
0x02 is bit 2 of IRAM byte 0x20, which in this driver is `SWBRK`. The assembler gives no
warning, because it is a perfectly legal instruction. The result would be no high priority
for RX and a false BREAK flag at power-up. It is now `ORL IP,#PT0`.

> **Technique:** give masks and bit addresses visibly different names. For example, `PT0_M`
> for the mask and `PT0_B` for the bit. Or check the listing: `D2 02` for `SETB PT0` is
> obviously not in the SFR bit range (0x80–0xFF).

### Other defects found

| Line(s) | Problem | Effect |
|---|---|---|
| 63–70 | `GATE`, `M1`, `M0` defined twice (Timer1, then Timer0 values) | Last definition wins: the Timer1 masks are silently lost |
| 300, 366 | `PS2` defined as 0x01 (IP2) and 0x04 (WDT_CONTR) | `ORL IP2,#PS2` would set the wrong bit |
| 373–377 | `IAP_ADDRH` defined as both 0xC3 and 0xC4; `IAP_ADDRL` missing | Flash programming code would write the high address byte into the low register |
| 251–252 | `S3REN .equ 0x20`, same as `S3SM2` | Should be 0x10 |
| 249–256 | S3CON comments number bits b0..b7 from the top | Cosmetic, but misleading |
| 174 | `BRT` (0x9C) | Not an STC15 register (your comment already doubts it) |
| several | P1/P2/P3/P5 comments say "I/O port 0" | Cosmetic |

> **Technique:** assemblers generally allow `.equ` redefinition, and the last value wins. A
> quick check for duplicates is a script listing every name defined more than once. That is how
> these were found.

---

## 9. Verification: the simulator and the test bench

### Why a custom simulator

Existing 8051 simulators (for example SDCC's `s51`) model a classic 12-clock 8051. They do
not model the STC15 1T timings, Timer2 auto-reload with hidden reload registers, INT4, or its
priority rules, and those are exactly what determine whether this UART works. `sim/sim8051.py`
(about 490 lines) implements:

* **The full MCS-51 instruction set,** decoded from the real `.ihx` file the assembler produced.
  `MOVX` and illegal opcodes are treated as faults, because the chip has no external RAM.
* **STC15 cycle counts** per instruction.
* **Timer0 mode 0 and Timer2,** as 16-bit auto-reload timers with the STC15 "write while
  stopped/running" rule.
* **INT4** as a falling edge on P3.0, with its hidden request flag.
* **Two priority levels,** and the 8051 rule that one instruction always runs after `RETI`
  before the next interrupt.
* **A crash detector:** executing an address that is not the start of an instruction in the
  listing is reported as a fault. That is how the 0x258 crash is caught precisely.

### What the test bench checks

| Test | What it proves |
|---|---|
| Init state | No stray flags after `UART_INIT`. This check was added after the `PT0` bug |
| Echo with idle gaps | Basic correctness, including 0x00/0xFF/0x55/0xAA patterns |
| TX edge accuracy | Every TX transition is within 2% of the ideal bit boundary |
| RX sample position | Where the firmware *actually* reads the pin within each bit |
| Back-to-back at 0 / −2 / +2% | Sustained full-duplex traffic with clock mismatch |
| Full-duplex jitter | TX edge delay and RX sample spread while both directions are busy |
| Baud tolerance sweep | Largest clock mismatch that still works |
| 0.15-bit glitch | Noise rejection |
| Framing error, 25-bit BREAK | Error paths, and recovery to normal operation afterwards |
| INT4 flag latching both ways | Robustness to an uncertain hardware detail |
| Interrupt latency 4 and 9 cycles | Robustness to the uncertain entry-time assumption |
| 400 bytes with host 4% fast | Graceful overflow: bytes dropped are flagged (`SWBFF`), and the rest stay in order |

### Model assumptions to confirm on real hardware

* **Interrupt entry time** is modelled as 6 cycles. The tests also pass at 4 and 9, but the
  exact values of `THALF_CORR` and `TX_CORR` depend on it. Use `RX_PROBE` (section 12) to trim.
* **Timer2 reload behaviour** is assumed to be the same as Timer0 mode 0 (the datasheet
  describes it that way).
* **Whether INT4 remembers an edge while disabled** is not certain from the datasheet. Both
  behaviours were tested and both work.

> **Technique:** when you are not sure how hardware behaves, make the assumption a *parameter*
> and test both values. If the design works either way, the uncertainty stops mattering.

---

## 10. Mistakes I made along the way (and what they teach)

Being honest about these is part of the lesson. Each one is a common trap.

1. **My test bench measured TX jitter against the wrong clock.** In the ±2% tests I first
   compared the MCU's TX edges with the *host's* bit period. The 2% difference accumulated
   over the frame and reported a false 20% "jitter". The fix was to measure the MCU's output
   against the MCU's own bit period. *Lesson: when a test fails, verify the measurement before
   changing the design.*
2. **I misread an overrun result.** With the host 5% fast, bytes were lost but `SWBFF` was
   *not* set. That meant the loss was not buffer overflow at all. At 5%, the next start edge
   arrives just as INT4 is re-armed, which is a receiver limit (hardware UARTs have it too). The
   test now uses 4%, where overflow is really exercised and correctly flagged. *Lesson: a result
   that contradicts a flag is a clue, not noise.*
3. **The `SETB PT0` bug** (section 8) passed all tests with my placeholder header. It only
   showed up with your real header. *Lesson: test with the real include files, and add a test
   for the invariant ("no stray flags after init") so that the same class of bug can't hide
   again.*
4. **I first set an arbitrary 2% threshold for full-duplex TX jitter.** The measured value
   (5.7% at 22 MHz) is a real, explained property: Timer2 cannot be made high priority on
   STC15. The threshold was changed to a justified budget (10% of a bit), and the reason is
   written down. *Lesson: a pass/fail limit should come from a requirement, not from what you
   hope to see.*

---

## 11. Results summary

### 22.118 MHz, 19200 baud

| Check | Original | Original + table fix | New driver |
|---|---|---|---|
| Echo, idle gaps | pass | pass | pass |
| Back-to-back, host 0 / −2 / +2% | crash / crash / pass | 5/64, 2/64, pass | pass ×3 |
| 0.15-bit glitch | crash | garbage bytes | rejected |
| Framing error, BREAK | crash | pass | pass |
| 400 bytes, host 4% fast | crash | 109 lost, desync | 3 dropped with `SWBFF`, rest in order |
| RX sample position | 0.544–0.556 | same | 0.498–0.503 |
| Worst TX edge, isolated frames | 1.0% | 1.0% | 0.3% |
| Worst TX edge, full duplex | — | 7.4% (at +2%) | 5.7% |
| ISR length INT4 / T0 / T2 (cycles) | 111 / 72–91 / 74–77 | same | 41 / 44–66 / 40–49 |
| CPU load while echoing | 9.6% | 9.6% | 5.6% |
| Code size (main + driver) | 396 bytes | 396 bytes | 300 bytes |
| Tests passed* | 6/15 | 8/15 | **17/17** |

\* Totals differ because the full-duplex jitter checks only run when the echo in that scenario was correct.

### 33 MHz

See section 7. Summary: **17/17 at 19200 and 38400**, CPU load 3.8%, worst full-duplex TX edge
2.4%.

---

## 12. Hardware bring-up checklist

1. Program `listings/uart_drv_22118000_19200.ihx` (or the 33 MHz file) with STC-ISP, setting
   the IRC frequency to match `MPU_CLK_HZ`.
2. Connect a USB-serial adapter: its TX to P3.0 and its RX to P3.1, with a common ground.
3. Send single characters and check the echo. Then send a long file (for example 10 kB) and
   compare it byte for byte. This is the back-to-back test.
4. **Trim RX timing:** set `RX_PROBE .EQU 1`, rebuild, and put a scope on P3.0 and P3.2. P3.2
   toggles at every sampling instant. Each toggle must fall in the middle of its bit on P3.0.
   If they are systematically early or late by *n* cycles, change `THALF_CORR` by *n*
   (larger value = earlier sample).
5. **Trim TX timing:** scope P3.1 and measure the start bit. It should be 52.08 µs at 19200.
   Each cycle of `TX_CORR` is 45 ns at 22.118 MHz or 30 ns at 33 MHz.
6. Send a BREAK (most terminal programs can) and confirm the echo continues afterwards.

---

## 13. Glossary

| Term | Meaning |
|---|---|
| **MARK / SPACE** | Line idle-high (1) / low (0) |
| **8N1** | 8 data bits, no parity, 1 stop bit |
| **Framing error** | The stop bit read as 0: bad timing, noise, or a BREAK |
| **BREAK** | Line held low for longer than a whole frame |
| **1T core** | Instructions take 1–6 clock cycles, versus 12 per machine cycle on the classic 8051 |
| **Auto-reload** | The timer reloads its start value automatically on overflow, so the period has no software jitter |
| **Latency** | Time from an event to the instruction that reacts to it |
| **Jitter** | Variation in latency from one event to the next |
| **Critical section** | Code run with interrupts disabled so that no handler can come between its instructions |
| **Ring buffer** | Fixed array used circularly with a head (write) and a tail (read) index |
| **Bit-addressable area** | IRAM 0x20–0x2F and SFRs at addresses ending in 0 or 8; each bit has its own address for `SETB`/`CLR`/`JB`/`MOV C,bit` |
