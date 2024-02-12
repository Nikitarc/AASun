/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	gpiobasic.c	Some routine to manage basic GPIO and RCC features

	When		Who	What
	17/03/18	ac	Creation for STM32F4xx
	16/01/20	ac	Port to H7 HAL drivers 1.6.0 ST changed the register bits and register names (BSSRH/BSSRL), sigh...
	20/03/20	ac	port to STM32L4+ (manage PWR_CR2_IOSV)
	29/04/20	ac	Add GPIOx_ASCR for L47x and L48xx
	04/03/21	ac	RCC_GetXxxClockFreq() functions are now static inside stm32l4xx_ll_rcc.c. Sigh...
	02/06/21	ac	Add AA_GPIO_MODE_ADCSWOPEN	to set analog switch open for STM32H7 PA0, PA1, PC2, PC3
	28/07/22	ac	Add return value to gpioConfigurePin() and gpioConfigureAndSetPin()
	07/05/23	ac	Port to STM32C0

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

#include	"global.h"
#include	"gpiobasic.h"

//----------------------------------------------------------------------
// Enable port clock

// portNum is 0=port A, 1=port B, etc

#if (defined STM32F4)
	__ALWAYS_STATIC_INLINE void gpioCockEnable (uint32_t portNum)
	{
		RCC->AHB1ENR |= 1u << portNum ;
		__IO uint32_t tmpreg = RCC->AHB1ENR ;
		(void) tmpreg ;
	}
#endif

#if (defined STM32L4)
	__ALWAYS_STATIC_INLINE void gpioCockEnable (uint32_t portNum)
	{
		RCC->AHB2ENR |= 1u << portNum ;
		__IO uint32_t tmpreg = RCC->AHB2ENR ;
		(void) tmpreg ;
//		LL_AHB2_GRP1_EnableClock (1u << portNum) ;
	}
#endif

#if (defined STM32H7)
	__ALWAYS_STATIC_INLINE void gpioCockEnable (uint32_t portNum)
	{
		RCC->AHB4ENR |= 1u << portNum ;
		__IO uint32_t tmpreg = RCC->AHB4ENR ;
		(void) tmpreg ;
	}
#endif

#if (defined STM32G4)
	__ALWAYS_STATIC_INLINE void gpioCockEnable (uint32_t portNum)
	{
		RCC->AHB2ENR |= 1u << portNum ;
		__IO uint32_t tmpreg = RCC->AHB2ENR ;
		(void) tmpreg ;
	}
#endif

#if (defined STM32G0  ||  defined STM32C0)
	__ALWAYS_STATIC_INLINE void gpioCockEnable (uint32_t portNum)
	{
		RCC->IOPENR |= 1u << portNum ;
		__IO uint32_t tmpreg = RCC->IOPENR ;
		(void) tmpreg ;
	}
#endif

