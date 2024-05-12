/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	uart.c		Basic UART management for STM32G0xx

	When		Who	What
	10/07/17	ac	Creation
	16/01/18	ac	Port to STM32F429
	02/09/18	ac	modification of pin desc to add flags
					Add flag UART_FLAG_NOIT : don't use interrupts with this uart
	23/11/18	ac	Port to STM32H743 HAL
	16/01/20	ac	Port to H7 LL drivers 1.6. ST changed register bits names, sigh...
					Add 'flags' parameter to uartInit(). Improved 'no interrupt' handling
	20/03/20	ac	Add LPUART1 management for STM32L4+
	26/01/21	ac	Port to STM32G0 from L4+ (Add basic USART management)
	12/04/23	ac	Add conditional GPIO configuration (the GPIO is ignored if the port is 15 or 'P')
					This allows RX only or TX only UART
	23/04/23	ac	Bug in TX FIFO threshold interrupt management
	15/06/23	ac	Clear OREC on RXNE interrupt


	Use LL driver for initialization (It's easier: for example it takes into account the multiple clock sources of each USART)
	Use register access for IO, it is more efficient
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

/*
-----------------------------------------------------------------------------
	UART can be used before the kernel is started and interrupts are enabled.
	To initialize an UART before the kernel is started, use the UART_FLAG_NOIT flag
	then the UART functions will use polling to send and receive characters.
	Interrupts should not be enabled before the kernel is started by aaStart_() function.

	This driver use TX and RX FIFO to lower the interruption rate to a minimum
	on full USART and LPUART.

	The uartPutChar() and uartGetChar() functions can not be called from an ISR
-----------------------------------------------------------------------------
*/
#include	"aa.h"
#include	"aakernel.h"
#include	"gpiobasic.h"
#include	"rccbasic.h"
#include	"uartbasic.h"

#include	"stm32g0xx_ll_usart.h"
#include	"stm32g0xx_ll_lpuart.h"
#include	"stm32g0xx_ll_rcc.h"

// On all G0, the IRQn for these (LP)USART is the same:
#define		IRQNUM_USART_3_4_5_6_LP1	29

//-----------------------------------------------------------------------------
// Define TX and RX buffers sizes for each UART

#define UART1_TX_SIZE		32		// AASun WIFI
#define	UART1_RX_SIZE		32

#define	UART2_TX_SIZE		2048	// AASun debug (console)
#define	UART2_RX_SIZE		32

#define	UART3_TX_SIZE		8		// AASun modified PCB V1: Temperature on expansion connector
#define	UART3_RX_SIZE		8

#define	UART4_TX_SIZE		32
#define	UART4_RX_SIZE		32

#define	UART5_TX_SIZE		32
#define	UART5_RX_SIZE		32

#define	UART6_TX_SIZE		32
#define	UART6_RX_SIZE		32

#define LPUART1_TX_SIZE		16		// AASun modified PCB V1: Linky
#define	LPUART1_RX_SIZE		8

#define LPUART2_TX_SIZE		32
#define	LPUART2_RX_SIZE		32

typedef uint16_t		uartTx_t ;
typedef uint16_t		uartRx_t ;

//-----------------------------------------------------------------------------
//	UART descriptor

//	The pin descriptor is a 32 bits word: xxxx xxxx xxxx xxxx xxxx xxxx xxxxxxxx
//					                      rxAf txAf txPt txPt rxPn txPn    flags

#define	pin2desc(txPin, txPort, rxPin, rxPort, txAf, rxAf, flags)	        \
				(	(((txPin)  & 0x0F) << 8)  | (((txPort) & 0x0F) << 16)  |\
					(((rxPin)  & 0x0F) << 12) | (((rxPort) & 0x0F) << 20)  |\
					(((txAf)   & 0x0F) << 24) | (((rxAf)   & 0x0F) << 28)  |\
					((flags) & UART_FLAG_MASK) \
				)

#define	desc2TxPin(desc)		(((desc) >> 8u)  & 0x0Fu)
#define	desc2TxPort(desc)		(((desc) >> 16u) & 0x0Fu)
#define	desc2RxPin(desc)		(((desc) >> 12u) & 0x0Fu)
#define	desc2RxPort(desc)		(((desc) >> 20u) & 0x0Fu)
#define	desc2TxAf(desc)			(((desc) >> 24u) & 0x0Fu)
#define	desc2RxAf(desc)			(((desc) >> 28u) & 0x0Fu)
#define	desc2Flags(desc)		((desc) & UART_FLAG_MASK)

// Internal flags
#define	IFLAG_BASIC			0x01u		// UART is basic: no FIFO
#define	IFLAG_NO_TX			0x02u		// TX not used
#define	IFLAG_NO_RX			0x04u		// RX not used

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// To set the IFLAG_BASIC field of the uart descriptor flags:
// Some USART are FULL featured, others are BASIC.
// Some USART can be FULL or BASIC depending on the MCU

#define	USART1_TYPE			0					// Always FULL

#if (defined RCC_CCIPR_USART2SEL)
	#define	USART2_TYPE		0					// FULL
#else
	#define	USART2_TYPE		IFLAG_BASIC			// BASIC
#endif

#if (defined RCC_CCIPR_USART3SEL)
	#define	USART3_TYPE		0					// FULL
#else
	#define	USART3_TYPE		IFLAG_BASIC			// BASIC
#endif

#define	USART4_TYPE			IFLAG_BASIC			// Always BASIC
#define	USART5_TYPE			IFLAG_BASIC			// Always BASIC
#define	USART6_TYPE			IFLAG_BASIC			// Always BASIC

#define	LPUART1_TYPE		0					// FULL
#define	LPUART2_TYPE		0					// FULL

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// The UART descriptor structure

typedef struct
{
	uint32_t		desc ;			// Configuration of tx and rx pins
	uint32_t		iflags ;		// Internal flags
	USART_TypeDef	* uartBase ;	// UARTx from stm32xxxx.h (pointer to registers)

					// Buffers management
	uartTx_t		txSize ;
	uartTx_t		iTxRead, iTxWrite, txCount ;
	uartRx_t		rxSize ;
	uartRx_t		iRxRead, iRxWrite, rxCount ;
	uint8_t			* pTxBuffer ;
	uint8_t			* pRxBuffer ;

	aaDriverDesc_t	txList ;		// The list of tasks waiting for Tx
	aaDriverDesc_t	rxList ;		// The list of tasks waiting for Rx

} uartDesc_t ;

static	uartDesc_t	* uartSelect (uint32_t	uart) ;

//-----------------------------------------------------------------------------

#if (WITH_UART1 == 1)

STATIC	uint8_t		uart1Tx [UART1_TX_SIZE] ;
STATIC	uint8_t		uart1Rx [UART1_RX_SIZE] ;

STATIC	uartDesc_t		uart1 =
{
		pin2desc (9u,  0u, 10u, 0u, 1u, 1u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (6u,  1u,  7u, 1u, 0u, 0u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (4u,  2u,  5u, 2u, 1u, 1u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART1_TYPE,		// iFlags
		USART1,				// USARTx

		UART1_TX_SIZE,
		0, 0, 0,
		UART1_RX_SIZE,
		0, 0, 0,
		uart1Tx,
		uart1Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (WITH_UART2 == 1)

STATIC	uint8_t		uart2Tx [UART2_TX_SIZE] ;
STATIC	uint8_t		uart2Rx [UART2_RX_SIZE] ;

STATIC	uartDesc_t		uart2 =
{
		pin2desc ( 2u, 0u,  3u, 0u, 1u, 1u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (14u, 0u, 15u, 0u, 1u, 1u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc ( 5u, 3u,  6u, 3u, 0u, 0u, 0),	// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART2_TYPE,		// iFlags
		USART2,				// USARTx

		UART2_TX_SIZE,
		0, 0, 0,
		UART2_RX_SIZE,
		0, 0, 0,
		uart2Tx,
		uart2Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined USART3  &&  WITH_UART3 == 1)

STATIC	uint8_t		uart3Tx [UART3_TX_SIZE] ;
STATIC	uint8_t		uart3Rx [UART3_RX_SIZE] ;

STATIC	uartDesc_t		uart3 =
{
		pin2desc (10u, 15u, 11u, 1u, 4u, 4u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
														// TX not used (port is 15)

//		pin2desc ( 5u, 0u,   0u, 1u, 4u, 4u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc ( 8u, 1u,   9u, 1u, 4u, 4u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (10u, 1u,  11u, 1u, 4u, 4u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc ( 4u, 2u,   5u, 2u, 0u, 0u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (10u, 2u,  11u, 2u, 0u, 0u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART3_TYPE,		// iFlags
		USART3,				// USARTx

		UART3_TX_SIZE,
		0, 0, 0,
		UART3_RX_SIZE,
		0, 0, 0,
		uart3Tx,
		uart3Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined USART4  &&  WITH_UART4 == 1)

STATIC	uint8_t		uart4Tx [UART4_TX_SIZE] ;
STATIC	uint8_t		uart4Rx [UART4_RX_SIZE] ;

STATIC	uartDesc_t		uart4 =
{
		pin2desc ( 0u, 0u,  1u, 0u, 8u, 8u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (10u, 2u, 11u, 2u, 8u, 8u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART4_TYPE,		// iFlags
		USART4,				// UARTx

		UART4_TX_SIZE,
		0, 0, 0,
		UART4_RX_SIZE,
		0, 0, 0,
		uart4Tx,
		uart4Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined USART5  &&  WITH_UART5 == 1)

STATIC	uint8_t		uart5Tx [UART5_TX_SIZE] ;
STATIC	uint8_t		uart5Rx [UART5_RX_SIZE] ;

STATIC	uartDesc_t		uart5 =
{
		pin2desc (12u, 2u, 2u, 3u, 8u, 8u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART5_TYPE,		// iFlags
		USART5,				// USARTx

		UART5_TX_SIZE,
		0, 0, 0,
		UART5_RX_SIZE,
		0, 0, 0,
		uart5Tx,
		uart5Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined USART6  &&  WITH_UART6 == 1)

STATIC	uint8_t		uart6Tx [UART6_TX_SIZE] ;
STATIC	uint8_t		uart6Rx [UART6_RX_SIZE] ;

STATIC	uartDesc_t		uart6 =
{
		pin2desc (12u, 2u, 2u, 3u, 8u, 8u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		USART6_TYPE,		// iFlags
		USART6,				// USARTx

		UART6_TX_SIZE,
		0, 0, 0,
		UART6_RX_SIZE,
		0, 0, 0,
		uart6Tx,
		uart6Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined LPUART1  &&  WITH_LPUART1 == 1)

STATIC	uint8_t		lpuart1Tx [LPUART1_TX_SIZE] ;
STATIC	uint8_t		lpuart1Rx [LPUART1_RX_SIZE] ;

STATIC	uartDesc_t		lpuart1 =
{
//		pin2desc ( 2u,  0u,  3u,  0u, 6u, 6u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		pin2desc (11u, 15u, 10u,  1u, 1u, 1u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc ( 1u,  2u,  0u,  2u, 1u, 1u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		LPUART1_TYPE,			// iFlags
		LPUART1,				// UARTx

		LPUART1_TX_SIZE,
		0, 0, 0,
		LPUART1_RX_SIZE,
		0, 0, 0,
		lpuart1Tx,
		lpuart1Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------

#if (defined LPUART2  &&  WITH_LPUART2 == 1)

STATIC	uint8_t		lpuart2Tx [LPUART2_TX_SIZE] ;
STATIC	uint8_t		lpuart2Rx [LPUART2_RX_SIZE] ;

STATIC	uartDesc_t		lpuart2 =
{
//		pin2desc ( 2u, 0u,  3u, 0u, 8u, 8u, LPUART1_TYPE),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc (11u, 1u, 10u, 1u, 8u, 8u, LPUART1_TYPE),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
//		pin2desc ( 1u, 2u,  0u, 2u, 8u, 8u, LPUART1_TYPE),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		pin2desc ( 7u, 6u,  8u, 6u, 8u, 8u, 0),		// txPin, txPort, rxPin, rxPort, txAf, rxAf, flags
		LPUART2_TYPE,			// iFlags
		LPUART2,				// UARTx

		LPUART2_TX_SIZE,
		0, 0, 0,
		LPUART2_RX_SIZE,
		0, 0, 0,
		lpuart2Tx,
		lpuart2Rx,

		{ { NULL, NULL } },	// txList
		{ { NULL, NULL } },	// rxList
} ;

#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// To check a flag in a descriptor flags field

__STATIC_INLINE	uint32_t checkDescFlag (uint32_t desc, uint32_t flag)
{
	return ((desc & flag) == 0u) ? 0u : 1u ;
}

//-----------------------------------------------------------------------------
// To check/set/clear an internal flag in a descriptor flags field

__STATIC_INLINE	uint32_t checkInternalFlag (uartDesc_t * pUart, uint32_t flag)
{
	return ((pUart->iflags & flag) == 0u) ? 0u : 1u ;
}

__STATIC_INLINE	void setInternalFlag (uartDesc_t * pUart, uint32_t flag)
{
	pUart->iflags |= flag ;
}

__STATIC_INLINE	void clearInternalFlag (uartDesc_t * pUart, uint32_t flag)
{
	pUart->iflags &= ~flag ;
}

//-----------------------------------------------------------------------------
//	Set a flag to UART descriptor
//	BEWARE:
//	If interrupts are disabled while the pTxBuffer is not empty, these characters will not be sent
//	If interrupts are disabled while tasks are waiting,
//	these tasks will not be awakened until interrupts are enabled again

aaError_t	uartSetFlag		(uartHandle_t hUsart, uint32_t flag, uint32_t bSet)
{
	uartDesc_t				* pUart = (uartDesc_t *) hUsart ;
	volatile USART_TypeDef	* uartBase ;	// UARTx

	if (pUart == NULL  ||  (flag & UART_FLAG_MASK) == 0u)
	{
		return AA_EARG ;
	}
	uartBase = pUart->uartBase ;

	if (bSet != 0u)
	{
		// Set flag
		// If UART_FLAG_NOIT is not set in desc, and set in flag, then disable interrupts
		if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) == 0  &&   (flag & UART_FLAG_NOIT) != 0)
		{
			if (checkInternalFlag (pUart, IFLAG_BASIC) == 0)
			{
				// With FIFO: Disable TX FIFO threshold interrupt
				uartBase->CR3 &= ~USART_CR3_TXFTIE ;	// Disable TX threshold interrupt

				uartBase->CR3 &= ~USART_CR3_RXFTIE ;	// Disable RX FIFO threshold
				if (! IS_LPUART_INSTANCE (uartBase))
				{
					uartBase->CR1 &= ~USART_CR1_RTOIE ;		// Disable RX timeout
				}

				// If TX is enabled then wait until TX FIFO is not full
				// because uartPutChar() require an empty character
				if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
				{
					while ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0)
					{
					}
				}
			}
			else
			{
				// Without FIFO
				uartBase->CR1 &= ~USART_CR1_TXEIE_TXFNFIE ;
			}
		}
		pUart->desc |= flag & UART_FLAG_MASK ;	// Add new flag
	}
	else
	{
		// Clear flag
		// If UART_FLAG_NOIT is set in desc, and set in flag, then enable interrupts
		if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) != 0  &&  (flag & UART_FLAG_NOIT) != 0)
		{
			// Enable interrupts
			if (checkInternalFlag (pUart, IFLAG_BASIC) == 0)
			{
				// With FIFO
				if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
				{
					uartBase->CR3 |= USART_CR3_RXFTIE ;			// RX FIFO threshold
					if (! IS_LPUART_INSTANCE (uartBase))
					{
						uartBase->CR1 |= USART_CR1_RTOIE ;		// RX timeout is present for UART
					}
				}
				// If pTxBuffer is not empty then enable TX interrupt to send these characters
				if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
				{
					if (pUart->txCount != 0)
					{
						uartBase->CR3 |= USART_CR3_TXFTIE ;		// Enable TX threshold interrupt
					}
				}
			}
			else
			{
				// Without FIFO
				if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
				{
					uartBase->CR1 |= USART_CR1_RXNEIE_RXFNEIE ;		// RXNE it enable
				}

				// If pTxBuffer is not empty then enable TX interrupt to send these characters
				if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
				{
					if (pUart->txCount != 0)
					{
						uartBase->CR1 |= USART_CR1_TXEIE_TXFNFIE ;	// Enable TX threshold interrupt
					}
				}
			}
		}
		pUart->desc &= (~flag & UART_FLAG_MASK) ;	// Remove this flag
	}
	return AA_ENONE ;
}

//-----------------------------------------------------------------------------
//	Get a flag from UART descriptor

aaError_t	uartGetFlag		(uartHandle_t hUsart, uint32_t * pFlag)
{
	uartDesc_t	* pUart = (uartDesc_t *) hUsart ;

	if (pUart == NULL)
	{
		return AA_EARG ;
	}
	* pFlag = desc2Flags (pUart->desc) ;
	return AA_ENONE ;
}

//-----------------------------------------------------------------------------
//	Returns pointer to UART/LPUART descriptor structure from UART number
//	For USART:  number 1 to n
//	For LPUART: number 101 to 102

static	uartDesc_t	* uartSelect (uint32_t	uart)
{
	#if (WITH_UART1 == 1)
		if (uart == 1)
		{
			return & uart1 ;
		}
	#endif

	#if (WITH_UART2 == 1)
		if (uart == 2)
		{
			return & uart2 ;
		}
	#endif

	#if (WITH_UART3 == 1)
		if (uart == 3)
		{
			return & uart3 ;
		}
	#endif

	#if (WITH_UART4 == 1)
		if (uart == 4)
		{
			return & uart4 ;
		}
	#endif

	#if (WITH_UART5 == 1)
		if (uart == 5)
		{
			return & uart5 ;
		}
	#endif

#if (WITH_LPUART1 == 1)
	if (uart == 101)
	{
		return & lpuart1 ;
	}
#endif

#if (WITH_LPUART2 == 1)
	if (uart == 102)
	{
		return & lpuart2 ;
	}
#endif

	(void) uart ;
	AA_ASSERT (0) ;
	return NULL ;
}

//-----------------------------------------------------------------------------
//	Returns count of available characters in TX buffer

uint32_t	uartCheckPutChar (uartHandle_t hUsart)
{
	uartDesc_t				* pUart = (uartDesc_t *) hUsart ;
	volatile USART_TypeDef	* uartBase ;	// UARTx

	if (pUart == NULL)
	{
		return 0u ;
	}
	uartBase = pUart->uartBase ;

	if (checkInternalFlag (pUart, IFLAG_NO_TX) != 0)
	{
		return 0 ;	// TX not enabled
	}

	// If the kernel is not started, don't use RX interrupt/buffer
	if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) != 0u)
	{
		return ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0u) ? 0u : 1u ;
	}

	return (uint32_t) (pUart->txSize - pUart->txCount) ;
}

//-----------------------------------------------------------------------------
//	Send one char to UART. Blocking call if TX buffer is full.

void uartPutChar (uartHandle_t hUsart, char cc)
{
	uartDesc_t				* pUart = (uartDesc_t *) hUsart ;
	volatile USART_TypeDef	* uartBase ;	// UARTx
	uint32_t				flagLF = 0u ;

	if (pUart == NULL)
	{
		return ;
	}
	uartBase = pUart->uartBase ;

	if (checkInternalFlag (pUart, IFLAG_NO_TX) != 0)
	{
		return ;	// TX not enabled
	}

	// If cc is LF, then maybe send CR-LF
	if (checkDescFlag (pUart->desc, UART_FLAG_LFCRLF) != 0u  &&  cc == '\n')
	{
		flagLF = 1u ;
		cc = '\r' ;
	}

	// If the kernel is not started, don't use TX interrupt/buffer
	if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) != 0u)
	{
		uartBase->TDR = (uint16_t) cc ;
		// TXE must be 1 when the first char will be send after the kernel start
		// So wait until the FIFO is not full
		while ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0u)
		{
		}

		if (flagLF != 0u)
		{
			uartBase->TDR = '\n' ;
			while ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0u)
			{
			}
		}
		return ;
	}

	// The kernel is started and interrupts are enabled, use TX buffer and TX interrupt
	while (1)
	{
		while (1)
		{
			aaCriticalEnter () ;
			if (pUart->txCount == pUart->txSize)
			{
				// Buffer is full, wait. The critical section will be released by aaIoWait().
				(void) aaIoWait (& pUart->txList, 0u, 0u) ;	// Not ordered, no timeout
			}
			else
			{
				break ;		// Not full. Note: exit while inside the critical section
			}
		}

		if (pUart->txCount != 0u  ||  (uartBase->ISR & USART_ISR_TXE_TXFNF) == 0)
		{
			pUart->pTxBuffer [pUart->iTxWrite++] = (uint8_t) cc ;
			if (pUart->iTxWrite == pUart->txSize)
			{
				pUart->iTxWrite = 0u ;
			}
			pUart->txCount++ ;
		}
		else
		{
			uartBase->TDR = (uint16_t) cc ;
			// If FIFO is full then enable TX FIFO threshold interrupt
			if (checkInternalFlag (pUart, IFLAG_BASIC) == 0)
			{
				// With FIFO: If FIFO is full then enable TX FIFO threshold interrupt
				if ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0)
				{
					uartBase->CR3 |= USART_CR3_TXFTIE ;
				}
			}
			else
			{
				// Without FIFO
				uartBase->CR1 |= USART_CR1_TXEIE_TXFNFIE ;
			}
		}
		aaCriticalExit () ;

		if (flagLF != 0u)
		{
			flagLF = 0u ;
			cc = '\n'  ;
			continue ;
		}
		break ;
	}
}

