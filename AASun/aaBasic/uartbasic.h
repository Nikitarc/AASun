/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	uart.h		Simple UART driver

	When		Who	What
	11/07/17	ac	Creation
	12/06/21	ac	Add options SWAPRXTX, TXINV, RXINX, DATAINV

----------------------------------------------------------------------
*/
/*
----------------------------------------------------------------------
	Copyright (c) 2013-2023 Alain Chebrou - AdAstra-Soft . com
	All rights reserved.

	This file is part of the AdAstra Real Time Kernel distribution.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.

	- Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.

	- Neither the name of the copyright holder nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----------------------------------------------------------------------
*/

#if ! defined AA_UART_H_
#define AA_UART_H_
//-----------------------------------------------------------------------------

#include	"aaprintf.h"		// For aaDevIo_t declaration

//-----------------------------------------------------------------------------
//	UART package configuration: Enabled UARTs/USARTs are defined to 1

// Don't set to 1 the USART not present in the used MCU
#define	WITH_UART1		0	// WIFI (not managed by uartbasic)
#define	WITH_UART2		1	// Console
#define	WITH_UART3		0	// Temperature (not managed by uartbasic)
#define	WITH_UART4		0
#define	WITH_UART5		0
#define	WITH_UART6		0
#define	WITH_LPUART1	1	// Linky
#define	WITH_LPUART2	0

// Priority of UART interrupts, right above sysTick priority (which is at lowest priority)
#define	UART_IRQ_PRIORITY		BSP_IRQPRIOMIN_PLUS(1)

//-----------------------------------------------------------------------------
// Flags of UART descriptor

// These flags can be used with uartInit(), uartSetFlag() and uartGetFlag()
#define	UART_FLAG_MASK		0xFFu
#define	UART_FLAG_NOIT		0x01u		// Don't use interrupts with this UART
#define	UART_FLAG_LFCRLF	0x04u		// On Tx, convert LF to CR-LF

// These flags can be used with uartInit() only
#define	UART_FLAG_SWAPRXTX	0x10u		// Swap TX and RX pin
#define	UART_FLAG_TXINV		0x20u		// Invert TX
#define	UART_FLAG_RXINV		0x40u		// Invert RX
#define	UART_FLAG_DATAINV	0x80u		// Binary data inversion

typedef void *	uartHandle_t ;			// UART handle

#ifdef __cplusplus
extern "C" {
#endif

aaError_t	uartInit			(uint32_t uart, uint32_t baudrate, uint32_t flags, uartHandle_t * pHandle) ;
aaError_t	uartClose			(uartHandle_t hUsart) ;
aaError_t	uartSetFlag			(uartHandle_t hUsart, uint32_t flag, uint32_t bSet) ;
aaError_t	uartGetFlag			(uartHandle_t hUsart, uint32_t * pFlag) ;

void 		uartPutChar			(uartHandle_t hUsart, char cc) ;
char 		uartGetChar			(uartHandle_t hUsart) ;
uint32_t	uartCheckGetChar	(uartHandle_t hUsart) ;
uint32_t	uartCheckPutChar	(uartHandle_t hUsart) ;

//	To initialize a UART as standard IO descriptor
void	uartInitStdDev (aaDevIo_t * pDev, uartHandle_t hUsart) ;

#ifdef __cplusplus
}
#endif

//-----------------------------------------------------------------------------
#endif 	// AA_UART_H_
