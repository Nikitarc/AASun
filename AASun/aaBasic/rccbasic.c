/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	gpiobasic.c	Some routine to manage basic GPIO and RCC features

	When		Who	What
	17/03/18	ac	Creation for STM32F4xx
	04/03/21	ac	RCC_GetXxxClockFreq() functions are now static inside stm32l4xx_ll_rcc.c. Sigh...
					So create public rccGetXxxClockFreq()
	26/07/21	ac	Add rccEnableTimClock(), rccDisableTimClock, rccResetTim()
	01/02/22	ac	rccbasic split from gpiobasic

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

#include	"stm32g0xx_ll_bus.h"
#include	"stm32g0xx_ll_rcc.h"

//----------------------------------------------------------------------

#define	OFFSET_MASK		0x0000FFFFu

//----------------------------------------------------------------------
//	Some utility functions to get clocks frequencies

uint32_t	rccGetSystemClockFreq	(void)
{
	// SYSTEM clock frequency
	return SystemCoreClock ;
}

uint32_t	rccGetHCLKClockFreq		(void)
{
	// HCLK clock frequency
	return __LL_RCC_CALC_HCLK_FREQ (SystemCoreClock, LL_RCC_GetAHBPrescaler());
}

uint32_t	rccGetPCLK1ClockFreq	(void)
{
	// PCLK1 clock frequency
	return __LL_RCC_CALC_PCLK1_FREQ	(rccGetHCLKClockFreq (), LL_RCC_GetAPB1Prescaler());
}

#if defined __LL_RCC_CALC_PCLK2_FREQ
uint32_t	rccGetPCLK2ClockFreq	(void)
{
	// PCLK2 clock frequency
	return __LL_RCC_CALC_PCLK2_FREQ (rccGetHCLKClockFreq (), LL_RCC_GetAPB2Prescaler());
}
#endif

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	Some functions dedicated to timers

void		rccEnableTimClock			(void * deviceAddress)
{
	switch ((uint32_t) deviceAddress)
	{
		//------------------ Timers in register APBRSTR1
#if (defined LPTIM1_BASE)
		case LPTIM1_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_LPTIM1) ;
			break ;
#endif

#if (defined LPTIM2_BASE)
		case LPTIM2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_LPTIM2) ;
			break ;
#endif

#if (defined TIM2_BASE)
		case TIM2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_TIM2) ;
			break ;
#endif

#if (defined TIM3_BASE)
		case TIM3_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_TIM3) ;
			break ;
#endif

#if (defined TIM4_BASE)
		case TIM4_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_TIM4) ;
			break ;
#endif

#if (defined TIM6_BASE)
		case TIM6_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_TIM6) ;
			break ;
#endif

#if (defined TIM7_BASE)
		case TIM7_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_TIM7) ;
			break ;
#endif

		//------------------ Timers in register APBRSTR2
		case TIM1_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_TIM1) ;
			break ;

		case TIM14_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_TIM14) ;
			break ;

#if (defined TIM15_BASE)
		case TIM15_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_TIM15) ;
			break ;
#endif

		case TIM16_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_TIM16) ;
			break ;

		case TIM17_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_TIM17) ;
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------

void		rccDisableTimClock			(void * deviceAddress)
{
	switch ((uint32_t) deviceAddress)
	{
	//------------------ Timers in register APBRSTR1
#if (defined LPTIM1_BASE)
		case LPTIM1_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_LPTIM1) ;
			break ;
#endif

#if (defined LPTIM2_BASE)
		case LPTIM2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_LPTIM2) ;
			break ;
#endif

#if (defined TIM2_BASE)
		case TIM2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_TIM2) ;
			break ;
#endif

#if (defined TIM3_BASE)
		case TIM3_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_TIM3) ;
			break ;
#endif

#if (defined TIM4_BASE)
		case TIM4_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_TIM4) ;
			break ;
#endif

#if (defined TIM6_BASE)
		case TIM6_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_TIM6) ;
			break ;
#endif

#if (defined TIM7_BASE)
		case TIM7_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_TIM7) ;
			break ;
#endif
		//------------------ Timers in register APBRSTR2
		case TIM1_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_TIM1) ;
			break ;

		case TIM14_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_TIM14) ;
			break ;

#if (defined TIM15_BASE)
		case TIM15_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_TIM15) ;
			break ;