//-----------------------------------------------------------------------------
//	Returns count of available characters in RX buffer

uint32_t	uartCheckGetChar (uartHandle_t hUsart)
{
	uartDesc_t				* pUart = (uartDesc_t *) hUsart ;
	volatile USART_TypeDef	* uartBase ;	// UARTx

	if (pUart == NULL)
	{
		return 0u ;
	}
	uartBase = pUart->uartBase ;

	if (checkInternalFlag (pUart, IFLAG_NO_RX) != 0)
	{
		return 0 ;	// RX not enabled
	}

	// If the kernel is not started, don't use RX interrupt/buffer
	if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) != 0u)
	{
		return ((uartBase->ISR & USART_ISR_RXNE_RXFNE) == 0u) ? 0u : 1u ;
	}

	return pUart->rxCount  ;
}

//-----------------------------------------------------------------------------
//	Get one char from UART. Blocking call if RX buffer is empty.

char uartGetChar (uartHandle_t hUsart)
{
	uartDesc_t	* pUart = (uartDesc_t *) hUsart ;
	char		cc ;

	if (pUart == NULL  ||  checkInternalFlag (pUart, IFLAG_NO_RX) != 0)
	{
		return (char) 0 ;	// Bad handle or RX not enabled
	}

	// If the kernel is not started, don't use RX interrupt/buffer
	if (aaIsRunning == 0u  ||  checkDescFlag (pUart->desc, UART_FLAG_NOIT) != 0u)
	{
		volatile USART_TypeDef	* uartBase = pUart->uartBase ;	// UARTx

		while ((uartBase->ISR & USART_ISR_RXNE_RXFNE) == 0u)
		{
		}
		return (char) uartBase->RDR  ;
	}


	while (1)
	{
		aaCriticalEnter () ;
		if (pUart->rxCount == 0u)
		{
			// Buffer is empty. aaIoWait() will release the critical section
			(void) aaIoWait (& pUart->rxList, 0u, 0u) ;	// Not ordered, no timeout
		}
		else
		{
			break ;	// Not empty. Note: exit while inside critical section
		}
	}

	cc = (char) pUart->pRxBuffer [pUart->iRxRead++] ;
	if (pUart->iRxRead == pUart->rxSize)
	{
		pUart->iRxRead = 0u ;
	}
	pUart->rxCount-- ;
	aaCriticalExit () ;

	return cc ;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	USART interrupt handlers

void UART_IRQHandler	(uartDesc_t * pUart) ;

void UART_IRQHandler	(uartDesc_t * pUart)
{
	volatile USART_TypeDef	* uartBase = pUart->uartBase ;	// UARTx
	uint16_t				cc ;

	aaIntEnter () ;

	// -------------- RXNE handler
	if (((uartBase->CR1 & USART_CR1_RXNEIE_RXFNEIE) != 0u  &&  (uartBase->ISR & USART_ISR_RXNE_RXFNE) != 0u)  ||
		((uartBase->CR3 & USART_CR3_RXFTIE) != 0u  &&  (uartBase->ISR & USART_ISR_RXFT) != 0u)  ||
		((uartBase->CR1 & USART_CR1_RTOIE) != 0u  &&  (uartBase->ISR & USART_ISR_RTOF) != 0u))
	{
		while ((uartBase->ISR & USART_ISR_RXNE_RXFNE) != 0u)
		{
			cc = uartBase->RDR & 0x0FF ;
			if (pUart->rxCount < pUart->rxSize)
			{
				pUart->pRxBuffer [pUart->iRxWrite++] = (uint8_t) cc ;
				if (pUart->iRxWrite == pUart->rxSize)
				{
					pUart->iRxWrite = 0 ;
				}
				pUart->rxCount++ ;
			}
		}
		if ((uartBase->ISR & USART_ISR_RTOF) != 0u)
		{
			uartBase->ICR |= USART_ICR_RTOCF ;
		}
		// Clear Overrun interrupt flag generated at the same time as RXNE
		uartBase->ICR = USART_ICR_ORECF ;

		(void) aaIoResumeWaitingTask (& pUart->rxList) ;
	}

	// -------------- TXE handler
	if (((uartBase->CR1 & USART_CR1_TXEIE_TXFNFIE) != 0u  &&  (uartBase->ISR & USART_ISR_TXE_TXFNF) != 0u)  ||
		((uartBase->CR3 & USART_CR3_TXFTIE) != 0u  &&  (uartBase->ISR & USART_ISR_TXFT) != 0u))
	{
		if (pUart->txCount != 0u)
		{
			// Character available
			while (pUart->txCount != 0u)
			{
				if ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0)
				{
					break ;	// FIFO full
				}
				uartBase->TDR = pUart->pTxBuffer [pUart->iTxRead++] ;
				if (pUart->iTxRead == pUart->txSize)
				{
					pUart->iTxRead = 0 ;
				}
				pUart->txCount-- ;
			}
			if (pUart->txCount == 0)
			{
				// Empty buffer: there is nothing more to emit
				if (checkInternalFlag (pUart, IFLAG_BASIC) == 0u)
				{
					// With FIFO: disable TX threshold interrupt if the FIFO is under the threshold
					if  ((uartBase->ISR & USART_ISR_TXFT) != 0)
					{
						uartBase->CR3 &= ~USART_CR3_TXFTIE ;
					}
				}
				else
				{
					// Without FIFO
					uartBase->CR1 &= ~USART_CR1_TXEIE_TXFNFIE ;
				}
			}

			aaIoResumeWaitingTask (& pUart->txList) ;
		}
		else
		{
			// Nothing to send, disable TX interrupt
			if (checkInternalFlag (pUart, IFLAG_BASIC) == 0u)
			{
				// With FIFO: disable TX threshold interrupt
				uartBase->CR3 &= ~USART_CR3_TXFTIE ;
			}
			else
			{
				// Without FIFO
				uartBase->CR1 &= ~USART_CR1_TXEIE_TXFNFIE ;
			}
		}
	}

	// ------------------------------------------------------------
	// Other USART interrupts handler can go here ...

	aaIntExit () ;
}

