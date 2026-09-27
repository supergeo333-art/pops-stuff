; ASSEMBLER CODE: INTEL 8051 device STC15W104
; SDCC SDAS8051 ASSEMBLER
; BIT_BANG UART     8 bit data, no-parity, 1 stop bit, 19200 baud, Interrupt Driven, full duplex
; INT4   Start-bit edge detect (P3.0)
; Timer 0 Data Reception   (high priority)
; Timer 2 Data Transmission
;
; Optimized / verified revision. Changes against the original driver:
;  - FIX: RX state table entry SWRX_S10 used SWTX_STATE_TABLE as base, a framing
;    error or BREAK jumped into the middle of the Timer2 ISR (crash).
;  - RX and TX are fully independent: INT4 stays armed while TX runs, so
;    back-to-back frames are never lost (RX_OPENBUFF once, then RX_BLIND/RX_POLL).
;  - 16 byte RX ring buffer (RXBUFFER) with overflow flag SWBFF.
;  - Start bit is validated at mid-bit (hardware-UART style noise rejection).
;  - Framing error / BREAK handled at the stop-bit sample, no extra polling state:
;    INT4 is falling-edge, so re-arming it with the line held low is harmless.
;  - Jump tables replaced by a down counter: shorter ISRs, no DPTR/B/R0 saves.
;  - Line is sampled (RX) / driven (TX) as the first instruction after PUSH PSW,
;    so the timing is fixed and independent of the state being processed.
;  - TX: start bit is driven by TX_TRANSMIT itself (no idle bit time before the
;    frame) and STXBSY is released only after the full stop bit: 10 bit times per
;    byte, back-to-back.
;  - SWTI now means "transmission complete" (it was also set at the start bit).
;  - Baud divisor is rounded instead of truncated.

.module bb_uart_drv

.optsdcc -mmcs51 --model-small -iram-size 128 --xram-size 0 --code-size 4088

.include "stc15w-sdcc.inc"			; STC15W104 Processor header file

;-----------------------------------------------------------------------
; Software UART
;-----------------------------------------------------------------------
MPU_CLK_HZ	.EQU		22118000			; 22.118MHz, 30.041 MHz Max
CLKDV		.EQU		1					; system clock No divide
CLKDV12		.EQU		12					; system clock divide 12