#endif

		case TIM16_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_TIM16) ;
			break ;

		case TIM17_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_TIM17) ;
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------

void		rccResetTim 				(void * deviceAddress)
{
	switch ((uint32_t) deviceAddress)
	{
	//------------------ Timers in register APBRSTR1
#if (defined LPTIM1_BASE)
		case LPTIM1_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_LPTIM1) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_LPTIM1) ;
			break ;
#endif

#if (defined LPTIM2_BASE)
		case LPTIM2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_LPTIM2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_LPTIM2) ;
			break ;
#endif

#if (defined TIM2_BASE)
		case TIM2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_TIM2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_TIM2) ;
			break ;
#endif

#if (defined TIM3_BASE)
		case TIM3_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_TIM3) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_TIM3) ;
			break ;
#endif

#if (defined TIM4_BASE)
		case TIM4_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_TIM4) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_TIM4) ;
			break ;
#endif

#if (defined TIM6_BASE)
		case TIM6_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_TIM6) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_TIM6) ;
			break ;
#endif

#if (defined TIM7_BASE)
		case TIM7_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_TIM7) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_TIM7) ;
			break ;
#endif
//------------------ Timers in register APBRSTR2
		case TIM1_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_TIM1) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_TIM1) ;
			break ;

		case TIM14_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_TIM14) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_TIM14) ;
			break ;

#if (defined TIM15_BASE)
		case TIM15_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_TIM15) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_TIM15) ;
			break ;
#endif

		case TIM16_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_TIM16) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_TIM16) ;
			break ;

		case TIM17_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_TIM17) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_TIM17) ;
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	Some functions dedicated to USART and LPUART

void	rccEnableUartClock (void * deviceAddress)
{
	switch ((uintptr_t) deviceAddress)
	{
		case USART1_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_USART1) ;
			break ;

		case USART2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USART2) ;
			break ;

#if (defined USART3_BASE)
		case USART3_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USART3) ;
			break ;
#endif

#if (defined USART4_BASE)
		case USART4_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USART4) ;
			break ;
#endif

#if (defined USART5_BASE)
		case USART5_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USART5) ;
			break ;
#endif

#if (defined USART6_BASE)
		case USART6_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USART6) ;
			break ;
#endif

#if (defined LPUART1_BASE)
		case LPUART1_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_LPUART1) ;
			break ;
#endif

#if (defined LPUART2_BASE)
		case LPUART2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_LPUART2) ;
			break ;
#endif

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------

void	rccDisableUartClock (void * deviceAddress)
{
	switch ((uintptr_t) deviceAddress)
	{
		case USART1_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_USART1) ;
			break ;

		case USART2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USART2) ;
			break ;

#if (defined USART3_BASE)
		case USART3_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USART3) ;
			break ;
#endif

#if (defined USART4_BASE)
		case USART4_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USART4) ;
			break ;
#endif

#if (defined USART5_BASE)
		case USART5_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USART5) ;
			break ;
#endif

#if (defined USART6_BASE)
		case USART6_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USART6) ;
			break ;
#endif

#if (defined LPUART1_BASE)
		case LPUART1_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_LPUART1) ;
			break ;
#endif

#if (defined LPUART2_BASE)
		case LPUART2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_LPUART2) ;
			break ;
#endif

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------

void	rccResetUart (void * deviceAddress)
{
	switch ((uintptr_t) deviceAddress)
	{
		case USART1_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_USART1) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_USART1) ;
			break ;

		case USART2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART2) ;
			break ;

#if (defined USART3_BASE)
		case USART3_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART3) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART3) ;
			break ;
#endif

#if (defined USART4_BASE)
		case USART4_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART4) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART4) ;
			break ;
#endif

#if (defined USART5_BASE)
		case USART5_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART5) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART5) ;
			break ;
#endif

#if (defined USART6_BASE)
		case USART6_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART6) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART6) ;
			break ;
#endif

#if (defined LPUART1_BASE)
		case LPUART1_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_LPUART1) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_LPUART1) ;
			break ;
#endif

#if (defined LPUART2_BASE)
		case LPUART2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_LPUART2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_LPUART2) ;
			break ;
#endif

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Peripheral clock enable
// Example:
// rccEnableClock (gpioPortFromName (pPinDesc->port), 1u << (pPinDesc->port - AA_GPIO_PORTA)) ;
// rccResetPeriph (SPI2, RCC_APBENR1_SPI2EN) ;
// rccResetPeriph (SPI1, RCC_APBENR2_SPI1EN) ;

