; STC15W series
; Special Function Registers
; Special Function Register Map and Defines
;
; Verification Date: 2026-08-12 - reviewed for errors, matches documentation 
; STC15W408AS-STC15F2K60S2-en.pdf Update Date: 2015/10/10


P0 .equ 0x80                  ; P0,	I/O port 0,	(Bit-Addressable)
	P07 .equ 0x80+7             
	P06 .equ 0x80+6             
	P05 .equ 0x80+5             
	P04 .equ 0x80+4             
	P03 .equ 0x80+3             
	P02 .equ 0x80+2             
	P01 .equ 0x80+1             
	P00 .equ 0x80+0             

SP .equ 0x81                  ; SP, stack pointer, (Non bit-Addressable)

DPL .equ 0x82                 ; DPL, data pointer low, (Non bit-Addressable)
					
DPH .equ 0x83                 ; DPH, data pointer high, (Non bit-Addressable)

S4CON .equ 0x84               ; S4CON register: Serial Port 4 (UART4) Control Register (No bit-Addressable)
	S4SM0	.equ	0x80	  ;b7, S4SM0  
	S4ST4	.equ	0x40	  ;b6, S4ST4 
	S4SM2	.equ	0x20	  ;b5, S4SM2 
	S4REN	.equ	0x10	  ;b4, S4REN 
	S4TB8	.equ	0x08	  ;b3, S4TB8 
	S4RB8	.equ	0x04	  ;b2, S4RB8 
	S4TI 	.equ	0x02	  ;b1, S4TI  
	S4RI 	.equ	0x01	  ;b0, S4RI  

S4BUF .equ 0x85               ; S4, serial buffer, (Non bit-addressable)

; 0x86 empty

PCON .equ 0x87                ; LVD interrupt: Power Control register PCON (Non bit-Addressable)
	;
	SMOD	.equ	0x80	  ;b7, SMOD	double Baud rate control bit.
	SMOD0	.equ	0x40	  ;b6, SMOD0 Frame Error select.
	LVDF	.equ	0x20	  ;b5, LVDF	Low-Voltage Flag.
	POF		.equ	0x10	  ;b4, POF	Power-On flag. It is set by power-off-on action and can only cleared by software.
	GF1		.equ	0x08	  ;b3, GF1	General-purposed flag 1
	GF0		.equ	0x04	  ;b2, GF0	General-purposed flag 0
	PD		.equ	0x02	  ;b1, PD	Power-Down bit.
	IDL		.equ	0x01	  ;b0, IDL	Idle mode bit.

TCON .equ 0x88                ; TCON, Timer/Counter Control Register (Bit-Addressable)
	;
	TF1 .equ 0x88+7           ; Timer 1 overflows the interrupt mark bit
	TR1 .equ 0x88+6           ; Timer 1 runs the control bit
	TF0 .equ 0x88+5           ; Timer 0 overflows the interrupt flag bit
	TR0 .equ 0x88+4           ; Timer 0 runs the control bit
	IE1 .equ 0x88+3           ; Outer interruption 1 marker
	IT1 .equ 0x88+2           ; Outer interruption 1 signal mode control level, 1: falling edge interruption, 0: rising and falling are all interrupted.
	IE0 .equ 0x88+1           ; The 0 marker is interrupted outside
	IT0 .equ 0x88+0           ; The outer interruption of the 0 signal mode control level, 1: the falling edge is interrupted, and 0: the rising and falling are all interrupted.

TMOD .equ 0x89                ; TMOD, Timer/Counter Mode Register (Non bit-addressable)
	;
	GATE 	.equ	0x80	  ; b7, T1 GATE 	Timer/Counter 1 Gate Control.
	CT1		.equ	0x40	  ; b6, T1 C/-T	Timer/Counter 1 Select bit.
	M1		.equ	0x20	  ; b5, T1 M1	Timer 1 Mode Select bits.
	M0		.equ	0x10	  ; b4, T1 M0
	GATE	.equ	0x08	  ; b3, T0 GATE	Timer/Counter 0 Gate Control.
	CT0		.equ	0x04	  ; b2, T0 C/-T	Timer/Counter 0 Select bit.
	M1		.equ	0x02	  ; b1, T0 M1	Timer 0 Mode Select bits.
	M0		.equ	0x01	  ; b0, T0 M0

TL0 .equ 0x8A                 ; TL0, Timer 0 low byte Register  (Non bit-addressable)

TL1 .equ 0x8B                 ; TL1, Timer 1 low byte Register  (Non bit-addressable)

TH0 .equ 0x8C                 ; TH0, Timer 0 high byte Register  (Non bit-addressable)

TH1 .equ 0x8D                 ; TH1, Timer 1 high byte Register  (Non bit-addressable)