SW_RX		.EQU		P3.0				; MCU_RxD(P3.0), SW UART RX GPIO pin (must be /INT4)
SW_TX		.EQU		P3.1				; MCU_TxD(P3.1), SW UART TX GPIO pin (can be any GPIO pin)
RX_PROBE	.EQU		0					; 1: toggle P3.2 at every RX sample instant (scope trim)
PROBE_PIN	.EQU		P3.2
;
BAUD		.EQU		19200				; baud rate
BAUD_RATE	.EQU		(((MPU_CLK_HZ/CLKDV)+(BAUD/2))/BAUD)	; cycles per bit, rounded (1152)
;
; Latency corrections, in system clocks. Values are from the cycle-counted model
; in sim/ (STC15 instruction timings, 6 clock interrupt entry). Trim on hardware
; with RX_PROBE=1: the P3.2 edges must sit in the middle of the RX bit cells.
TX_CORR		.EQU		13				; Timer2 overflow -> SW_TX edge, minus TX_TRANSMIT start-bit lead
RX_CORR		.EQU		0				; per-bit correction (should stay 0)
THALF_CORR	.EQU		32				; INT4 edge -> Timer0 start + Timer0 overflow -> SW_RX sample
;
; Timer0 is the Receive Clock, started on /INT4 runs asynchronously
; Timer2 is the Transmit Clock, started by TX_TRANSMIT
; Both timers are 16-bit auto-reload: written while stopped, TLx/THx load the counter
; and the reload register; written while running, only the reload register.
TXBAUD		.EQU		(65536-BAUD_RATE)				; TX bit period
TXBAUD_1ST	.EQU		(65536-(BAUD_RATE-TX_CORR))		; TX start bit period
RXBAUD		.EQU		(65536-(BAUD_RATE-RX_CORR))		; RX bit period
RXBAUD_HLF	.EQU		(65536-((BAUD_RATE/2)-THALF_CORR))	; edge -> middle of start bit
;
; SWSCON: Software Serial Port Control (Bit-Addressable) Register
SWSCON		.EQU		0x20				; Serial Port Control Register
; Bits
SWRI		.EQU		0x00			; Receive flag, set while RX buffer holds data (see RX_POLL)
SWTI		.EQU		0x01			; Transmit flag, set when the stop bit has been sent, cleared by TX_TRANSMIT
SWBRK		.EQU		0x02			; RX break signal (sticky, cleared by software)
SWBFF		.EQU		0x03			; RX buffer overflow, byte dropped (sticky, cleared by software)
SWREN		.EQU		0x04			; Serial reception enabled (RX_OPENBUFF / RX_CLOSEBUFF)
SRXBSY		.EQU		0x05			; Receiver Busy. "SWSM0"
STXBSY		.EQU		0x06			; Transmitter Busy. "SWSM1"
SWFE		.EQU		0x07			; Framing Error BIT (sticky, cleared by software)
;
; TX shift register, bit-addressable so the ISR can output bit 0 with MOV C,bit
TXBIN		.EQU		0x21			; TX BIT shift register
TXBIN0		.EQU		0x08			; bit address of TXBIN.0
;
; UART BYTE BUFFER
RXBIN		.EQU		0x30			; RX BIT accumulator
RXDATA		.EQU		0x31			; last byte returned by RX_POLL / RX_BLIND
TXDATA		.EQU		0x33			; TX Data Buffer
;
; UART STATE MACHINE
SURXST		.EQU		0x34			; receiver bit counter:   9 start, 8..1 data, 0 stop
SUTXST		.EQU		0x35			; transmitter bit counter: 10..1
;
; UART RX RING BUFFER
RXBUFFER	.EQU		0x40			; 0x40 - 0x4F Receive Buffer (16 byte aligned)
RXBUFEND	.EQU		(RXBUFFER+0x0F)	; buffer end, also the wrap mask (15 bytes usable)
RXTAIL		.EQU		0x36			; read pointer, owned by RX_POLL
RXHEAD		.EQU		0x08			; write pointer = R0 of bank 1, owned by the RX ISR
;

;=======================================================
; Interrupt Vectors Table (ABS addresses 0x0000-0x002B)
;=======================================================
.area VECTOR    (CODE)
.area INTV (ABS)				; allow .org defines

		;; --- Rest Vector  ---
		.ORG    0x0000
		LJMP    _START

		;; --- Externa 0 Interrupt (INT0) ---
		.area INT0 (ABS,CODE)
		.ORG    0x0003
		RETI

		;; --- Timer 0 Interrupt (TF0) ---
		.area TMR0 (ABS,CODE)
		.ORG    0x000B
		LJMP	_TIMER0_ISR

		;; --- Externa 1 Interrupt (INT1) ---
		.area INT1 (ABS,CODE)
		.ORG    0x0013
		RETI

		;; --- Externa 2 Interrupt (INT2) ---
		.area INT2 (ABS,CODE)
		.ORG    0x0053
		RETI

		;; --- Externa 3 Interrupt (INT3) ---
		.area INT3 (ABS,CODE)
		.ORG    0x005B
		RETI

		;; --- Timer 2 Interrupt (TF2) ---
		.area TMR2 (ABS,CODE)
		.ORG    0x0063      ; Timer 2
		LJMP   _TIMER2_ISR

		;; --- Externa 4 Interrupt (INT4) ---
		.area INT4 (ABS,CODE)
		.ORG    0x0083
		LJMP   _INT4_ISR