// For timers enableBitMask is ignored

void	rccEnableClock (void * deviceAddress, uint32_t enableBitMask)
{
	// Only one AHB and one APB on G0
	// BUT: the syntax use APB1 and APB2 !!!! Thanks...
	if (((uintptr_t) deviceAddress & ~OFFSET_MASK) == AHBPERIPH_BASE)
	{
		/*	DMA1
			DMA2
			FLASH
			CRC
			AES
			RNG
		*/
		AA_ASSERT (enableBitMask != 0) ;
		LL_AHB1_GRP1_EnableClock (enableBitMask) ;
		return ;
	}

	if (((uintptr_t) deviceAddress ) >= IOPORT_BASE)
	{
		AA_ASSERT (enableBitMask != 0) ;
		LL_IOP_GRP1_EnableClock (enableBitMask) ;
		return ;
	}

	switch ((uintptr_t) deviceAddress)
	{
		//------------------ APB2

		case ADC1_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_ADC) ;
			break ;

		case SPI1_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_SPI1) ;
			break ;

		case SYSCFG_BASE:
			LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_SYSCFG) ;
			break ;

		//------------------ APB1
		case RTC_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_RTC) ;
			break ;

		case WWDG_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_WWDG) ;
			break ;

#if defined (FDCAN1) || defined (FDCAN2)
		case FDCAN1_BASE:
		case FDCAN2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_FDCAN) ;
			break ;
#endif

#if defined (USB_DRD_FS)
		case USB_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_USB) ;
			break ;
#endif

		case SPI2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_SPI2) ;
			break ;

#if defined (SPI3_BASE)
		case SPI3_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_SPI3) ;
			break ;
#endif

#if defined (CRS_BASE)
		case CRS_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_CRS) ;
			break ;
#endif

		case I2C1_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_I2C1) ;
			break ;

		case I2C2_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_I2C2) ;
			break ;

#if defined (I2C3_BASE)
		case I2C3_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_I2C3) ;
			break ;
#endif

#if defined (DAC1_BASE)
		case DAC1_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_DAC1) ;
			break ;
#endif

		case PWR_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_PWR) ;
			break ;

		case DBG_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_DBGMCU) ;
			break ;

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------

void	rccDisableClock (void * deviceAddress, uint32_t enableBitMask)
{
	// Only one AHB and one APB on G0
	// BUT: the syntax use APB1 and APB2 !!!! Thanks...
	if (((uintptr_t) deviceAddress & ~OFFSET_MASK) == AHBPERIPH_BASE)
	{
		/*	DMA1
			DMA2
			FLASH
			CRC
			AES
			RNG
		*/
		AA_ASSERT (enableBitMask != 0) ;
		LL_AHB1_GRP1_DisableClock (enableBitMask) ;
		return ;
	}

	if (((uintptr_t) deviceAddress ) >= IOPORT_BASE)
	{
		AA_ASSERT (enableBitMask != 0) ;
		LL_IOP_GRP1_DisableClock (enableBitMask) ;
		return ;
	}

	switch ((uintptr_t) deviceAddress)
	{
		//------------------ APB2

		case ADC1_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_ADC) ;
			break ;

		case SPI1_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_SPI1) ;
			break ;

		case SYSCFG_BASE:
			LL_APB2_GRP1_DisableClock (LL_APB2_GRP1_PERIPH_SYSCFG) ;
			break ;

		//------------------ APB1
		case RTC_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_RTC) ;
			break ;

		case WWDG_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_WWDG) ;
			break ;

#if defined (FDCAN1) || defined (FDCAN2)
		case FDCAN1_BASE:
		case FDCAN2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_FDCAN) ;
			break ;
#endif

#if defined (USB_DRD_FS)
		case USB_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_USB) ;
			break ;
#endif

		case SPI2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_SPI2) ;
			break ;

#if defined (SPI3)
		case SPI3_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_SPI3) ;
			break ;
#endif

#if defined (CRS)
		case CRS_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_CRS) ;
			break ;
#endif

		case I2C1_BASE:
			LL_APB1_GRP1_EnableClock (LL_APB1_GRP1_PERIPH_I2C1) ;
			break ;

		case I2C2_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_I2C2) ;
			break ;

