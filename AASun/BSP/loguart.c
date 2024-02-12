/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	loguart.c		UART management for aaLogMes
					ITM not available for Cortex-M0(+) core

	When		Who	What
	26/01/21	ac	Creation
	08/09/21	ac	Use AA_WITH_LOGMES for conditional code


	Use LL driver for initialization (It's easier)
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

#include	"aa.h"
#include	"gpiobasic.h"

#include	"stm32g0xx_ll_usart.h"

// Use the UART Tx only, polling mode (no interrupt)
// Use the FIFO if available
// The USART clock source must have been configured previously, in the SystemClock_Config.c file.

// It is recommended to use a high baud rate (e.g 8 MHz)
// Beware: the FTDI C232HM does not support the baud rates of 7 Mbaud, 9 Mbaud, 10 Mbaud and 11 Mbaud.

#define	LOG_UART		USART3
#define	LOG_BAUDRATE	8000000

#define	LOG_TXPORT		'B'
#define	LOG_TXPIN		10
#define	LOG_TXAF		4

#if (AA_WITH_LOGMES == 1)
//-----------------------------------------------------------------------------

void	bspLogUartInit (void)
{
	{
		// Configure the TX pin
		{
			gpioPinDesc_t	gpioDesc ;

			gpioDesc.port	= LOG_TXPORT ;
			gpioDesc.pin	= LOG_TXPIN ;
			gpioDesc.af		= LOG_TXAF ;
			gpioDesc.flags	= AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH ;
			gpioConfigurePin (& gpioDesc) ;
		}

		// Configure and start the USART
		{
			LL_USART_InitTypeDef USART_InitStruct ;

			rccEnableUartClock (LOG_UART) ;

			USART_InitStruct.PrescalerValue			= LL_USART_PRESCALER_DIV1 ;
			USART_InitStruct.BaudRate				= LOG_BAUDRATE ;
			USART_InitStruct.DataWidth				= LL_USART_DATAWIDTH_8B ;
			USART_InitStruct.StopBits				= LL_USART_STOPBITS_1 ;
			USART_InitStruct.Parity					= LL_USART_PARITY_NONE ;
			USART_InitStruct.TransferDirection		= LL_USART_DIRECTION_TX ;
			USART_InitStruct.HardwareFlowControl	= LL_USART_HWCONTROL_NONE ;
			USART_InitStruct.OverSampling			= LL_USART_OVERSAMPLING_8 ;
			LL_USART_Init (LOG_UART, & USART_InitStruct) ;
			LL_USART_ConfigAsyncMode (LOG_UART) ;

			if (IS_UART_FIFO_INSTANCE (LOG_UART))
			{
				// USART with FIFO
				LL_USART_SetTXFIFOThreshold		(LOG_UART, LL_USART_FIFOTHRESHOLD_1_4) ;
				LL_USART_EnableFIFO				(LOG_UART) ;
			}

			LL_USART_Enable (LOG_UART) ;
			while (! (LL_USART_IsActiveFlag_TEACK (LOG_UART)))
			{
			}
		}
	}
}

//-----------------------------------------------------------------------------

void	bspLogUartPut	(uintptr_t dev, char value)
{
	(void) dev ;

	while ((LOG_UART->ISR & USART_ISR_TXE_TXFNF) == 0u)
	{
	}
	LOG_UART->TDR = (uint16_t) value ;
}

//-----------------------------------------------------------------------------
#endif  // AA_WITH_LOGMES
