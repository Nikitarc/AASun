/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	gpiobasic.h	Some routine to manage basic GPIO and RCC features

	When		Who	What
	17/03/18	ac	Creation
	29/04/20	ac	Add GPIOx_ASCR for L47x and L48xx
	14/05/21	ac	Add gpioConfigureAndSetPin()
	02/06/21	ac	Add AA_GPIO_MODE_ADCSWOPEN	to set analog switch open for STM32H7 PA0, PA1, PC2, PC3
	01/02/22	ac	Move rcc stuff to new rccbasic.h
	28/07/22	ac	Add return value to gpioConfigurePin() and gpioConfigureAndSetPin()

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

#if ! defined GPIOBASIC_H_
#define	GPIOBASIC_H_
//----------------------------------------------------------------------

typedef struct gpioPinDesc_s
{
	uint8_t		port ;	//	'A', 'B', ... Unused pin: 0 or '0'
	uint8_t		pin ;	//	0 to 15
	uint8_t		af ;	//	AA_GPIO_AF_n
	uint8_t		flags ;
	
} gpioPinDesc_t ;

//	To specify PGIO port
#define AA_GPIO_PORTA				((uint8_t)'A')
#define AA_GPIO_PORTB				((uint8_t)'B')
#define AA_GPIO_PORTC				((uint8_t)'C')
#define AA_GPIO_PORTD				((uint8_t)'D')
#define AA_GPIO_PORTE				((uint8_t)'E')
#define AA_GPIO_PORTF				((uint8_t)'F')
#define AA_GPIO_PORTG				((uint8_t)'G')
#define AA_GPIO_PORTH				((uint8_t)'H')
#define AA_GPIO_PORTI				((uint8_t)'I')
#define AA_GPIO_PORTJ				((uint8_t)'J')
#define AA_GPIO_PORTK				((uint8_t)'K')

// To specify PIN alternate function
#define AA_GPIO_AF_0                (0x0000000u)
#define AA_GPIO_AF_1                (0x0000001u)
#define AA_GPIO_AF_2                (0x0000002u)
#define AA_GPIO_AF_3                (0x0000003u)
#define AA_GPIO_AF_4                (0x0000004u)
#define AA_GPIO_AF_5                (0x0000005u)
#define AA_GPIO_AF_6                (0x0000006u)
#define AA_GPIO_AF_7                (0x0000007u)
#define AA_GPIO_AF_8                (0x0000008u)
#define AA_GPIO_AF_9                (0x0000009u)
#define AA_GPIO_AF_10               (0x000000Au)
#define AA_GPIO_AF_11               (0x000000Bu)
#define AA_GPIO_AF_12               (0x000000Cu)
#define AA_GPIO_AF_13               (0x000000Du)
#define AA_GPIO_AF_14               (0x000000Eu)
#define AA_GPIO_AF_15               (0x000000Fu)

// gpioPinDesc_t.flags bits definitions

// If GPIO_MODE_ALTERNATE is selected then 'af' field must be filled, else must be 0
#define AA_GPIO_MODE_MASK			0x03u	//
#define AA_GPIO_MODE_INPUT			0x00u	//	Select input mode (default mode, floating)
#define AA_GPIO_MODE_OUTPUT			0x01u	//	Select output mode (default PP)
#define AA_GPIO_MODE_ALTERNATE		0x02u	//	Select alternate function mode
#define AA_GPIO_MODE_ANALOG			0x03u	//	Select analog mode

#if defined(STM32H7)
	#define AA_GPIO_MODE_ADCSWOPEN	0x20u	//	Set analog switch open for STM32H7 PA0, PA1, PC2, PC3
#else
	#define AA_GPIO_MODE_ADCSWOPEN	0x00u
#endif

#if defined(GPIO_ASCR_ASC0)
	#define AA_GPIO_MODE_ADCSWITCH	0x20u	//	Set analog switch if L47x and L48xx
#else
	#define AA_GPIO_MODE_ADCSWITCH	0x00u
#endif

#define AA_GPIO_SPEED_LOW			0x00u	//	Select I/O low output speed
#define AA_GPIO_SPEED_MEDIUM		0x04u	//	Select I/O medium output speed
#define AA_GPIO_SPEED_HIGH			0x08u	//	Select I/O fast output speed
#define AA_GPIO_SPEED_VERYHIGH		0x0Cu	//	Select I/O high output speed
#define AA_GPIO_SPEED_MASK			0x0Cu

#define AA_GPIO_PUSH_PULL			0x00u
#define AA_GPIO_OPEN_DRAIN			0x10u	//	Only if output or alternate

#define	AA_GPIO_PULL_NO				0x00u
#define AA_GPIO_PULL_UP				0x40u	//	Not for analog
#define AA_GPIO_PULL_DN				0x80u

// GPIO mode shortcuts
#define AA_GPIO_MODE_ADC			(AA_GPIO_MODE_ANALOG | AA_GPIO_MODE_ADCSWITCH)	//	Configure for ADC input
#define AA_GPIO_MODE_ADC_OPEN		(AA_GPIO_MODE_ANALOG | AA_GPIO_MODE_ADCSWOPEN)	//	Configure for ADC input with open switch