; ============================================================
;				MAIN PROGRAM CODE
; ============================================================
.AREA CSEG (ABS,CODE) ; ABSOLUTE ADDRESS, debugging
.ORG 0X0100

_START:

			; INITIALIZE STACK POINTER
			MOV SP, 	#0x60           ; set stack pointer STACK TEST, 32 byte stack
			ANL	PSW,	#(~0x18)		; Bank zero(0)

			ACALL	UART_INIT			; bit-bang uart configuration
			ACALL	RX_OPENBUFF			; receiver runs from now on, in the background

_START00:

			ACALL	RX_BLIND			; call is blocking, A = data

			; echo back data
			MOV	TXDATA,	A
TX_RETRY:
			ACALL	TX_TRANSMIT
			JC	TX_RETRY				; transmitter busy with previous byte, retry

			SJMP	_START00



;=======================================================================
;				UART BIT BANG DRIVER
;=======================================================================
; Resources:  Timer0 + INT4 (RX), Timer2 (TX), register bank 1 R0 (RX ISR),
;             IRAM 0x20-0x21, 0x30-0x36, 0x40-0x4F.
; Callers run in register bank 0.


;========================================================================
;						UART INITIALIZATION
;========================================================================
UART_INIT:
			; SW_RX		.EQU		P3.0				; MCU_RxD(P3.0), SW UART RX GPIO pin (must be /INT4)
			; SW_TX		.EQU		P3.1				; MCU_TxD(P3.1), SW UART TX GPIO pin (can be any GPIO pin)
			; P3M1[7 : 0]	P3M0 [7 : 0]
			; 		1			0			P3.0 input-only (high-impedance )
			;MOV	P3M1,	#0b00000001			;
			;MOV	P3M0,	#0b00000000			;

			SETB	SW_TX					; set TX output MARK
			MOV		SWSCON,	#0x00			; all flags clear, RX disabled, TX idle

			MOV		RXTAIL,	#RXBUFFER		; empty receive ring
			MOV		RXHEAD,	#RXBUFFER

			;--------------- Configure System Clock Source -------------
			ANL CLK_DIV,	#(~(CLKS2|CLKS1|CLKS0))			; set system clock divider/1

			;------------------ INT4 -----------------------------------
			ANL		AUXR2,	#(~EX4)			; disabled until RX_OPENBUFF

			;------------------ Configure Timer 0 (RX) ------------------
			CLR		TR0						; STOP
			ANL		TMOD,	#0xF0			; Timer0 mode 0 (16 bit auto-reload), timer, no gate
			ORL		AUXR,	#T0x12			; Timer 0 in 1T mode
			CLR		TF0
			SETB	PT0						; high priority: RX sampling preempts TX and INT4
			SETB	ET0						; only runs while a frame is being received

			;------------------ Configure Timer 2 (TX) -------------------
			ANL		AUXR,	#(~T2R)			; Timer2 stop
			ORL		AUXR,	#T2x12			; T2 in 1T mode, clock source SYSCLK/1
			ORL		IE2,	#ET2			; only runs while a frame is being sent

			SETB	EA						; Go Live!

			RET


;=======================================================================
; 						OPEN, ALLOW UART RECEPTION
; Received bytes are buffered, 15 bytes max, with overflow detection (SWBFF)
;=======================================================================
RX_OPENBUFF:
			CLR		SWFE					; clear framing error
			CLR		SWBRK					; clear line break
			CLR		SWBFF					; clear overflow
			SETB	SWREN					; RX enabled
			ORL		AUXR2,	#EX4			; arm start-bit detection
			RET

;=======================================================================
;				CLOSE, DISABLE UART RECEPTION
; Buffered bytes stay readable with RX_POLL / RX_BLIND.
;=======================================================================
RX_CLOSEBUFF:
			ANL		AUXR2,	#(~EX4)			; no new frames
			CLR		TR0						; abort frame in progress
			CLR		SRXBSY
			CLR		SWREN
			RET

