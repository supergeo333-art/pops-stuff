; Minimal STC15W104 SFR definitions (subset used by the bit-bang UART driver)
; Standard 8051 SFRs/bits (P3, TMOD, TH0, TL0, TR0, TF0, ET0, EA, SP, PSW, ...)
; are built into sdas8051.
AUXR		.EQU	0x8E		; Auxiliary register
T0x12		.EQU	0x80		;   Timer0 1T mode
T1x12		.EQU	0x40		;   Timer1 1T mode
T2R			.EQU	0x10		;   Timer2 run
T2_CT		.EQU	0x08		;   Timer2 counter/timer select
T2x12		.EQU	0x04		;   Timer2 1T mode
AUXR2		.EQU	0x8F		; INT_CLKO (a.k.a. AUXR2)
INT_CLKO	.EQU	0x8F
EX4			.EQU	0x40		;   INT4 enable (falling edge only)
EX3			.EQU	0x20
EX2			.EQU	0x10
IE2			.EQU	0xAF		; Interrupt enable 2
ET2			.EQU	0x04		;   Timer2 interrupt enable
T2H			.EQU	0xD6
T2L			.EQU	0xD7
CLK_DIV		.EQU	0x97		; Clock divider
CLKS2		.EQU	0x04
CLKS1		.EQU	0x02
CLKS0		.EQU	0x01
P3M1		.EQU	0xB1
P3M0		.EQU	0xB2