#if defined (I2C3_BASE)
		case I2C3_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_I2C3) ;
			break ;
#endif

#if defined (DAC1_BASE)
		case DAC1_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_DAC1) ;
			break ;
#endif

		case PWR_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_PWR) ;
			break ;

		case DBG_BASE:
			LL_APB1_GRP1_DisableClock (LL_APB1_GRP1_PERIPH_DBGMCU) ;
			break ;

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------
// Example:
// rccResetPeriph (SPI2, RCC_APBENR1_SPI2EN) ;
// rccResetPeriph (SPI1, RCC_APBENR2_SPI1EN) ;

void	rccResetPeriph (void * deviceAddress, uint32_t enableBitMask)
{
	// Only one AHB and one APB on G0
	// BUT: the syntax use APB1 and APB2 !!!! Thanks...

	if (((uintptr_t) deviceAddress & ~OFFSET_MASK) == AHBPERIPH_BASE)
	{
		/*	DMA1
			DMA2
			FLASH
			CRC
			AES
			RNG
		*/
		AA_ASSERT (enableBitMask != 0) ;
		LL_AHB1_GRP1_ForceReset   (enableBitMask) ;
		LL_AHB1_GRP1_ReleaseReset (enableBitMask) ;
		return ;
	}

	if (((uintptr_t) deviceAddress ) >= IOPORT_BASE)
	{
		// IO ports reset: GPIOA, GPIOB...
		AA_ASSERT (enableBitMask != 0) ;
		LL_IOP_GRP1_ForceReset   (enableBitMask) ;
		LL_IOP_GRP1_ReleaseReset (enableBitMask) ;
		return ;
	}

	switch ((uintptr_t) deviceAddress)
	{
		//------------------ APB2

		case ADC1_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_ADC) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_ADC) ;
			break ;

		case SPI1_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_SPI1) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_SPI1) ;
			break ;

		case SYSCFG_BASE:
			LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_SYSCFG) ;
			LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_SYSCFG) ;
			break ;

		//------------------ APB1
		case RTC_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_RTC) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_RTC) ;
			break ;

		case WWDG_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_WWDG) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_WWDG) ;
			break ;

#if defined (FDCAN1) || defined (FDCAN2)
		case FDCAN1_BASE:
		case FDCAN2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_FDCAN) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_FDCAN) ;
			break ;
#endif

#if defined (USB_DRD_FS)
		case USB_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USB) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USB) ;
			break ;
#endif

		case SPI2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_SPI2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_SPI2) ;
			break ;

#if defined (SPI3_BASE)
		case SPI3_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_SPI3) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_SPI3) ;
			break ;
#endif

#if defined (CRS_BASE)
		case CRS_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_CRS) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_CRS) ;
			break ;
#endif

		case I2C1_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_I2C1) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_I2C1) ;
			break ;

		case I2C2_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_I2C2) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_I2C2) ;
			break ;

#if defined (I2C3_BASE)
		case I2C3_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_I2C3) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_I2C3) ;
			break ;
#endif

#if defined (DAC1_BASE)
		case DAC1_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_DAC1) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_DAC1) ;
			break ;
#endif

		case PWR_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_PWR) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_PWR) ;
			break ;

		case DBG_BASE:
			LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_DBGMCU) ;
			LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_DBGMCU) ;
			break ;

		//------------------ not managed
		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//----------------------------------------------------------------------
//	All devices except: timers, GPIO, I2S

// For timer use rccGetTimerClockFrequency()
// For I2S use  LL_RCC_GetI2SClockSource (LL_RCC_I2Sx_CLKSOURCE)

