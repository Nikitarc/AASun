/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	bsp.h		Target / compiler specific "C" and "asm" routines
				For STM32F1/F4/L4/H7/G0 and GCC

	When		Who	What
	30/12/13	ac	Creation
	07/12/19	ac	bspToggleOutput() is atomic
					BSP_IRQPRIOMAX_MINUS robustness improvement
	20/04/20	ac	Unify fort to all F4 L4+ and H7 families
	08/05/20	ac	Add bit banding macros (Except for M7)
	14/01/20	ac	Add  support for M0+. Become specific to Cortex-M0(+)
	13/05/21	ac	BSP_ATTR_ALLWAYS_INLINE changed (add static inline)
	17/01/22	ac	Add BSP_ATTR_WEAK to bspSystemInit_() and bspHardwareInit_()
	27/01/22	ac	Add bspMainStackCheck()
	09/06/22	ac	add BSP_ATTR_ALIGN(x)

	This file contain functions declarations definitions and typedef for use by applications
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

#if ! defined AABSP_H_
#define AABSP_H_
//--------------------------------------------------------------------------------

#if (defined STM32G050xx || defined STM32G051xx  ||	\
	 defined STM32G061xx || defined STM32G070xx  || defined STM32G071xx	|| defined STM32G081xx	||	\
	 defined STM32G0B0xx || defined STM32G0B1xx  || defined STM32G0C1xx)
	#include	<stm32g0xx.h>

#else
	#error "STM32 model not defined"
#endif

// Some MCU families GPIO has a BRR register, but the way to find out is not provided!
// BSP_HAS_BRR is not 0 if the BRR register is available
#define	BSP_HAS_BRR		(GPIO_BRR_BR0 == 1  ||  GPIO_BRR_BR_0 == 1)

#include	"bspboard.h"

//--------------------------------------------------------------------------------
//	Some useful attribute definitions

#define	BSP_ATTR_USED				__attribute__ ((used))
#define	BSP_ATTR_WEAK				__attribute__ ((weak))
#define	BSP_ATTR_NAKED				__attribute__ ((naked))
#define	BSP_ATTR_NORETURN			__attribute__ ((noreturn))
#define	BSP_ATTR_ALLWAYS_INLINE		__attribute__ ((always_inline))  static inline
#define	BSP_ATTR_NOINIT				__attribute__ ((section(".noinit")))
#define	BSP_ATTR_ALIGN(x)			__attribute__((aligned(x)))

// Other inline keywords are defined in core_cm4.h or core_cm7.h
#if defined(__CC_ARM)

	#define __ALWAYS_INLINE			__attribute__ ((always_inline))
	#define __ALWAYS_STATIC_INLINE	__attribute__ ((always_inline)) static inline
	#define __NEVER_INLINE			__attribute__ ((noinline))

#elif defined ( __GNUC__ )

	#define __ALWAYS_INLINE			__attribute__ ((always_inline))
	#define __ALWAYS_STATIC_INLINE	__attribute__ ((always_inline)) static inline
	#define __NEVER_INLINE			__attribute__ ((noinline))

#else

	#error Compiler not supported

#endif

//--------------------------------------------------------------------------------
//	BSP configuration

#if ! defined (NULL)
	#ifdef __cplusplus
		#define	NULL	0
	#else
		#define	NULL	((void *) 0)
	#endif
#endif

typedef	uint32_t			bspBaseType ;		// Natural word size of the processor arch
typedef uint32_t			bspStackType_t ;	// Size of stack words. For windows we put a HANDLE in the stack


// Max and min usable interruption priority, value non shifted
// Priorities 0..BSP_MAX_INT_PRIO-1 are never disabled, therefore theses interrupts are "zero latency"
// Remember: the lower the numerical value, the higher the priority (or urgency). Zero is the highest priority level
// In the BASEPRI register the priority value is shifted to high bits:
//   If the NVIC have 4 priority bits (__NVIC_PRIO_BITS == 4),
//   then the values 0, 1..15 are written as 0, 0x10..0xF0 in BASEPRI register
//   A value of 0 doesn't disable any interruption!!!