;=======================================================================
; 						RX POLL, NON BLOCKING
; out:	C=1  buffer empty
;		C=0  A = RXDATA = received byte
; uses R0 (restored)
;=======================================================================
RX_POLL:
			CLR		SWRI					; ISR sets it again for a byte arriving from here on
			MOV		A,	RXTAIL
			CJNE	A,	RXHEAD,	RX_POLL1	; tail != head: data available
			SETB	C
			RET
RX_POLL1:
			PUSH	0x00					; R0 bank(0)
			MOV		R0,	A					; R0 -> oldest byte
			INC		A
			ANL		A,	#RXBUFEND			; wrap
			PUSH	ACC						; new tail
			MOV		A,	@R0					; data
			POP		RXTAIL					; release the slot only after reading it
			MOV		RXDATA,	A
			POP		0x00

			MOV		A,	RXTAIL				; more data left? keep SWRI set
			CJNE	A,	RXHEAD,	RX_POLL2
			SJMP	RX_POLL3
RX_POLL2:
			SETB	SWRI
RX_POLL3:
			MOV		A,	RXDATA
			CLR		C
			RET

;=======================================================================
; 						RX SIMPLE RECEIVE, BLOCKING
; out:	A = RXDATA = received byte
;		C = SWFE | SWBRK | SWBFF, error flags are sticky, clear at application level
;=======================================================================
RX_BLIND:
			ACALL	RX_POLL
			JC		RX_BLIND				; wait for data
			MOV		C,	SWFE				; indicate error state
			ORL		C,	SWBRK
			ORL		C,	SWBFF
			RET


;=======================================================================
;				UART RECEIVER INTERRUPT SERVICE INT4 & TIMER 0
;=======================================================================
; INT4: leading (falling) edge of the start bit. Starts Timer0 so that its first
; overflow falls in the middle of the start bit, every following one a bit time
; later. No registers or flags are touched, so nothing needs saving.
_INT4_ISR:
			MOV		TL0,	#(RXBAUD_HLF & 0xFF)	; (3) Timer0 stopped: counter and reload
			MOV		TH0,	#(RXBAUD_HLF >> 8)		; (3)
			SETB	TR0								; (3) running, first overflow at mid start bit
			MOV		TL0,	#(RXBAUD & 0xFF)		; running: reload register only,
			MOV		TH0,	#(RXBAUD >> 8)			; 1 bit time for the following overflows
			CLR		TF0
			ANL		AUXR2,	#(~EX4)			; ignore data edges until the stop bit
			MOV		SURXST,	#9				; next Timer0 overflow: start bit check
			SETB	SRXBSY
			RETI

;-----------------------------------------------------------------------------
; Timer0: one interrupt per bit cell, SURXST counts down
;	9     middle of start bit, abort if the line is MARK again (glitch)
;	8..1  data b0..b7
;	0     stop bit, deliver byte
;-----------------------------------------------------------------------------
_TIMER0_ISR:
			PUSH	PSW						; (3)
			MOV		C,	SW_RX				; (2) sample RX, fixed delay after the overflow
		.if RX_PROBE
			CPL		PROBE_PIN				; scope: marks the sampling instant
		.endif
			PUSH	ACC

			MOV		A,	SURXST
			JZ		SWRX_STOP				; stop bit
			DEC		SURXST
			XRL		A,	#9					; XRL/JZ leave the sample in C untouched
			JZ		SWRX_START				; start bit

			MOV		A,	RXBIN				; data bit: right shift Carry into shift register
			RRC		A
			MOV		RXBIN,	A

TIMER0_ISR_RXEND:
			POP		ACC
			POP		PSW
			RETI

SWRX_START:
			JNC		TIMER0_ISR_RXEND		; still SPACE: good start bit
			CLR		TR0						; false start (noise), back to idle
			CLR		SRXBSY
			ORL		AUXR2,	#EX4
			SJMP	TIMER0_ISR_RXEND