uint32_t	rccGetClockFrequency (void * deviceAddress)
{
	uint32_t		devClock ;

	switch ((uintptr_t) deviceAddress)
	{
		case SPI1_BASE:
		case SPI2_BASE:
#if (! defined LL_RCC_I2C2_CLKSOURCE)
		case I2C2_BASE:
#endif
#if (! defined RCC_CCIPR_USART2SEL)
		case USART2_BASE:
#endif
#if (! defined RCC_CCIPR_USART3SEL  && defined USART3_BASE)
		case USART3_BASE:
#endif
#if (defined USART4_BASE)
		case USART4_BASE:
#endif
#if (defined USART5_BASE)
		case USART5_BASE:
#endif
#if (defined USART6_BASE)
		case USART6_BASE:
#endif
#if (defined DAC_BASE)
		case DAC_BASE:
#endif
			// These devices are on on APB clock
			devClock = rccGetPCLK1ClockFreq () ;
			break ;

		case I2C1_BASE:
			devClock = LL_RCC_GetI2CClockFreq (LL_RCC_I2C1_CLKSOURCE) ;
			break ;

#if (defined LL_RCC_I2C2_CLKSOURCE)
		case I2C2_BASE:
			devClock = LL_RCC_GetI2CClockFreq (LL_RCC_I2C2_CLKSOURCE) ;
			break ;
#endif

		case USART1_BASE:
			devClock = LL_RCC_GetUSARTClockFreq (LL_RCC_USART1_CLKSOURCE) ;
			break ;

#if (defined RCC_CCIPR_USART2SEL)
		case USART2_BASE:
			devClock = LL_RCC_GetUSARTClockFreq (LL_RCC_USART2_CLKSOURCE) ;
			break ;
#endif

#if (defined RCC_CCIPR_USART3SEL)
		case USART3_BASE:
			devClock = LL_RCC_GetUSARTClockFreq (LL_RCC_USART3_CLKSOURCE) ;
			break ;
#endif

#if (defined LPUART1_BASE)
		case LPUART1_BASE:
			devClock = LL_RCC_GetLPUARTClockFreq (LL_RCC_LPUART1_CLKSOURCE) ;
			break ;
#endif

#if (defined LPUART2)
		case LPUART2_BASE:
			devClock = LL_RCC_GetLPUARTClockFreq (LL_RCC_LPUART2_CLKSOURCE) ;
			break ;
#endif

		case ADC1_BASE:
			devClock = LL_RCC_GetADCClockFreq (LL_RCC_ADC_CLKSOURCE) ;
			break ;

#if (defined (FDCAN1)  ||  defined (FDCAN2))
		case FDCAN1_BASE:
		case FDCAN2_BASE:
			devClock = LL_RCC_GetFDCANClockFreq (LL_RCC_FDCAN_CLKSOURCE) ;
			break ;
#endif

#if (defined CEC)
		case CEC_BASE:
			devClock = LL_RCC_GetCECClockFreq (LL_RCC_CEC_CLKSOURCE) ;
			break ;
#endif

#if (defined RNG)
		case RNG_BASE:
			devClock = LL_RCC_GetRNGClockFreq (LL_RCC_RNG_CLKSOURCE) ;
			break ;
#endif

		default:
			devClock = 0u ;
			break ;
	}

	AA_ASSERT (devClock != 0u) ;	// Clock source not started
	return devClock ;
}

//----------------------------------------------------------------------
//	Get TIMER and LPTIMER clock frequency
//	Timer clock depends on APB bus prescaler value
//	Timer clock may be APBx peripherals frequency x2

uint32_t	rccGetTimerClockFrequency (void * deviceAddress)
{
	uint32_t	prescaler ;
	uint32_t	timClock ;

	(void) deviceAddress ;	// Some MCUs have all timers on PCLK1

#if (defined LPTIM1)
	if (deviceAddress == LPTIM1)
	{
		timClock = LL_RCC_GetLPTIMClockFreq (LL_RCC_LPTIM1_CLKSOURCE) ;
	}
	else
#endif

#if (defined LPTIM2)
	if (deviceAddress == LPTIM2)
	{
		timClock = LL_RCC_GetLPTIMClockFreq (LL_RCC_LPTIM2_CLKSOURCE) ;
	}
	else
#endif

#if (defined RCC_CCIPR_TIM1SEL)
	if (deviceAddress == TIM1)
	{
		timClock = LL_RCC_GetTIMClockFreq (LL_RCC_TIM1_CLKSOURCE) ;
	}
	else
#endif

#if (defined RCC_CCIPR_TIM15SEL)
	if (deviceAddress == TIM15)
	{
		timClock = LL_RCC_GetTIMClockFreq (LL_RCC_TIM15_CLKSOURCE) ;
	}
	else
#endif

	{
		// All other timers are on PCLK1
		timClock = rccGetPCLK1ClockFreq () ;		// timClock is PCLK1
		prescaler = LL_RCC_GetAPB1Prescaler () ;
		if (prescaler != 0u)
		{
			timClock *= 2u ;
		}
	}

	AA_ASSERT (timClock != 0u) ;	// Clock source not started

	return timClock ;
}

//----------------------------------------------------------------------