// No BASEPRI on Cortex-M0 => no zero latency interrupts, so BSP_MAX_INT_PRIO is 0
#define	BSP_MAX_INT_PRIO	0u
#define	BSP_MIN_INT_PRIO	((1u << __NVIC_PRIO_BITS) - 1u)		// 0x03 if __NVIC_PRIO_BITS is 2

// Macros to define interrupts priorities
// If the argument is a constant, the macros can be used as initializer
// To use in code the functions versions bspIrqPrioMinPlus() and bspIrqPrioMaxMinus() are preferable to avoid large expansion
#define 	BSP_IRQPRIOMAX_MINUS(offset)			\
				(((BSP_MAX_INT_PRIO + (offset)) > BSP_MIN_INT_PRIO)	\
					? BSP_MIN_INT_PRIO				\
					: (BSP_MAX_INT_PRIO + (offset)))

#define 	BSP_IRQPRIOMIN_PLUS(offset)				\
				(((BSP_MIN_INT_PRIO - (offset)) > BSP_MIN_INT_PRIO)	\
					? BSP_MAX_INT_PRIO				\
					: (BSP_MIN_INT_PRIO - (offset)))

//--------------------------------------------------------------------------------
// Some useful GCC compiler intrinsics

#define	aaVA_LIST	__builtin_va_list
#define	aaVA_START	__builtin_va_start
#define	aaVA_END	__builtin_va_end
#define	aaVA_ARG	__builtin_va_arg
#define	aaISDIGIT	__builtin_isdigit
#define	aaSTRLEN	__builtin_strlen
#define	aaSTRCMP	__builtin_strcmp
#define	aaSTRNCMP	__builtin_strncmp
#define	aaSTRCPY	__builtin_strcpy
#define	aaMEMCPY	__builtin_memcpy
#define	aaMEMSET	__builtin_memset

//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------
//	Functions equivalent to BSP_IRQPRIOMAX_MINUS and BSP_IRQPRIOMIN_PLUS

uint32_t		bspIrqPrioMinPlus		(uint32_t offset) ;
uint32_t		bspIrqPrioMaxMinus		(uint32_t offset) ;

//--------------------------------------------------------------------------------
//	Debug breakpoint

#if defined (DEBUG)
	#if defined(__CC_ARM)
		#define BSP_DEBUG_BKPT()	__breakpoint(0)
	#else
		#define BSP_DEBUG_BKPT()	__ASM volatile ("bkpt 0")
	#endif
#else
	#define BSP_DEBUG_BKPT()
#endif

//--------------------------------------------------------------------------------

// Usage example: bspOutput (BSP_LED0, 1) or bspOutput (BSP_LED0, 0)
// If the parameters are constants, the generated code is 1 or 2 assembly instructions
// This is an atomic function
__STATIC_INLINE void bspOutput (uint32_t io, uint32_t value)
{
	GPIO_TypeDef	* GPIOx = BSP_IO_GPIO (io >> 4u) ;
	uint32_t		pin	= 1u << (io & 0x0Fu) ;

	if (value == 0u)
	{
		#if (BSP_HAS_BRR != 0)
			GPIOx->BRR = pin ;			// F1/L4/G0/L0 Reset bit
		#else
			GPIOx->BSRR = pin << 16u ;	// F4/H7 Reset bit
		#endif
	}
	else
	{
		GPIOx->BSRR = pin ;				// Set bit
	}
}

// This is an atomic function
__STATIC_INLINE void bspToggleOutput (uint32_t io)
{
	GPIO_TypeDef	* GPIOx = BSP_IO_GPIO (io >> 4u) ;
	uint32_t		pinMask	= 1u << (io & 0x0Fu) ;
	uint32_t		odr = GPIOx->ODR ;

	GPIOx->BSRR = ((odr & pinMask) << 16u) | (~odr & pinMask) ;
}