#define AA_GPIO_MODE_OUTPUT_PP		(AA_GPIO_MODE_OUTPUT)		// Output Push_Pull
#define AA_GPIO_MODE_OUTPUT_PP_UP	(AA_GPIO_MODE_OUTPUT | AA_GPIO_PULL_UP)
#define AA_GPIO_MODE_OUTPUT_PP_DN	(AA_GPIO_MODE_OUTPUT | AA_GPIO_PULL_DN)

#define AA_GPIO_MODE_OUTPUT_OD		(AA_GPIO_MODE_OUTPUT | AA_GPIO_OPEN_DRAIN)		// Output Open Drain
#define AA_GPIO_MODE_OUTPUT_OD_UP	(AA_GPIO_MODE_OUTPUT | AA_GPIO_OPEN_DRAIN | AA_GPIO_PULL_UP)
#define AA_GPIO_MODE_OUTPUT_OD_DN	(AA_GPIO_MODE_OUTPUT | AA_GPIO_OPEN_DRAIN | AA_GPIO_PULL_DN)

#define AA_GPIO_MODE_INPUT_FLOAT	(AA_GPIO_MODE_INPUT)
#define AA_GPIO_MODE_INPUT_UP		(AA_GPIO_MODE_INPUT	| AA_GPIO_PULL_UP)
#define AA_GPIO_MODE_INPUT_DN		(AA_GPIO_MODE_INPUT	| AA_GPIO_PULL_DN)

#define AA_GPIO_MODE_ALT_PP			(AA_GPIO_MODE_ALTERNATE)	// Alternate Push_Pull
#define AA_GPIO_MODE_ALT_PP_UP		(AA_GPIO_MODE_ALTERNATE | AA_GPIO_PULL_UP)
#define AA_GPIO_MODE_ALT_PP_DN		(AA_GPIO_MODE_ALTERNATE | AA_GPIO_PULL_DN)

#define AA_GPIO_MODE_ALT_OD			(AA_GPIO_MODE_ALTERNATE | AA_GPIO_OPEN_DRAIN)		// Output Open Drain
#define AA_GPIO_MODE_ALT_OD_UP		(AA_GPIO_MODE_ALTERNATE | AA_GPIO_OPEN_DRAIN | AA_GPIO_PULL_UP)
#define AA_GPIO_MODE_ALT_OD_DN		(AA_GPIO_MODE_ALTERNATE | AA_GPIO_OPEN_DRAIN | AA_GPIO_PULL_DN)

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Get a pointer to port registers from port name ('A', 'B', ..)
#define gpioPortFromName(portName)		((GPIO_TypeDef *)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*((uint32_t)portName-(uint32_t)AA_GPIO_PORTA)))

// Get a pointer to port registers from port number (0, 1, ..)
#define gpioPortFromNum(portNum)		((GPIO_TypeDef *)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*((uint32_t)portNum)))

uint32_t	gpioConfigureAndSetPin_	(const gpioPinDesc_t * pPinDesc, uint32_t value) ;		// Libray Internal use only

// Set the pin value before set pin mode: avoid glitch if there is external pull up/down
// The pin value must be set after the port clock is enabled and before the mode is set.
__ALWAYS_STATIC_INLINE uint32_t	gpioConfigureAndSetPin	(const gpioPinDesc_t * pPinDesc, uint32_t value)
{
	return gpioConfigureAndSetPin_ (pPinDesc, 0x8000u | value) ;
}

__ALWAYS_STATIC_INLINE uint32_t	gpioConfigurePin		(const gpioPinDesc_t * pPinDesc)
{
	return gpioConfigureAndSetPin_ (pPinDesc, 0) ;
}

void		gpioPinSet					(const gpioPinDesc_t * pPinDesc, uint32_t bValue) ;
uint32_t	gpioPinGet					(const gpioPinDesc_t * pPinDesc) ;
void		gpioPinToggle				(const gpioPinDesc_t * pPinDesc) ;

// To set multiple bits at the same time. Pins parameter is a bit mask of the pins to set.
__ALWAYS_STATIC_INLINE void	gpioPortPinSet			(GPIO_TypeDef * pPort, uint32_t pins)
{
	pPort->BSRR = pins ;
}

// To clear multiple bits at the same time. Pins parameter is a bit mask of the pins to reset.
__ALWAYS_STATIC_INLINE void	gpioPortPinReset		(GPIO_TypeDef * pPort, uint32_t pins)
{
	#if (BSP_HAS_BRR != 0)
		pPort->BRR = pins ;
	#else
		pPort->BSRR = pins << 16u ;
	#endif
}

// To toggle multiple bits at the same time. Pins parameter is a bit mask of the pins to toggle.
__ALWAYS_STATIC_INLINE void	gpioPortPinToggle		(GPIO_TypeDef * pPort, uint32_t pins)
{
	uint32_t		odr ;

	odr = pPort->ODR ;
	pPort->BSRR = ((odr & pins) << 16u) | (~odr & pins) ;
}

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------
#endif	// GPIOBASIC_H_