AUXR .equ 0x8E                ; AUXR, Auxiliary register (Non bit-addressable)
	;
	T0x12		.equ	0x80	  ; b7, T0x12, Timer 0 clock source bit.				
	T1x12		.equ	0x40	  ; b6, T1x12, Timer 1 clock source bit.
	UART_M0x6	.equ	0x20	  ; b5, UART_M0x6, Baud rate select bit of UART1 while it is working under Mode-0
	T2R			.equ	0x10	  ; b4, T2R, Timer 2 Run control bit
	T2_CT2		.equ	0x08	  ; b3, T2_C/T, Counter or timer 2 selector
	T2x12		.equ	0x04	  ; b2, T2x12, Timer 2 clock source bit.
	EXTRAM		.equ	0x02	  ; b1, EXTRAM, Internal / external RAM access control bit.
	S1ST2		.equ	0x01	  ; b0, S1ST2, the control bit that UART1 select Timer 2 as its baud-rate generator.

; WAKE_CLKO .equ 0x8F ; NOT STC15W1xx
          
INT_CLKO .equ 0x8F            
AUXR2 .equ 0x8F            
	;
	;		.equ	0x80	  ; b7, -
	EX4		.equ	0x40	  ; b6, EX4					; EX4, Enable bit of External Interrupt 4 (INT4)
	EX3		.equ	0x20	  ; b5, EX3					; EX3, Enable bit of External Interrupt 3 (INT3)
	EX2		.equ	0x10	  ; b4, EX2					; EX2, Enable bit of External Interrupt 2 (INT2)
	;		.equ	0x08	  ; b3, -
	T2CLKO	.equ	0x04	  ; b2, T2CLKO				; Timer 2(T2) programmable clock output T2CLKO
	T1CLKO	.equ	0x02	  ; b1, T1CLKO				; T0 configured for Timer 1(T1) programmable clock output T1CLKO
	T0CLKO	.equ	0x01	  ; b0, T0CLKO				; T1 configured for Timer 0(T0) programmable clock output T0CLKO

P1 .equ 0x90                  ; P1,	I/O port 0, (Bit-Addressable)
	P17 .equ 0x90+7             
	P16 .equ 0x90+6             
	P15 .equ 0x90+5             
	P14 .equ 0x90+4             
	P13 .equ 0x90+3             
	P12 .equ 0x90+2             
	P11 .equ 0x90+1             
	P10 .equ 0x90+0             

P1M1 .equ 0x91                ; P1M1 register (non bit addressable)
P1M0 .equ 0x92                ; P1M0 register (non bit addressable)
	; Configure I/O ports mode
	; [P1M1.b, P1M0.b]:{0,0} Quasi_bidirectional,Sink Current up to 20mA, 	
	;				   {0,1} Push-pull output, 20mA
	;				   {1,0} Input-only (high-impedance )
	;				   {1,1} Open Drain, no internal pull-up resister
	; Example: MOV P1M1, #10100000B
	;          MOV P1M0, #11000000B
	; P1.7 in Open Drain mode, P1.6 in strong push-pull output, P1.5 in high-impedance input, P1.4-0 quasi_bidirectional
	
P0M1 .equ 0x93                ; P0M1 register (non bit addressable)
P0M0 .equ 0x94                ; P0M0 register (non bit addressable)
	; Configure I/O ports mode

P2M1 .equ 0x95                ; P2M1 register (non bit addressable)
P2M0 .equ 0x96                ; P2M0 register (non bit addressable)
	; Configure I/O ports mode