// Returns 0 if input is low, and 1 if input is high
// Usage example: x = bspInput (BSP_BUTTON0)
__STATIC_INLINE uint32_t bspInput (uint32_t io)
{
	return (BSP_IO_GPIO (io >> 4u)->IDR & (1u << (io & 0x0Fu))) == 0u ? 0u : 1u ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Get a time stamp or duration
//	The time stamp is the value of the Cortex cycle counter CYCCNT
//	No CYCCNT on M0/M0+ core
//	The time stamp functions are inlined for performance reason

// Parameters for the time stamp package and bspDelayUs()

// Define if the time stamp package must be available
// If no timer is available, set WITH_BSPTS to 0
#define	WITH_BSPTS		1

// Define the timer to use for the time stamp (a 16 bits timer)
#define	BSPTS_TIMER		TIM16

// Define the timer clock frequency
// Better to choose a power of 2: faster divide
#define	BSPTS_FREQ		4000000u					// Hz
#define	BSPTS_MHZ		((BSPTS_FREQ)/1000000u)		// MHz

//--------------------------------------------------------------------------------
//	Get a time stamp

__STATIC_INLINE  uint32_t	bspTsGet (void)
{
	#if (WITH_BSPTS == 1)
		return BSPTS_TIMER->CNT ;
	#else
		AA_ASSERT (0) ;		// Time stamp not configured
		return 0 ;
	#endif
}

//--------------------------------------------------------------------------------
//	Compute time difference between now and the time stamp value pointed to by pTs
//	Store now at the place pointed by pTs (now is a raw value)
//	Return difference value in us

__STATIC_INLINE uint32_t	bspTsDelta	(uint32_t * pTs)
{
	#if (WITH_BSPTS == 1)
		uint32_t	ts ;
		uint32_t	delta ;

		ts = BSPTS_TIMER->CNT ;

		delta = (ts - * pTs) & 0xFFFFu ;
		delta /= BSPTS_MHZ ;		// Convert delta to us

		* pTs = ts ;
		return delta ;
	#else
		(void) pTs ;
		AA_ASSERT (0) ;		// Time stamp not configured
		return 0 ;
	#endif
}

//--------------------------------------------------------------------------------
//	Compute time difference between now and the time stamp value pointed to by pTs
//	Store now at the place pointed by pTs (now is a raw value)
//	Return difference as a raw value (not converted  to us)

__STATIC_INLINE uint32_t	bspRawTsDelta	(uint32_t * pTs)
{
	#if (WITH_BSPTS == 1)
		uint32_t	ts ;
		uint32_t	delta ;

		ts = BSPTS_TIMER->CNT ;

		delta = (ts - * pTs) & 0xFFFFu ;
		* pTs = ts ;
		return delta ;
	#else
		(void) pTs ;
		AA_ASSERT (0) ;		// Time stamp not configured
		return 0 ;
	#endif
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Public BSP core functions for application use

uint32_t		bspMainStackCheck		(void)  ;

uint32_t		bspGetTickRate			(void) ;
aaError_t		bspSetTickRate			(uint32_t tickHz) ;
uint32_t		bspGetSysClock 			(void) ;
void			bspDelayUs				(uint32_t us) ;

void BSP_ATTR_WEAK	BSP_ATTR_NORETURN bspResetHardware (void) ;
void BSP_ATTR_WEAK	bspSystemInit_		(void) ;
void BSP_ATTR_WEAK	bspHardwareInit_ 	(void) ;

// Kernel use only
void			bspBanner_ 				(void) ;
void			bspBoardInit_ 			(void) ;
void			bspTickStretchInit_ 	(uint32_t tickHz) ;
void			bspTickStretch_ 		(void) ;

void			bspJumpToBootLoader		(void) ;

//--------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// AABSP_H_