//-----------------------------------------------------------------------------

#if (WITH_UART1 == 1)
	void USART1_IRQHandler	(void) ;
	void USART1_IRQHandler	(void)
	{
		UART_IRQHandler (& uart1) ;
	}
#endif

//-----------------------------------------------------------------------------

#if (WITH_UART2 == 1  ||  WITH_LPUART2 == 1)
	#if defined (LPUART2)
		void USART2_LPUART2_IRQHandler	(void) ;
		void USART2_LPUART2_IRQHandler	(void)
	#else
		void USART2_IRQHandler	(void) ;
		void USART2_IRQHandler	(void)
	#endif
		{
			#if (WITH_UART2 == 1)
			{
				UART_IRQHandler (& uart2) ;
			}
			#endif
			#if (WITH_LPUART2 == 1)
			{
				UART_IRQHandler (& lpuart2) ;
			}
			#endif
		}
#endif

//-----------------------------------------------------------------------------

#if (WITH_UART3 == 1 ||  WITH_UART4 == 1 || WITH_UART5 == 1 ||	\
	 WITH_UART6 == 1 ||  WITH_LPUART1 == 1)

	#if (defined STM32G070xx)

		void USART3_4_IRQHandler	(void) ;
		void USART3_4_IRQHandler	(void)

	#elif (defined STM32G071xx  ||  defined STM32G081xx)

		void USART3_4_LPUART1_IRQHandler	(void) ;
		void USART3_4_LPUART1_IRQHandler	(void)

	#elif (defined STM32G061xx)

		void LPUART1_IRQHandler	(void) ;
		void LPUART1_IRQHandler	(void)

	#elif (defined STM32G0B0xx)

		void USART3_4_5_6_IRQHandler	(void) ;
		void USART3_4_5_6_IRQHandler	(void)

	#elif (defined STM32G0B1xx || defined STM32G0C1xx)

		void USART3_4_5_6_LPUART1_IRQHandler	(void) ;
		void USART3_4_5_6_LPUART1_IRQHandler	(void)

	#endif
	{
		#if (WITH_UART3 == 1)
		{
			UART_IRQHandler (& uart3) ;
		}
		#endif
		#if (WITH_UART4 == 1)
		{
			UART_IRQHandler (& uart4) ;
		}
		#endif
		#if (WITH_UART5 == 1)
		{
			UART_IRQHandler (& uart5) ;
		}
		#endif
		#if (WITH_UART6 == 1)
		{
			UART_IRQHandler (& uart6) ;
		}
		#endif
		#if (WITH_LPUART1 == 1)
		{
			UART_IRQHandler (& lpuart1) ;
		}
		#endif
	}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	uart parameter is :