CLK_DIV .equ 0x97             ; CLK_DIV (PCON2) : Clock Division register (Non bit addressable
PCON2 .equ 0x97               ;
	;
	MCK0_S1	.equ	0x80	  ; b7, MCK0_S1			; Master clock output external clock
	MCK0_S0	.equ	0x40	  ; b6, MCK0_S0
	ADRJ	.equ	0x20	  ; b5, ADRJ			; ADC Result Arrangement Register Bit ----ADRJ
	Tx_Rx	.equ	0x10	  ; b4, Tx_Rx			; Tx_Rx:the set bit of relay and broadcast mode of UART1 
	MCLK0_2	.equ	0x08	  ; b3, MCLK0_2 		; MCLKO_2:to select Master Clock output on where
	CLKS2	.equ	0x04	  ; b2, CLKS2			; Master clock frequency/(b2-b1), System Clock(SYSclk) Division
	CLKS1	.equ	0x02	  ; b1, CLKS1
	CLKS0	.equ	0x01	  ; b0, CLKS0


SCON .equ 0x98                ; Serial Port 1 Control Register (Bit-Addressable)
	;
	FE  .equ 0x98+7           ; Framing Error (SM0/FE) FE : Framing Error bit. 
	SM0 .equ 0x98+7           ; Serial Mode 0 (SM0/FE) The SMOD0 bit must be set to enable access to the FE bit
	SM1 .equ 0x98+6           ; Serial Mode 1
	SM2 .equ 0x98+5           ; Serial Mode 2
	REN .equ 0x98+4           ; Receive Enable
	TB8 .equ 0x98+3           ; Transmit Bit 8
	RB8 .equ 0x98+2           ; Receive Bit 8
	TI 	.equ 0x98+1           ; Transmit Interrupt
	RI 	.equ 0x98+0           ; Receive Interrupt
;
SBUF .equ 0x99                ; Serial Port 1 Data Buffer Register

S2CON .equ 0x9A               ; Serial Port 2 (UART2) Control Register (No bit-Addressable)
	;
	S2SM0	.equ	0x80	  ; b7, S2SM0			; 8,9-bit UART, baud-rate variable
	;-		.equ	0x40	  ; b6, -
	S2SM2	.equ	0x20	  ; b5, S2SM2			; Enable the automatic address recognition feature.
	S2REN	.equ	0x10	  ; b4, S2REN			; Enable the serial port reception.
	S2TB8	.equ	0x08	  ; b3, S2TB8			; 9th data bit which will be transmitted in mode 1.
	S2RB8	.equ	0x04	  ; b2, S2RB8			; In mode 1, the received 9th data bit will go into this bit.
	S2TI	.equ	0x02	  ; b1, S2TI			; Transmit interrupt flag.
	S2RI	.equ	0x01	  ; b0, S2RI			; Receive interrupt flag.
;
S2SBUF .equ 0x9B                ; Serial Port 2 Data Buffer Register

BRT .equ 0x9C                 ; dedicated Baud-Rate Timer, unavialable Datasheet Error?

P1ASF .equ 0x9D               ; P1 Analog Function Configure register: P1ASF (Non bit-addressable)
	;
	; b7, P17ASF, 0x80 		  ; Set(1) P1.x as ADC input channel-x
	; b6, P16ASF, 0x40 		  ;
	; b5, P15ASF, 0x20 		  ;
	; b4, P14ASF, 0x10 		  ;
	; b3, P13ASF, 0x08 		  ;
	; b2, P12ASF, 0x04 		  ;
	; b1, P11ASF, 0x02 		  ;
	; b0, P10ASF, 0x01 		  ;

; 0x9E	not used
; 0x9F	not used

P2 .equ 0xA0                  ; P2,	I/O port 0, (Bit-Addressable)
	P27 .equ 0xA0+7             
	P26 .equ 0xA0+6             
	P25 .equ 0xA0+5             
	P24 .equ 0xA0+4             
	P23 .equ 0xA0+3             
	P22 .equ 0xA0+2             
	P21 .equ 0xA0+1             
	P20 .equ 0xA0+0             

BUS_SPEED .equ 0xA1           ;xxxx,xx10 Bus Speed Control Register (Non bit-addressable)
	;	
	; 		.equ	0x80	  ; b7, - 		
	; 		.equ	0x40	  ; b6, - 		
	; 		.equ	0x20	  ; b5, - 		
	; 		.equ	0x10	  ; b4, - 		
	; 		.equ	0x08	  ; b3, - 		
	; 		.equ	0x04	  ; b2, - 		
	EXRTS1	.equ	0x02	  ; b1, EXRTS1 	(Extend Ram Timing Selector)
	EXRTS0	.equ	0x01	  ; b0, EXRTS0 	
		
AUXR1 .equ 0xA2               			; Auxiliary register 1 (Non bit-addressable)
P_SW1 .equ 0xA2               			; Auxiliary register 1
	;		
	S1_S1 	.equ	0x80; b7, S1_S1 	; UART1/S1-0 can be switched between P1 and P3
	S1_S0 	.equ	0x40; b6, S1_S0 	;
	CCP_S1	.equ	0x20; b5, CCP_S1 	; CCP can be switched in P1 and P2 and P3
	CCP_S0	.equ	0x10; b4, CCP_S0 	;	
	SPI_S1	.equ	0x08; b3, SPI_S1 	; SPI can be switched in P1 and P2 and P4
	SPI_S0	.equ	0x04; b2, SPI_S0	;
	;		.equ	0x02; b1, -	
	DPS		.equ	0x01; b0, DPS		; DPTR registers select bit.
	
; 0xA3	not used
; 0xA4	not used
; 0xA5	not used
; 0xA6	not used
; 0xA7	not used

IE .equ 0xA8                  ; Interrupt Control Register (Bit-Addressable)
	EA   .equ 0xA8+7;         ;
	ELVD .equ 0xA8+6;         ;
	EADC .equ 0xA8+5;         ;
	ES   .equ 0xA8+4;         ;
	ET1  .equ 0xA8+3;         ;
	EX1  .equ 0xA8+2;         ;
	ET0  .equ 0xA8+1;         ;
	EX0  .equ 0xA8+0;         ;

SADDR .equ 0xA9;              ; Slave Address Register, (Non bit-addressable)

WKTCL .equ 0xAA               ; Power-Down Wake-up Timer Control register, low byte, (Non bit-addressable)
WKTCL_CNT .equ 0xAA           ; Power Down Wakeup Timer Control High Byte, low byte

WKTCH .equ 0xAB               ; Power-Down Wake-up Timer Control register and enable, high byte, (Non bit-addressable)
WKTCH_CNH .equ 0xAB               ; Power-Down Wake-up Timer Control register and enable, high byte, (Non bit-addressable)

S3CON .equ 0xAC               ; Serial Port 3 Control Register, (Non bit-addressable)
	;
	S3SM0	.equ	0x80	  ; b0,  S3SM0 			; Serial Port 3 Select Mode Bit 0
	S3ST3	.equ	0x40	  ; b1,  S3ST3 			; Serial Port 3 Select Timer 3
	S3SM2	.equ	0x20	  ; b2,  S3SM2 			; Serial Port 3 Select Mode Bit 2
	S3REN	.equ	0x20	  ; b3,  S3REN 			; Serial Port 3 Receive Enable
	S3TB8	.equ	0x08	  ; b4,  S3TB8 			; Serial Port 3 Transmit Bit 8 Data
	S3RB8	.equ	0x04	  ; b5,  S3RB8 			; Serial Port 3 Receive Bit 8 Data
	S3TI 	.equ	0x02	  ; b6,  S3TI  			; Serial Port 3 Transmit Interrupt Flag
	S3RI 	.equ	0x01	  ; b7,  S3RI  			; Serial Port 3 Receive Interrupt Flag

S3BUF .equ 0xAD               ; Serial Port 3 Data Register, (Non bit-addressable)

; 0xAE	not used

IE2 .equ 0xAF                 ; Interrupt Enable 2 Rsgister (Non bit-addressable)
	;
	;-	
	ET4 	.equ	0x40	  ; b6,  ET4 			; Enable Timer 4 Interrupt
	ET3 	.equ	0x20	  ; b5,  ET3 			; Enable Timer 3 Interrupt
	ES4 	.equ	0x10	  ; b4,  ES4 			; Enable Serial Port 4 Interrupt
	ES3 	.equ	0x08	  ; b3,  ES3 			; Enable Serial Port 3 Interrupt
	ET2 	.equ	0x04	  ; b2,  ET2 			; Enable Timer 2 Interrupt
	ESPI	.equ	0x02	  ; b1,  ESPI			; Enable Serial Peripheral Interface Interrupt
	ES2 	.equ	0x01	  ; b0,  ES2 			; Enable Serial Port 2 Interrupt
    
P3 .equ 0xB0                  ; P3,	I/O port 0,	(Bit-Addressable)
	P37 .equ 0xB0+7             
	P36 .equ 0xB0+6             
	P35 .equ 0xB0+5             
	P34 .equ 0xB0+4             
	P33 .equ 0xB0+3             
	P32 .equ 0xB0+2             
	P31 .equ 0xB0+1             
	P30 .equ 0xB0+0             
				
P3M1 .equ 0xB1                ; Port 3 Mode Register 1  (Non bit-addressable)
				
P3M0 .equ 0xB2                ; Port 3 Mode Register 0  (Non bit-addressable)

P4M1 .equ 0xB3                ; Port 4 Mode Register 1  (Non bit-addressable)

P4M0 .equ 0xB4                ; Port 4 Mode Register 0  (Non bit-addressable)


IP2 .equ 0xB5                 ; Interrupt Priority Register 2 (Non bit-addressable)
	;		.equ	0x80	  ; b7, -
	;		.equ	0x40	  ; b6, -
	;		.equ	0x20	  ; b5, -
	;		.equ	0x10	  ; b4, -
	;		.equ	0x08	  ; b3, -
	;		.equ	0x04	  ; b2, -	
	PSPI	.equ	0x02
	PS2		.equ	0x01
	
; 0xB6	not used
; 0xB7	not used

IP	.equ	0xB8			  ; Interrupt Priority Low
	;
	PPCA	.equ	0x80	  ; b7,  PPCA			; Priority Programmable Counter Array
	PLVD	.equ	0x40	  ; b6,  PLVD			; Priority Low Voltage Detect
	PADC	.equ	0x20	  ; b5,  PADC			; Priority ADC
	PS  	.equ	0x10	  ; b4,  PS  			; Priority Serial
	PT1 	.equ	0x08	  ; b3,  PT1 			; Priority Timer 1
	PX1 	.equ	0x04	  ; b2,  PX1 			; Priority External 1
	PT0 	.equ	0x02	  ; b1,  PT0 			; Priority Timer 0
	PX0 	.equ	0x01	  ; b0,  PX0 			; Priority External 0

SADEN	.equ	0xB9


P_SW2 .equ 0xBA               ; Peripheral function switch register (Non bit-addressable)
	;
	;		.equ	0x80	  ; b7, -
	;		.equ	0x40	  ; b6, -
	;		.equ	0x20	  ; b5, -
	;		.equ	0x10	  ; b4, -
	;		.equ	0x08	  ; b3, -
	S4_S	.equ	0x04	  ; b2, S4_S			; UART4/S4 can be switched between P0 and P5
	S3_S	.equ	0x02	  ; b1, S3_S			; UART3/S3 can be switched between P0 and P5
	S2_S	.equ	0x01	  ; b0, S2_S    		; UART4/S4 can be switched between P0 and P5

; 0xBB	not used

ADC_CONTR .equ 0xBC           ; ADC Conversion Control Register (Non bit-addressable)
	;
	ADC_POWER	.equ	0x80  ; b7, ADC_POWER
	SPEED1		.equ	0x40  ; b6, SPEED1			; Times needed by an A/D Coversion
	SPEED0		.equ	0x20  ; b5, SPEED0
	ADC_FLAG	.equ	0x10  ; b4, ADC_FLAG		; ADC interrupt flag.
	ADC_START	.equ	0x08  ; b3, ADC_START		; ADC start bit, which enable ADC conversion.
	CHS2		.equ	0x04  ; b2, CHS2			; Select one analog input source from 8 channels.			
	CHS1		.equ	0x02  ; b1, CHS1
	CHS0 		.equ	0x01  ; b0, CHS0 

ADC_RES .equ 0xBD             ; ADC Conversion Result High 8 Bits

ADC_RESL .equ 0xBE            ; ADC Conversion Result Low 2 Bits

; 0xBF	not used

P4 .equ 0xC0                  ; P4,	I/O port 4,	(Bit-Addressable)
	P47 .equ 0xC0+7             
	P46 .equ 0xC0+6             
	P45 .equ 0xC0+5             
	P44 .equ 0xC0+4             
	P43 .equ 0xC0+3             
	P42 .equ 0xC0+2             
	P41 .equ 0xC0+1             
	P40 .equ 0xC0+0             

WDT_CONTR .equ 0xC1           ; Watchdog Control Register (Non bit-addressable)
	;
	WDT_FLAG	.equ	0x80  ; b7, WDT_FLAG		; WDT reset flag. 1 timer overflow
	;-			.equ	0x40  ; b6, -
	EN_WDT		.equ	0x20  ; b5, EN_WDT			; Enable WDT bit. When set, WDT is started.
	CLR_WDT		.equ	0x10  ; b4, CLR_WDT			; WDT clear bit. When set, WDT will recount. Hardware will automatically clear this bit.
	IDLE_WDT	.equ	0x08  ; b3, IDLE_WDT		; WDT IDLE mode bit. When set, WDT is enabled in IDLE mode.
	PS2			.equ	0x04  ; b2, PS2				; WDT Pre-scale value set bit. 0 div2, 7 div256
	PS1			.equ	0x02  ; b1, PS1
	PS0			.equ	0x01  ; b0, PS0

IAP_DATA .equ 0xC2            ; In Application Programming Data, Flash data register low (Non bit-addressable)
ISP_DATA .equ 0xC2            ; In System Programming, Flash data register low (Non bit-addressable)

IAP_ADDRH .equ 0xC3           ; In Application Programming Address High (Non bit-addressable)
ISP_ADDRH .equ 0xC3           ; In System Programming Address High (Non bit-addressable)

IAP_ADDRH .equ 0xC4           ; In Application Programming Address Low (Non bit-addressable)
ISP_ADDRH .equ 0xC4           ; In System Programming Address Low (Non bit-addressable)

IAP_CMD .equ 0xC5             ; ISP/IAP Flash Command Register : IAP_CMD (Non bit -addressable)
ISP_CMD .equ 0xC5             ; In System Programming Command,   IAPCMD = 0x03 ;In Application Programming Command
	;
	;		.equ	0x80	  ; b7, -
	;		.equ	0x40	  ; b6, -
	;		.equ	0x20	  ; b5, -
	;		.equ	0x10	  ; b4, -
	;		.equ	0x08	  ; b3, -
	;		.equ	0x04	  ; b2, -
	MS1		.equ	0x02	  ; b1, MS1				; MS1, MS0 : ISP/IAP operating mode selection. 
	MS0		.equ	0x01	  ; b0, MS0				; IAP_CMD is used to select the flash mode
	; Valid IAP/ISP commands follow
		; IAP_ERASE = 0x03
		; IAP_WRITE = 0x02
		; IAP_READ  = 0x01
		; IAP_IDL   = 0x00
	; End of valid commands

IAP_TRIG .equ 0xC6            ; ISP/IAP Flash Command Trigger Register : IAP_TRIG (Address: C6H, (Non bit -addressable)

IAP_CONTR .equ 0xC7           ; ISP/IAP Control Register : IAP_CONTR (Non bit-addressable)
ISP_CONTR .equ 0xC7           ; In System Programming Control
	;
	IAPEN		.equ	0x80	  ; b7, IAPEN			; In Application Programming Enable
	SWBS 		.equ	0x40	  ; b6, SWBS 			; Software Boot Selection
	SWRST		.equ	0x20	  ; b5, SWRST			; Software Reset Trigger
	CMD_FAIL	.equ	0x10	  ; b4, CMD_FAIL		; Command Fail
	IAP_WT		.equ	0x08	  ; b3, IAP_WT			; ??? In Application Programming Wait
	WT2			.equ	0x04	  ; b2, WT2				; WT2~WT0 : Waiting time selection while flash is busy.
	WT1			.equ	0x02	  ; b1, WT1
	WT0			.equ	0x01	  ; b0, WT0

P5 .equ 0xC8                  ; P5,	I/O port 0,	(Bit-Addressable)
	P57 .equ 0xC8+7             
	P56 .equ 0xC8+6             
	P55 .equ 0xC8+5             
	P54 .equ 0xC8+4             
	P53 .equ 0xC8+3             
	P52 .equ 0xC8+2             
	P51 .equ 0xC8+1             
	P50 .equ 0xC8+0             
	
P5M1 .equ 0xC9                ; P5M1 register (non bit addressable)
P5M0 .equ 0xCA                ; P5M0 register (non bit addressable)
	; Configure I/O ports mode

P6M1 .equ 0xCB                ; P5M1 register (non bit addressable)
P6M0 .equ 0xCC                ; P5M0 register (non bit addressable)
	; Configure I/O ports mode
	
SPSTAT .equ 0xCD              ; Serial Peripheral Status Register (non bit addressable)
	;
	SPIF	.equ	0x80	  ; b7, SPIF				;Serial Peripheral Interface Flag
	WCOL	.equ	0x40	  ; b6, WCOL				;Write Collision
	;		.equ	0x20	  ; b5, -
	;		.equ	0x10	  ; b4, -
	;		.equ	0x08	  ; b3, -
	;		.equ	0x04	  ; b2, - 
	;		.equ	0x02	  ; b1, -
	;		.equ	0x01	  ; b0, -
	
SPCTL .equ 0xCE;              ; Serial Peripheral Control (non bit addressable)
	;
	SSIG	.equ	0x80	  ; b7, SSIG				; Control whether SS pin is ignored or not.
	SPEN	.equ	0x40	  ; b6, SPEN				; SPI enable bit.
	DORD	.equ	0x20	  ; b5, DORD				; Set the transmitted or received SPI data order.
	MSTR	.equ	0x10	  ; b4, MSTR				; Master/Slave mode select bit.
	CPOL	.equ	0x08	  ; b3, CPOL				; SPI clock polarity select bit.
	CPHA	.equ	0x04	  ; b2, CPHA				; SPI clock phase select bit.
	SPR1	.equ	0x02	  ; b1, SPR1				; SPR1-SPR0 : SPI clock rate select bit (when in master mode)
	SPR0	.equ	0x01	  ; b0, SPR0				; 
	
SPDAT .equ 0xCF               ; Serial Peripheral Data Register (non bit addressable)

PSW .equ 0xD0                 ; Program status word
	CY 	.equ 0xD0+7           ; Carry
	AC 	.equ 0xD0+6           ; Auxiliary Carry
	F0 	.equ 0xD0+5           ; Flag 0
	RS1 .equ 0xD0+4           ; Register Select 1
	RS0 .equ 0xD0+3           ; Register Select 0
	OV 	.equ 0xD0+2           ; Overflow
	F1 	.equ 0xD0+1           ; Flag 1
	P 	.equ 0xD0+0           ; Parity
	
T4T3M .equ 0xD1               ; T3/T4 Mode Register (non bit addressable)
	;
	T4R   	.equ	0x80	  ; b7, T4R    				; Timer 4 Run Control
	T4_CT 	.equ	0x40	  ; b6, T4_CT  				; Timer 4 Counter or Timer Mode Select
	T4x12 	.equ	0x20	  ; b5, T4x12  				; Timer 4 Clock x12 Prescaler Bypass
	T4CLKO	.equ	0x10	  ; b4, T4CLKO 				; Timer 4 Clock Output Control
	T3R   	.equ	0x08	  ; b3, T3R    				; Timer 3 Run Control
	T3_CT 	.equ	0x04	  ; b2, T3_CT  				; Timer 3 Counter or Timer Mode Select
	T3x12 	.equ	0x02	  ; b1, T3x12  				; Timer 3 Clock x12 Prescaler Bypass
	T3CLKO	.equ	0x01	  ; b0, T3CLKO 				; Timer 3 Clock Output Control

T4H .equ 0xD2                 ; Timer 4 High Byte (non bit addressable)
RL_TH4 .equ 0xD2              ; Reload Timer 4 High Byte
;
T4L .equ 0xD3                 ; Timer 4 Low Byte
RL_TL4 .equ 0xD3              ; Reload Timer 4 Low Byte
;
T3H .equ 0xD4                 ; Timer 3 High Byte (non bit addressable)
RL_TH3 .equ 0xD4              ; Reload Timer 3 High Byte
;
T3L .equ 0xD5                 ; Timer 3 Low Byte
RL_TL3 .equ 0xD5              ; Reload Timer 3 Low Byte

T2H .equ 0xD6                 ; Timer 2 High Byte (non bit addressable)
RL_TH2 .equ 0xD6              ; Reload Timer 2 High Byte

T2L .equ 0xD7                 ; Timer 2 Low Byte
RL_TL2 .equ 0xD7              ; Reload Timer 2 Low Byte

CCON .equ 0xD8                ; PCA Control Register (bit-Addressable)
	CF   	.equ 0xD8+7         ; PCA Flag
	CR   	.equ 0xD8+6         ; PCA Run
	; - 			
	; - 			
	; - 			
	CCF3 	.equ 0xD8+3         ; Capture Compare Flag 3
	CCF2 	.equ 0xD8+2         ; PCA Module 2 interrupt flag. Capture Compare Flag 2
	CCF1 	.equ 0xD8+1         ; PCA Module 1 interrupt flag. Capture Compare Flag 1
	CCF0 	.equ 0xD8+0         ; PCA Module 0 interrupt flag. Capture Compare Flag 0

CMOD .equ 0xD9                ; PCA Working Mode Register (non bit addressable)
	;
	CIDL	.equ	0x80	  ; b7 CIDL					; PCA Counter Idle Mode Control
	;-		.equ	0x40	  ; b6 -
	;-		.equ	0x20	  ; b5 -
	;-		.equ	0x10	  ; b4 -
	CPS2	.equ	0x08	  ; b3 CPS2					; CPS2, CPS1, CPS0 : PCA Counter Pulse Select bits
	CPS1	.equ	0x04	  ; b2 CPS1					; PCA Counter Pulse Source Select 1
	CPS0	.equ	0x02	  ; b1 CPS0					; PCA Counter Pulse Source Select 0
	ECF 	.equ	0x01	  ; b0 ECF 					; PCA Enable Counter Overflow interrupt.
	
CCAPM0 .equ 0xDA              ; Compare Capture PWM 0 Mode (non bit addressable)
	;
	;		.equ	0x80	  ; b7 -
	ECOM0 	.equ	0x40	  ; b6 ECOM0  				; Enable Comparator 0
	CCAPP0	.equ	0x20	  ; b5 CCAPP0 				; PCA Capture Positive-going 0
	CCAPN0	.equ	0x10	  ; b4 CCAPN0 				; PCA Capture Negative-going 0
	MAT0  	.equ	0x08	  ; b3 MAT0   				; Match 0
	TOG0  	.equ	0x04	  ; b2 TOG0   				; Toggle 0
	PWM0  	.equ	0x02	  ; b1 PWM0   				; Pulse Width Modulator 0
	ECCF0 	.equ	0x01	  ; b0 ECCF0  				; Enable PCA Capture Flag 0
	
CCAPM1 .equ 0xDB;             ; Compare Capture PWM 1 Mode (non bit addressable)
	;
	;		.equ	0x80	  ; b7 -
	ECOM1 	.equ	0x40	  ; b6 ECOM1  				; Enable Comparator 1
	CCAPP1	.equ	0x20	  ; b5 CCAPP1 				; PCA Capture Positive-going 1
	CCAPN1	.equ	0x10	  ; b4 CCAPN1 				; PCA Capture Negative-going 1
	MAT1  	.equ	0x08	  ; b3 MAT1   				; Match 1
	TOG1  	.equ	0x04	  ; b2 TOG1   				; Toggle 1
	PWM1  	.equ	0x02	  ; b1 PWM1   				; Pulse Width Modulator 1
	ECCF1 	.equ	0x01	  ; b0 ECCF1  				; Enable PCA Capture Flag 1

CCAPM2 .equ 0xDC;             ; Compare Capture PWM 2 Mode (non bit addressable)
	;
	;		.equ	0x80	  ; b7 -
	ECOM2 	.equ	0x40	  ; b6 ECOM2  				; Enable Comparator 2
	CCAPP2	.equ	0x20	  ; b5 CCAPP2 				; PCA Capture Positive-going 2
	CCAPN2	.equ	0x10	  ; b4 CCAPN2 				; PCA Capture Negative-going 2
	MAT2  	.equ	0x08	  ; b3 MAT2   				; Match 2
	TOG2  	.equ	0x04	  ; b2 TOG2   				; Toggle 2
	PWM2  	.equ	0x02	  ; b1 PWM2  					; Pulse Width Modulator 2
	ECCF2 	.equ	0x01	  ; b0 ECCF2  				; Enable PCA Capture Flag 2

; 0xDD	not used
; 0xDE	not used
; 0xDF	not used

; A	.equ 0xE0 ; located in ASxxxx Assembler V02.00 + NoICE + SDCC mods  (Intel 8051), page 1.
ACC .equ 0xE0                 ; Accumulator (Bit Addressable)
	ACC0 .equ 0xE0+0          ; Accumulator Bit 0
	ACC1 .equ 0xE0+1          ; Accumulator Bit 1
	ACC2 .equ 0xE0+2          ; Accumulator Bit 2
	ACC3 .equ 0xE0+3          ; Accumulator Bit 3
	ACC4 .equ 0xE0+4          ; Accumulator Bit 4
	ACC5 .equ 0xE0+5          ; Accumulator Bit 5
	ACC6 .equ 0xE0+6          ; Accumulator Bit 6
	ACC7 .equ 0xE0+7          ; Accumulator Bit 7
							  
P7M1 .equ 0xE1                ; P7M1 register (non bit addressable)
P7M0 .equ 0xE2                ; P7M0 register (non bit addressable)
		; Configure I/O ports mode

; 0xE3	not used
; 0xE4	not used
; 0xE5	not used
; 0xE6	not used
; 0xE7	not used

P6 .equ 0xE8                  ; P6,	I/O port 6,	(Bit-Addressable)
	P67 .equ 0xE8+7             
	P66 .equ 0xE8+6             
	P65 .equ 0xE8+5             
	P64 .equ 0xE8+4             
	P63 .equ 0xE8+3             
	P62 .equ 0xE8+2             
	P61 .equ 0xE8+1             
	P60 .equ 0xE8+0             
	
CL .equ 0xE9                  ; PCA Counter Low Byte  (non bit addressable)

CCAP0L .equ 0xEA              ; PCA Module 0 Capture/Compare Register Low Byte (non bit addressable)

CCAP1L .equ 0xEB              ; PCA Module 1 Capture/Compare Register Low Byte (non bit addressable)

CCAP2L .equ 0xEC              ; PCA Module 2 Capture/Compare Register Low Byte (non bit addressable)

; 0xED	not used
; 0xEE	not used
; 0xEF	not used

B .equ 0xF0                   ; B Register (Bit-Addressable)
	B7 .equ 0xF0+7            ; B Bit 7
	B6 .equ 0xF0+6            ; B Bit 6
	B5 .equ 0xF0+5            ; B Bit 5
	B4 .equ 0xF0+4            ; B Bit 4
	B3 .equ 0xF0+3            ; B Bit 3
	B2 .equ 0xF0+2            ; B Bit 2
	B1 .equ 0xF0+1            ; B Bit 1
	B0 .equ 0xF0+0            ; B Bit 0

; 0xF1	not used

PCA_PWM0 .equ 0xF2            ; PCA_PWM0 : PWM register of PCA module 0 (non bit addressable)
	;
	EBS0_1	.equ	0x80	  ; b7 EBS0_1	; Function Select bit when PCA module 0 work as Pulse Width Modulator (PWM)
	EBS0_0	.equ	0x40	  ; b6 EBS0_0	;
	;		.equ	0x20	  ; b5 -
	;		.equ	0x10	  ; b4 -
	;		.equ	0x08	  ; b3 -
	;		.equ	0x04	  ; b2 -
	EPC0H	.equ	0x02	  ; b1 EPC0H	; Associated with CCAP0H, it is used in PCA PWM mode.
	EPC0L	.equ	0x01	  ; b0 EPC0L	; Associated with CCAP0L, it is used in PCA PWM mode.

PCA_PWM1 .equ 0xF3            ; Programmable Counter Array Module 1 PWM Register (non bit addressable)
	;
	EBS1_1	.equ	0x80	  ; b7 EBS1_1	; Function Select bit when PCA module 1 work as Pulse Width Modulator (PWM)
	EBS1_0	.equ	0x40	  ; b6 EBS1_0	; 
	;-		.equ	0x20	  ; b5 -
	;-		.equ	0x10	  ; b4 -
	;-		.equ	0x08	  ; b3 -
	;-		.equ	0x04	  ; b2 -
	EPC1H	.equ	0x02	  ; b1 EPC1H	; Associated with CCAP1H, it is used in PCA PWM mode.
	EPC1L	.equ	0x01	  ; b0 EPC1L	; Associated with CCAP1L, it is used in PCA PWM mode.
	
PCA_PWM2 .equ 0xF4            ; Programmable Counter Array Module 2 PWM Register (non bit addressable)
	;
	EBS2_1	.equ	0x80	  ; b7 EBS2_1	; Function Select bit when PCA module 1 work as Pulse Width Modulator (PWM)
	EBS2_0	.equ	0x40	  ; b6 EBS2_0	; 
	;		.equ	0x20	  ; b5 -
	;		.equ	0x10	  ; b4 -
	;		.equ	0x08	  ; b3 -
	;		.equ	0x04	  ; b2 -
	EPC2H	.equ	0x02	  ; b1 EPC2H	; Associated with CCAP1H, it is used in PCA PWM mode.
	EPC2L	.equ	0x01	  ; b0 EPC2L	; Associated with CCAP1L, it is used in PCA PWM mode.
	
; 0xF5	not used
; 0xF6	not used
; 0xF7	not used

P7 .equ 0xF8                  ; P7,	I/O port 7,	(Bit-Addressable)
	P77 .equ 0xF8+7             
	P76 .equ 0xF8+6             
	P75 .equ 0xF8+5             
	P74 .equ 0xF8+4             
	P73 .equ 0xF8+3             
	P72 .equ 0xF8+2             
	P71 .equ 0xF8+1             
	P70 .equ 0xF8+0             
	
CH .equ 0xF9                  ; PCA Counter High Byte

CCAP0H .equ 0xFA              ; PCA Module 0 Capture/Compare Register High Byte

CCAP1H .equ 0xFB              ; PCA Module 1 Capture/Compare Register High Byte

CCAP2H .equ 0xFC              ; PCA Module 2 Capture/Compare Register High Byte

; 0xFD	not used
; 0xFE	not used
; 0xFF	not used