//----------------------------------------------------------------------
// Configure one pin using its pin descriptor
// Uses a 4-byte descriptor instead of 24 bytes in the stack with LL

	uint32_t	gpioConfigureAndSetPin_ (const gpioPinDesc_t * pPinDesc, uint32_t value)
{
	GPIO_TypeDef		* pGpio ;
	uint32_t			temp ;

	if (pPinDesc->port == 0u || pPinDesc->port == '0')
	{
		// Pin not used
		return AA_EFAIL ;
	}

	//	On STM32L4+ the pins PG[15:2] are powered by VDDIO2, and need to be connected before use...
	#if (defined STM32L4  &&  defined(PWR_CR2_IOSV))
		if ((pPinDesc->port == 'G')  &&  (pPinDesc->pin > 1u))
		{
			volatile uint32_t	reg ;

			RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN ;
			reg = RCC->APB1ENR1 ;	// Delay after an RCC peripheral clock enabling
			(void) reg ;

			PWR->CR2 |= PWR_CR2_IOSV ;
		}
	#endif

	pGpio = gpioPortFromName (pPinDesc->port) ;

	// Enable port clock
	gpioCockEnable (pPinDesc->port - AA_GPIO_PORTA) ;

	// In case of Alternate function mode selection
	if ((pPinDesc->flags & AA_GPIO_MODE_MASK) == AA_GPIO_MODE_ALTERNATE)
	{
		// Check the Alternate function parameter: 0x0..0xF
		AA_ASSERT ((pPinDesc->af & 0xF0u) == 0u) ;
	}
	else
	{
		// AF must be 0 if not AA_GPIO_MODE_ALTERNATE
		AA_ASSERT ((pPinDesc->af & 0x0Fu) == 0u) ;
	}

	// In case of Output or Alternate function mode selection
	if ((pPinDesc->flags & AA_GPIO_MODE_MASK) == AA_GPIO_MODE_ALTERNATE ||
		(pPinDesc->flags & AA_GPIO_MODE_MASK) == AA_GPIO_MODE_OUTPUT)
	{
		// Configure the IO Speed
		temp = pGpio->OSPEEDR ;
		temp &= ~(GPIO_OSPEEDR_OSPEED0 << (pPinDesc->pin * 2u)) ;
		temp |= (((pPinDesc->flags & AA_GPIO_SPEED_MASK) >> 2u) << (pPinDesc->pin * 2u)) ;
		pGpio->OSPEEDR = temp ;

		// Configure the IO Output Type
		temp = pGpio->OTYPER ;
		temp &= ~(GPIO_OTYPER_OT0 << pPinDesc->pin) ;
		temp |= (((pPinDesc->flags & AA_GPIO_OPEN_DRAIN) >> 4u) << pPinDesc->pin) ;
		pGpio->OTYPER = temp ;
	}

	// Activate the Pull-up or Pull down resistor for the current IO
	// No PUPD for analog pin
	temp = pGpio->PUPDR ;
	temp &= ~(GPIO_PUPDR_PUPD0 << (pPinDesc->pin * 2u)) ;
	if ((pPinDesc->flags & AA_GPIO_MODE_MASK) != AA_GPIO_MODE_ANALOG)
	{
		temp |= (((pPinDesc->flags >> 6u) & GPIO_PUPDR_PUPD0) << (pPinDesc->pin * 2u)) ;
	}
	pGpio->PUPDR = temp ;

	// Configure Alternate function mapped with the current IO
	temp = pGpio->AFR [pPinDesc->pin >> 3u] ;
	temp &= ~((uint32_t) 0x0Fu        << ((pPinDesc->pin & 0x07u) * 4u)) ;
	temp |=  ((uint32_t) pPinDesc->af << ((pPinDesc->pin & 0x07u) * 4u)) ;
	pGpio->AFR[pPinDesc->pin >> 3u] = temp ;

	if (value != 0u)
	{
		// Set output value before direction mode
		if ((value & 1u) == 0u)
		{
			gpioPortPinReset (pGpio, 1u << pPinDesc->pin) ;
		}
		else
		{
			gpioPortPinSet (pGpio, 1u << pPinDesc->pin) ;
		}
	}

	// Configure IO Direction mode (Input, Output, Alternate or Analog)
	temp = pGpio->MODER ;
#if (defined GPIO_MODER_MODER0)
	temp &= ~(GPIO_MODER_MODER0 << (pPinDesc->pin * 2u)) ;
#else
	temp &= ~(GPIO_MODER_MODE0 << (pPinDesc->pin * 2u)) ;
#endif
	temp |= ((pPinDesc->flags & AA_GPIO_MODE_MASK) << (pPinDesc->pin * 2u)) ;
	pGpio->MODER = temp ;

#if (defined GPIO_ASCR_ASC0)
	// Special case for L47x and L48xx : enable analog switch for ADC
	if ((pPinDesc->flags & AA_GPIO_MODE_ADCSWITCH) != 0u)
	{
		pGpio->ASCR |= 1 << pPinDesc->pin ;
	}
#endif

#if (defined STM32H7)
	// Special case for some H7 : enable analog switch for direct ADC
	if (pGpio == GPIOA  &&  pPinDesc->pin == 0)
	{
		if ((pPinDesc->flags & AA_GPIO_MODE_ADCSWOPEN) != 0)
		{
			SYSCFG->PMCR |= SYSCFG_PMCR_PA0SO ;
		}
		else
		{
			SYSCFG->PMCR &= ~SYSCFG_PMCR_PA0SO ;
		}
	}
	else if (pGpio == GPIOA  &&  pPinDesc->pin == 1)
	{
		if ((pPinDesc->flags & AA_GPIO_MODE_ADCSWOPEN) != 0)
		{
			SYSCFG->PMCR |= SYSCFG_PMCR_PA1SO ;
		}
		else
		{
			SYSCFG->PMCR &= ~SYSCFG_PMCR_PA1SO ;
		}
	}
	else if (pGpio == GPIOC  &&  pPinDesc->pin == 2)
	{
		if ((pPinDesc->flags & AA_GPIO_MODE_ADCSWOPEN) != 0)
		{
			SYSCFG->PMCR |= SYSCFG_PMCR_PC2SO ;
		}
		else
		{
			SYSCFG->PMCR &= ~SYSCFG_PMCR_PC2SO ;
		}
	}
	else if (pGpio == GPIOC  &&  pPinDesc->pin == 3)
	{
		if ((pPinDesc->flags & AA_GPIO_MODE_ADCSWOPEN) != 0)
		{
			SYSCFG->PMCR |= SYSCFG_PMCR_PC3SO ;
		}
		else
		{
			SYSCFG->PMCR &= ~SYSCFG_PMCR_PC3SO ;
		}
	}
#endif
	return AA_ENONE ;
}