//		The UART number from 1 to n,
//		Or the LPUART number + 100, so LPUART1 is 101


aaError_t	uartInit (uint32_t	uart, uint32_t baudrate, uint32_t flags, uartHandle_t * pHandle)
{
	uartDesc_t	* pUart ;
	IRQn_Type	irqNum = (IRQn_Type) 0 ;	// To avoid warning when no UART is selected
	uint32_t	cr2 ;

	pUart = uartSelect (uart) ;
	if (pUart == NULL)
	{
		return AA_EFAIL ;
	}
	aaIoDriverDescInit (& pUart->txList) ;
	aaIoDriverDescInit (& pUart->rxList) ;

	// Set user flags
	pUart->desc = (pUart->desc & ~UART_FLAG_MASK) | (flags & UART_FLAG_MASK) ;

	// Add internal flags
	pUart->iflags &= IFLAG_BASIC ;	// Clear all flags excepted IFLAG_BASIC
	if (desc2TxPort (pUart->desc) == 15)
	{
		// TX pin not used
		pUart->iflags |= IFLAG_NO_TX ;
	}
	if (desc2RxPort (pUart->desc) == 15)
	{
		// RX pin not used
		pUart->iflags |= IFLAG_NO_RX ;
	}

#if (WITH_UART1 == 1)
{
	if (uart == 1)
	{
		irqNum = USART1_IRQn ;

		rccEnableUartClock (USART1) ;
		rccResetUart (USART1) ;
	}
}
#endif

#if (WITH_UART2 == 1)
{
	if (uart == 2)
	{
		#if (defined LPUART2)
			irqNum = USART2_LPUART2_IRQn ;
		#else
			irqNum = USART2_IRQn ;
		#endif

		rccEnableUartClock (USART2) ;
		rccResetUart (USART2) ;
	}
}
#endif

#if (WITH_UART3 == 1)
{
	if (uart == 3)
	{
		irqNum = IRQNUM_USART_3_4_5_6_LP1 ;

		rccEnableUartClock (USART3) ;
		rccResetUart (USART3) ;
	}
}
#endif

#if (WITH_UART4 == 1)
{
	if (uart == 4)
	{
		irqNum = IRQNUM_USART_3_4_5_6_LP1 ;

		rccEnableUartClock (USART4) ;
		rccResetUart (USART4) ;
	}
}
#endif

#if (WITH_UART5 == 1)
{
	if (uart == 5)
	{
		irqNum = IRQNUM_USART_3_4_5_6_LP1 ;

		rccEnableUartClock (USART5) ;
		rccResetUart (USART5) ;
	}
}
#endif

#if (WITH_UART6 == 1)
{
	if (uart == 6)
	{
		irqNum = IRQNUM_USART_3_4_5_6_LP1 ;

		rccEnableUartClock (USART6) ;
		rccResetUart (USART6) ;
	}
}
#endif

#if (WITH_LPUART1 == 1)
{
	if (pUart->uartBase == LPUART1)
	{
		irqNum = IRQNUM_USART_3_4_5_6_LP1 ;

		rccEnableUartClock (LPUART1) ;
		rccResetUart (LPUART1) ;
	}
}
#endif


#if (WITH_LPUART2 == 1)
{
	if (pUart->uartBase == LPUART2)
	{
		irqNum = USART2_LPUART2_IRQn ;

		rccEnableUartClock (LPUART2) ;
		rccResetUart (LPUART2) ;
	}
}
#endif

	pUart->iTxRead  = 0u ;
	pUart->iTxWrite = 0u ;
	pUart->txCount  = 0u ;
	pUart->iRxRead  = 0u ;
	pUart->iRxWrite = 0u ;
	pUart->rxCount  = 0u ;

	// Configure TX and RX pins
	{
		gpioPinDesc_t	gpioDesc ;

		if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
		{
			gpioDesc.port	= (uint8_t) ('A' + desc2TxPort (pUart->desc)) ;
			gpioDesc.pin	= desc2TxPin (pUart->desc) ;
			gpioDesc.af		= desc2TxAf (pUart->desc) ;
			gpioDesc.flags	= AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL | AA_GPIO_SPEED_LOW ;
			gpioConfigurePin (& gpioDesc) ;
		}

		if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
		{
			gpioDesc.port	= (uint8_t) ('A' + desc2RxPort (pUart->desc)) ;
			gpioDesc.pin	= desc2RxPin (pUart->desc) ;
			gpioDesc.af		= (uint8_t) desc2RxAf (pUart->desc) ;
			gpioDesc.flags	= AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL | AA_GPIO_SPEED_LOW ;
			gpioConfigurePin (& gpioDesc) ;
		}
	}

	// Initialize and Enable USART

	// Set option bits
	cr2 = pUart->uartBase->CR2 ;
	if ((desc2Flags (pUart->desc) & UART_FLAG_SWAPRXTX) != 0)
	{
		cr2 |= USART_CR2_SWAP ;
	}
	if ((desc2Flags (pUart->desc) & UART_FLAG_TXINV) != 0)
	{
		cr2 |= USART_CR2_TXINV ;
	}
	if ((desc2Flags (pUart->desc) & UART_FLAG_RXINV) != 0)
	{
		cr2 |= USART_CR2_RXINV ;
	}
	if ((desc2Flags (pUart->desc) & UART_FLAG_DATAINV) != 0)
	{
		cr2 |= USART_CR2_DATAINV ;
	}
	pUart->uartBase->CR2 = cr2 ;

	// Configure
	if (IS_LPUART_INSTANCE (pUart->uartBase))
	{
		// Low power UART. Some MCU doesn't have LPUART
#if (defined LPUART1  ||  defined LPUART2)
		LL_LPUART_InitTypeDef	LPUART_InitStruct = { 0 } ;
		uint32_t				periphClk = 0 ;	// Init to avoid compiler message

		// Compute prescaler: avoid PRESC clock > 50 MHz
		if (pUart->uartBase == LPUART1)
		{
			periphClk = LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART1_CLKSOURCE);
		}
#if (defined LPUART2)
		else if (pUart->uartBase == LPUART2)
		{
			periphClk = LL_RCC_GetLPUARTClockFreq(LL_RCC_LPUART2_CLKSOURCE);
		}
#endif
		else
		{
			AA_ASSERT(0) ;	// Not supported
			return AA_EARG ;
		}
		LPUART_InitStruct.PrescalerValue =
				(periphClk > 100000000) ? LL_USART_PRESCALER_DIV8 :
				(periphClk >  50000000) ? LL_USART_PRESCALER_DIV4 :
				(periphClk >  25000000) ? LL_USART_PRESCALER_DIV2 : LL_USART_PRESCALER_DIV1 ;

		LPUART_InitStruct.BaudRate				= baudrate ;
		LPUART_InitStruct.DataWidth				= LL_LPUART_DATAWIDTH_8B ;
		LPUART_InitStruct.StopBits				= LL_LPUART_STOPBITS_1 ;
		LPUART_InitStruct.Parity				= LL_LPUART_PARITY_NONE ;
		if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
		{
			LPUART_InitStruct.TransferDirection |= LL_LPUART_DIRECTION_TX ;
		}
		if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
		{
			LPUART_InitStruct.TransferDirection |= LL_LPUART_DIRECTION_RX ;
		}
		LPUART_InitStruct.HardwareFlowControl	= LL_LPUART_HWCONTROL_NONE ;
		LL_LPUART_Init (pUart->uartBase, & LPUART_InitStruct) ;

		// LPUART doesn't have RX timeout interrupt, so can't use RX FIFO, so threshold is 1
		LL_LPUART_SetTXFIFOThreshold		(pUart->uartBase, LL_LPUART_FIFOTHRESHOLD_1_4) ;
		LL_LPUART_SetRXFIFOThreshold		(pUart->uartBase, LL_LPUART_FIFOTHRESHOLD_1_8) ;
		LL_LPUART_EnableFIFO				(pUart->uartBase) ;

		LL_LPUART_Enable (pUart->uartBase) ;
#endif
	}
	else
	{
		// High power UART or USART

		LL_USART_InitTypeDef USART_InitStruct = { 0 } ;

		USART_InitStruct.PrescalerValue			= LL_USART_PRESCALER_DIV1 ;
		USART_InitStruct.BaudRate				= baudrate ;
		USART_InitStruct.DataWidth				= LL_USART_DATAWIDTH_8B ;
		USART_InitStruct.StopBits				= LL_USART_STOPBITS_1 ;
		USART_InitStruct.Parity					= LL_USART_PARITY_NONE ;
		if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
		{
			USART_InitStruct.TransferDirection |= LL_LPUART_DIRECTION_TX ;
		}
		if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
		{
			USART_InitStruct.TransferDirection |= LL_LPUART_DIRECTION_RX ;
		}
		USART_InitStruct.HardwareFlowControl	= LL_USART_HWCONTROL_NONE ;
		USART_InitStruct.OverSampling			= LL_USART_OVERSAMPLING_16 ;
		LL_USART_Init (pUart->uartBase, & USART_InitStruct) ;

		// Default mode is async full-duplex
		// LL_USART_ConfigAsyncMode (pUart->uartBase) ;

		if (checkInternalFlag (pUart, IFLAG_BASIC) == 0)
		{
			// USART with FIFO: Set FIFOs levels and enable
			uint32_t	reg ;
			reg = pUart->uartBase->CR3 & ~(USART_CR3_TXFTCFG | USART_CR3_RXFTCFG) ;
			pUart->uartBase->CR3 = reg | (LL_USART_FIFOTHRESHOLD_7_8 << USART_CR3_TXFTCFG_Pos) | (LL_USART_FIFOTHRESHOLD_3_4 << USART_CR3_RXFTCFG_Pos) ;
			pUart->uartBase->CR1 |= USART_CR1_FIFOEN ;	// Enable FIFO

			pUart->uartBase->RTOR = 20u ;				// Timeout delay (baud clock)
			pUart->uartBase->CR2 |= USART_CR2_RTOEN ;	// Enable RX timeout
		}

		pUart->uartBase->CR1 |= USART_CR1_UE ;			// Enable USART
		// Wait for TEAK and REAK flags set
		if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
		{
			while ((pUart->uartBase->ISR & USART_ISR_REACK) != USART_ISR_REACK)
			{
			}
		}
		if (checkInternalFlag (pUart, IFLAG_NO_TX) == 0)
		{
			while ((pUart->uartBase->ISR & USART_ISR_TEACK) != USART_ISR_TEACK)
			{
			}
		}
	}

	// Configure interrupt priority
	NVIC_SetPriority (irqNum, UART_IRQ_PRIORITY) ;

	if (checkDescFlag (pUart->desc, UART_FLAG_NOIT) == 0u)
	{
		if (checkInternalFlag (pUart, IFLAG_NO_RX) == 0)
		{
			// Enable RX interrupt, don't enable TX interrupt
			if (checkInternalFlag (pUart, IFLAG_BASIC) == 0)
			{
				// Full USART and LPUART with FIFO
				pUart->uartBase->CR3 |= USART_CR3_RXFTIE ;		// RX FIFO threshold it enabled
				if (! IS_LPUART_INSTANCE (pUart->uartBase))
				{
					// Only full USART has RTOIE
					pUart->uartBase->CR1 |= USART_CR1_RTOIE ;	// RX timeout not available for LPUART
				}
			}
			else
			{
				// Basic USART without FIFO
				pUart->uartBase->CR1 |= USART_CR1_RXNEIE_RXFNEIE ;	// RXNE it enabled
			}
		}
	}

	// Enable USARTx global interrupt
	NVIC_EnableIRQ (irqNum) ;

	* pHandle = pUart ;
	return AA_ENONE ;
}