;------------------------------------------------------------------------------------
; STOP BIT, Framing Error  Detection if-then Break Detection
;------------------------------------------------------------------------------------
SWRX_STOP:
			CLR		TR0						; frame complete, Timer0 stop
			ORL		AUXR2,	#EX4			; re-arm start-bit detection (mid stop bit). If the
											; line is still SPACE (error/BREAK) the next falling
											; edge only comes after it returned to MARK
			CLR		SRXBSY
			JC		SWRX_PUT				; stop bit is MARK, valid frame
			SETB	SWFE					; flag Framing Error
			MOV		A,	RXBIN
			JNZ		SWRX_PUT
			SETB	SWBRK					; all zero + no stop bit: BREAK
SWRX_PUT:
			MOV		PSW,	#0x08			; bank 1, R0 = RXHEAD (PSW restored on exit)
			MOV		A,	R0
			INC		A
			ANL		A,	#RXBUFEND			; wrap
			CJNE	A,	RXTAIL,	SWRX_PUT1	; next head == tail: buffer full
			SETB	SWBFF					; overflow, byte dropped
			SJMP	TIMER0_ISR_RXEND
SWRX_PUT1:
			MOV		@R0,	RXBIN			; store byte
			MOV		R0,	A					; then publish it
			SETB	SWRI					; data available
			SJMP	TIMER0_ISR_RXEND


;=======================================================================
;  TX UART TRANSMISSION
;=======================================================================

;=======================================================================
; 						TRANSMIT ONE BYTE
; in:	TXDATA
; out:	C=1 transmitter busy, nothing done (retry)
;		C=0 transmission started, STXBSY set until the stop bit is complete
;=======================================================================
TX_TRANSMIT:

			SETB	C						; set error, cleared if routine runs OK
			JB		STXBSY,	TX_TRANSMIT_EXIT	; TX already busy, can not transmit

			SETB	STXBSY					; indicate TX busy
			CLR		SWTI					; transmission not complete
			MOV		TXBIN,	TXDATA			; byte into shift register
			MOV		SUTXST,	#10				; 8 data + stop + end of stop

			MOV		T2L,	#(TXBAUD_1ST & 0xFF)	; Timer2 stopped: counter and reload
			MOV		T2H,	#(TXBAUD_1ST >> 8)		; start bit period

			CLR		EA						; keep timer start and start bit edge together
			ORL		AUXR,	#T2R			; T2 start to run
			CLR		SW_TX					; START bit
			SETB	EA

			MOV		T2L,	#(TXBAUD & 0xFF)		; running: reload register only
			MOV		T2H,	#(TXBAUD >> 8)			; data/stop bit period

			CLR		C						; indicate success

TX_TRANSMIT_EXIT:

			RET

;=======================================================================
;  TIMER 2 INTERRUPT, TX UART TRANSMISSION
;=======================================================================
; SUTXST 10..3  output b0..b7
;        2      output stop bit (MARK was shifted in behind the data)
;        1      end of stop bit: stop Timer2, transmitter free
_TIMER2_ISR:
			PUSH	PSW						; (3)
			MOV		C,	TXBIN0				; (2) bit for this cell was prepared last time
			MOV		SW_TX,	C				; (3) output bit on GPIO pin, fixed delay
			PUSH	ACC

			MOV		A,	TXBIN				; prepare next bit
			SETB	C						; shift a STOP bit into MSB
			RRC		A
			MOV		TXBIN,	A

			DJNZ	SUTXST,	TIMER2_ISR_TXEND

			ANL		AUXR,	#(~T2R)			; T2 STOP! Stop bit sent.
			CLR		STXBSY					; transmitter available
			SETB	SWTI					; transmit complete

TIMER2_ISR_TXEND:
			POP		ACC
			POP		PSW
			RETI


;-----------------------------------------------------------------------
;						END BIT-BANG UART
;-----------------------------------------------------------------------


;=======================================================================
;					END OF PROGRAM
;=======================================================================