//----------------------------------------------------------------------
//	Set the value of the pin using its descriptor
//	Atomic

void	gpioPinSet		(const gpioPinDesc_t * pPinDesc, uint32_t bValue)
{
	GPIO_TypeDef	* pPort ;
	uint32_t		pinMask ;

	AA_ASSERT (pPinDesc->port != 0u  &&  pPinDesc->port != '0') ;

	pPort = gpioPortFromName (pPinDesc->port) ;
	pinMask = 1u << pPinDesc->pin ;

	if (bValue == 0u)
	{
		#if (BSP_HAS_BRR != 0)
			pPort->BRR = pinMask ;			// Reset bit
		#else
			pPort->BSRR = pinMask << 16u ;	// Reset bit
		#endif
	}
	else
	{
		pPort->BSRR = pinMask ;				// Set bit
	}
}

//----------------------------------------------------------------------
//	Get the value of the pin using its descriptor

uint32_t	gpioPinGet		(const gpioPinDesc_t * pPinDesc)
{
	GPIO_TypeDef	* pPort ;

	AA_ASSERT (pPinDesc->port != 0u  &&  pPinDesc->port != '0') ;

	pPort = gpioPortFromName (pPinDesc->port) ;
	return (pPort->IDR & (1u << pPinDesc->pin)) == 0u ? 0u : 1u ;
}

//----------------------------------------------------------------------
//	Toggle the value of the pin using its descriptor (atomic)

void	gpioPinToggle		(const gpioPinDesc_t * pPinDesc)
{
	GPIO_TypeDef	* pPort ;
	uint32_t		pinMask ;
	uint32_t		odr ;

	AA_ASSERT (pPinDesc->port != 0u  &&  pPinDesc->port != '0') ;

	pPort = gpioPortFromName (pPinDesc->port) ;
	pinMask	= 1u << pPinDesc->pin ;
	odr = pPort->ODR ;

	pPort->BSRR = ((odr & pinMask) << 16u) | (~odr & pinMask) ;
}

//----------------------------------------------------------------------