//-----------------------------------------------------------------------------
//	Warning: it is not checked that waiting lists are empty, or last character transmitted

aaError_t	uartClose			(uartHandle_t hUsart)
{
	uartDesc_t	* pUart = (uartDesc_t *) hUsart ;

	// Disable interrupts
	pUart->uartBase->CR3 &= ~(USART_CR3_RXFTIE | USART_CR3_TXFTIE) ;
	if (IS_LPUART_INSTANCE (pUart->uartBase) == 0u  &&  checkInternalFlag (pUart, IFLAG_BASIC) == 0)
	{
		pUart->uartBase->CR1 &= ~USART_CR1_RTOIE ;	// RX timeout not available for LPUART
	}

	// Disable UART
	pUart->uartBase->CR1 &= ~USART_CR1_UE ;

	return AA_ENONE ;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	To register a UART as standard input/output

void	uartInitStdDev (aaDevIo_t * pDev, uartHandle_t hUsart)
{
	pDev->hDev				= hUsart ;
	pDev->getCharPtr		= uartGetChar ;
	pDev->putCharPtr		= uartPutChar ;
	pDev->checkGetCharPtr	= uartCheckGetChar ;
	pDev->checkPutCharPtr	= uartCheckPutChar ;
}

//-----------------------------------------------------------------------------
