/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	Devices vector table for GCC based toolchain:
	STM32G050-051-061-070-071-081-0B0-0B1-0C1

	Exception vectors and default handlers for Cortex-M0+, Cortex-M3, Cortex-M4, Cortex-M7
	Interrupts vector and default handlers

	After Reset the Cortex-M processor is in Thread mode,
	priority is Privileged, and the stack is set to Main.

	When		Who	What
	13/01/21	ac	Creation
	04/11/21	ac	Rework of MCU dependent  #if
	29/07/22	ac	Add "exception = __get_IPSR ()" in Default_Handler
	20/01/23	ac Error of TIM7_IRQHandler() declaration for G070
	15/09/23	ac	Add call to aaSpuriousInterruptHandler() in Default_Handler()


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
	__ARM_ARCH_6M__		Set to 1 when generating code for Armv6-M (Cortex-M0, Cortex-M0+, Cortex-M1)
	__ARM_ARCH_7M__		Set to 1 when generating code for Armv7-M (Cortex-M3)
	__ARM_ARCH_7EM__	Set to 1 when generating code for Armv7-M (Cortex-M4, Cortex-M7) with FPU
*/

#include	<stm32g0xx.h>

#if defined (DEBUG)
	#if defined(__CC_ARM)
		#define __DEBUG_BKPT()	__breakpoint(0)
	#else
		#define __DEBUG_BKPT()	__ASM volatile ("bkpt 0")
	#endif
#else
	#define __DEBUG_BKPT()	while (1) { }
#endif

extern void __attribute__((noreturn)) start_ (void);	// The entry point in startup.c

// ----------------------------------------------------------------------------
// Exception Stack Frame of the Cortex-M0-M0+-M3-M4-M7 processors.

typedef struct exceptionStackFrame_s
{
	uint32_t	r0 ;
	uint32_t	r1 ;
	uint32_t	r2 ;
	uint32_t	r3 ;
	uint32_t	r12 ;
	uint32_t	lr ;
	uint32_t	pc ;
	uint32_t	psr ;
	#if  defined(__ARM_ARCH_7EM__)
		uint32_t s[16];				// Floating point registers
	#endif

} exceptionStackFrame_t ;

void	  HardFault_Handler_C (exceptionStackFrame_t * pFrame, uint32_t lr) ;

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

	void	  UsageFault_Handler_C	(exceptionStackFrame_t * pFrame, uint32_t lr) ;
	void	  BusFault_Handler_C	(exceptionStackFrame_t * pFrame, uint32_t lr) ;

#endif // defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

// ----------------------------------------------------------------------------
// Forward declarations of exception handlers
// There is specific default handlers for these exception at the end of the file

extern void		Reset_Handler		(void) ;
extern void		NMI_Handler			(void) ;
extern void		HardFault_Handler	(void) ;

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
	extern void	MemManage_Handler	(void) ;
	extern void	BusFault_Handler	(void) ;
	extern void	UsageFault_Handler	(void) ;
	extern void	DebugMon_Handler	(void) ;
#endif

extern void		SVC_Handler			(void) ;
extern void		PendSV_Handler		(void) ;
extern void		SysTick_Handler		(void) ;

// Generic default handler for interruptions
void __attribute__((weak))	Default_Handler				(void) ;
void						aaSpuriousInterruptHandler	(uint32_t exception) ;

// ----------------------------------------------------------------------------
// Forward declaration of the IRQ handlers with weak definitions.
// The application can defines a handler with the same name and take precedence

void __attribute__ ((weak, alias ("Default_Handler"))) WWDG_IRQHandler					(void) ;

#if (defined PWR_PVD_SUPPORT)
	#if (defined PWR_CR2_IOSV)
		void __attribute__ ((weak, alias ("Default_Handler"))) PVD_VDDIO2_IRQHandler	(void) ;
	#else
		void __attribute__ ((weak, alias ("Default_Handler"))) PVD_IRQHandler			(void) ;
	#endif
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) RTC_TAMP_IRQHandler				(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) FLASH_IRQHandler					(void) ;

#if (defined CRS)
	void __attribute__ ((weak, alias ("Default_Handler"))) RCC_CRS_IRQHandler			(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) RCC_IRQHandler				(void) ;
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) EXTI0_1_IRQHandler				(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) EXTI2_3_IRQHandler				(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) EXTI4_15_IRQHandler				(void) ;

#if (defined USB_BASE)
	#if (defined UCPD1)
		void __attribute__ ((weak, alias ("Default_Handler"))) USB_UCPD1_2_IRQHandler		(void) ;
	#else
		void __attribute__ ((weak, alias ("Default_Handler"))) USB_IRQHandler				(void) ;
	#endif
#else
	#if (defined UCPD1)
		void __attribute__ ((weak, alias ("Default_Handler"))) UCPD1_2_IRQHandler			(void) ;
	#endif
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) DMA1_Channel1_IRQHandler			(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) DMA1_Channel2_3_IRQHandler		(void) ;

#if (defined DMA2)
	void __attribute__ ((weak, alias ("Default_Handler"))) DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler(void) ;
#endif

#if (defined COMP)
	void __attribute__ ((weak, alias ("Default_Handler"))) ADC1_COMP_IRQHandler			(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) ADC1_IRQHandler				(void) ;
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) TIM1_BRK_UP_TRG_COM_IRQHandler	(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) TIM1_CC_IRQHandler				(void) ;

#if (defined TIM2)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM2_IRQHandler				(void) ;
#endif

#if (defined TIM4)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM3_TIM4_IRQHandler			(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM3_IRQHandler				(void) ;
#endif

#if (defined DAC1)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM6_DAC_LPTIM1_IRQHandler	(void) ;
#elif (defined TIM6)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM6_IRQHandler				(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) LPTIM1_IRQHandler			(void) ;
#endif

#if (defined LPTIM2  &&  defined TIM7)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM7_LPTIM2_IRQHandler		(void) ;
#elif (defined TIM7)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM7_IRQHandler				(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) LPTIM2_IRQHandler			(void) ;
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) TIM14_IRQHandler					(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) TIM15_IRQHandler					(void) ;

#if (defined FDCAN1)
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM16_FDCAN_IT0_IRQHandler	(void) ;
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM17_FDCAN_IT1_IRQHandler	(void) ;
#else
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM16_IRQHandler				(void) ;
	void __attribute__ ((weak, alias ("Default_Handler"))) TIM17_IRQHandler				(void) ;
#endif

void __attribute__ ((weak, alias ("Default_Handler"))) I2C1_IRQHandler					(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) I2C2_IRQHandler					(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) SPI1_IRQHandler					(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) SPI2_IRQHandler					(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) USART1_IRQHandler				(void) ;
void __attribute__ ((weak, alias ("Default_Handler"))) USART2_IRQHandler				(void) ;

#if (defined LPUART1  &&  !defined USART3  &&  !defined USART5)
	void __attribute__ ((weak, alias ("Default_Handler"))) LPUART1_IRQHandler			(void) ;

#elif (!defined LPUART1  &&  defined USART3  &&  !defined USART5)
	void __attribute__ ((weak, alias ("Default_Handler"))) USART3_4_IRQHandler			(void) ;

#elif (!defined LPUART1  &&  defined USART3  &&  defined USART5)
	void __attribute__ ((weak, alias ("Default_Handler"))) USART3_4_5_6_IRQHandler		(void) ;

#elif (defined LPUART1  &&  defined USART3  &&  !defined USART5)
	void __attribute__ ((weak, alias ("Default_Handler"))) USART3_4_LPUART1_IRQHandler	(void) ;

#elif (defined LPUART1  &&  defined USART3  &&  defined USART5)
	void __attribute__ ((weak, alias ("Default_Handler"))) USART3_4_5_6_LPUART1_IRQHandler	(void) ;
#endif

#if (defined CEC)
	void __attribute__ ((weak, alias ("Default_Handler"))) CEC_IRQHandler				(void) ;
#endif

#if (defined AES)
	void __attribute__ ((weak, alias ("Default_Handler"))) AES_RNG_IRQHandler			(void) ;
#endif

// ----------------------------------------------------------------------------

extern unsigned int _estack_ ;				// From linker script: initial Main stack address

typedef void (* const pHandler) (void) ;	// Type of the interruptions vector array

// ----------------------------------------------------------------------------

// The table of interrupt handlers. It has an explicit section name
// and relies on the linker script to place it at the correct location in memory.

// Can't place integer and function pointer in one array (Forbidden by ISO C)
__attribute__ ((section(".isr_stackaddress"),used)) unsigned int * isrStackAdress =
{
	& _estack_				// From linker script: initial stack address
} ;

__attribute__ ((section(".isr_vectors"),used)) pHandler isrVectors_[] =
{
	// Cortex-M Core Handlers
	//									   The initial Main stack pointer is in the section .isr_stackaddress
	Reset_Handler,						// The reset handler

	NMI_Handler,						// The NMI handler
	HardFault_Handler,					// The hard fault handler

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
	MemManage_Handler,					// The MPU fault handler
	BusFault_Handler,					// The bus fault handler
	UsageFault_Handler,					// The usage fault handler
#else
	0,									// Reserved
	0,									// Reserved
	0,									// Reserved
#endif
	0,									// Reserved
	0,									// Reserved
	0,									// Reserved
	0,									// Reserved
	SVC_Handler,						// SVCall handler
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
	DebugMon_Handler,					// Debug monitor handler
#else
	0,									// Reserved
#endif
	0,									// Reserved
	PendSV_Handler,						// The PendSV handler
	SysTick_Handler,					// The SysTick handler

	// ----------------------------------------------------------------------
	// External Interrupts
	WWDG_IRQHandler,					//  0 Window WatchDog

#if (defined PWR_PVD_SUPPORT)
	#if (defined PWR_CR2_IOSV)
		PVD_VDDIO2_IRQHandler,			//  1 PVD through EXTI line 16, PVM (monit. VDDIO2) through EXTI line 34
	#else
		PVD_IRQHandler,					//  1 PVD through EXTI Line detection (EXTI line 16)
	#endif
#else
	0,
#endif

	RTC_TAMP_IRQHandler,				//  2 RTC through the EXTI line 19 & 21
	FLASH_IRQHandler,					//  3 FLASH

#if (defined CRS)
	RCC_CRS_IRQHandler,					//  4 RCC and Clock Recovery System
#else
	RCC_IRQHandler,						//  4 RCC
#endif

	EXTI0_1_IRQHandler,					//  5 EXTI Line 0 and 1
	EXTI2_3_IRQHandler,					//  6 EXTI Line 2 and 3
	EXTI4_15_IRQHandler,				//  7 EXTI Line 4 to 15

#if (defined UCPD1 && !defined USB_BASE)
	UCPD1_2_IRQHandler,					//  8 UCPD1, UCPD2
#elif (!defined UCPD1 && defined USB_BASE)
	USB_IRQHandler,						//  8 UCPD1, UCPD2
#elif (defined UCPD1  &&  defined USB_BASE)
	USB_UCPD1_2_IRQHandler,				//  8 UCPD1, UCPD2
#else
	0,									//  8
#endif

	DMA1_Channel1_IRQHandler,			//  9 DMA1 Channel 1
	DMA1_Channel2_3_IRQHandler,			// 10 DMA1 Channel 2 and Channel 3

#if (defined DMA2)
	DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler,	// 11 DMA1 4-7, DMA2 1-5, DMAMUX1 overrun
#else
	DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler,	// 11 DMA1 Channel 4 to Channel 7, DMAMUX1 overrun
#endif

#if (defined COMP)
	ADC1_COMP_IRQHandler,				// 12 ADC1, COMP1 and COMP2 (combined with EXTI 17 & 18)
#else
	ADC1_IRQHandler,					// 12 ADC1
#endif

	TIM1_BRK_UP_TRG_COM_IRQHandler,		// 13 TIM1 Break, Update, Trigger and Commutation
	TIM1_CC_IRQHandler,					// 14 TIM1 Capture Compare

#if (defined TIM2)
	TIM2_IRQHandler,					// 15 TIM2
#else
	0,
#endif

#if (defined TIM4)
	TIM3_TIM4_IRQHandler,				// 16 TIM3
#else
	TIM3_IRQHandler,					// 16 TIM6
#endif

#if (defined DAC1)
	TIM6_DAC_LPTIM1_IRQHandler,			// 17 TIM6, DAC and LPTIM1
#elif defined TIM6
	TIM6_IRQHandler,					// 17 TIM6 only
#else
	LPTIM1_IRQHandler,					// 17 LPTIM1 only
#endif

#if (defined LPTIM2  &&  defined TIM7)
	TIM7_LPTIM2_IRQHandler,				// 18 TIM7 and LPTIM2
#elif (defined TIM7)
	TIM7_IRQHandler,					// 18 TIM7 only
#else
	LPTIM2_IRQHandler,					// 18 LPTIM2 only
#endif

	TIM14_IRQHandler,					// 19 TIM14
	TIM15_IRQHandler,					// 20 TIM15

#if (defined FDCAN1)
	TIM16_FDCAN_IT0_IRQHandler,			// 21 TIM16
	TIM17_FDCAN_IT1_IRQHandler,			// 22 TIM17
#else
	TIM16_IRQHandler,					// 21 TIM16
	TIM17_IRQHandler,					// 22 TIM17
#endif

	I2C1_IRQHandler,					// 23 I2C1 (combined with EXTI 23)
#if (definedI2C3)
	I2C2_3_IRQHandler,					// 24 I2C2 I2C3
#else
	I2C2_IRQHandler,					// 24 I2C2
#endif

	SPI1_IRQHandler,					// 25 SPI1
	SPI2_IRQHandler,					// 26 SPI2
	USART1_IRQHandler,					// 27 USART1
	USART2_IRQHandler,					// 28 USART2

#if (defined LPUART1  &&  !defined USART3  &&  !defined USART5)
	LPUART1_IRQHandler,					// 29 LPUART1 (combined with EXTI 28)

#elif (!defined LPUART1  &&  defined USART3  &&  !defined USART5)
	USART3_4_IRQHandler,				// 29 USART3, USART4

#elif (!defined LPUART1  &&  defined USART3  &&  defined USART5)
	USART3_4_5_6_IRQHandler,			// 29 USART 3 to 6 (combined with EXTI 28)

#elif (defined LPUART1  &&  defined USART3  &&  !defined USART5)
	USART3_4_LPUART1_IRQHandler,		// 29 USART3, USART4 and LPUART1 (combined with EXTI 28)

#elif (defined LPUART1  &&  defined USART3  &&  defined USART5)
	USART3_4_5_6_LPUART1_IRQHandler,	// 29 USART 3 to 6, LPUART1 (combined with EXTI 28)
#else
	0,									// 29
#endif

#if (defined CEC)
	CEC_IRQHandler,						// 30 CEC (combined with EXTI 27)
#else
	0,									// 30
#endif

#if (defined AES)
	AES_RNG_IRQHandler					// 31 AES and RNG
#else
	0,									// 31
#endif
} ;

//-----------------------------------------------------------------------------
// The processor jumps here if an unexpected interrupt occurs or a
// specific handler is not present in the application code.
// When in DEBUG, triggers a debug exception to warn the user

void __attribute__ ((section(".isr_handlers"))) Default_Handler (void)
{
	uint32_t	exception ;

	exception = __get_IPSR () & 0x03Fu ;	// IPSR and SCB->ICSR[VECTACTIVE] are the same info
	aaSpuriousInterruptHandler (exception) ;
	// If exception is >= 16 this is not a system exception,
	// so subtract 16 to get the peripheral exception number in the isrVectors_ array.
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Default exception handlers. Override the ones here by defining your own
// handler routines in your application code.
// When in DEBUG, triggers a debug exception to warn the user

void __attribute__ ((section(".isr_handlers"), noreturn, naked)) Reset_Handler (void)
{
	start_ () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak)) NMI_Handler (void)
{
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak)) SVC_Handler (void)
{
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak)) PendSV_Handler (void)
{
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------
// Hard Fault handler wrapper in assembly.
// It extracts the location of stack frame and passes it to handler in C as a pointer.
// We also pass the LR value as second parameter.
// (Based on Joseph Yiu's, The Definitive Guide to ARM Cortex-M3 and
// Cortex-M4 Processors, Third Edition, Chap. 12.8, page 402).

// naked attribute:
// Use this attribute on the ARM ports to indicate that the specified function
// does not need prologue/epilogue sequences generated by the compiler.
// It is up to the programmer to provide these sequences.
// The only statements that can be safely included in naked functions are asm statements that do not have operands.
// All other statements, including declarations of local variables, if statements, and so forth, should be avoided.
// Naked functions should be used to implement the body of an assembly function,
// while allowing the compiler to construct the requisite function declaration for the assembler.

// From Joseph Yiu: The Definitive Guide to ARM® Cortex® -M0 and Cortex-M0+ Processors
// Other hard fault drive: https://community.nxp.com/thread/424925

void __attribute__ ((section(".isr_handlers"),weak,naked)) HardFault_Handler (void)
{
	__ASM volatile
	(
		" mov	r0,#4				\n"
		" mov	r1,lr				\n"
		" tst	r0,r1				\n"
		" beq	stacking_used_MSP	\n"
		" mrs	r0,psp				\n"
		" b		get_LR_and_branch	\n"
		"stacking_used_MSP:			\n"
		" mrs	r0,msp				\n"
		"get_LR_and_branch:			\n"
		" mov	r1,lr				\n"
		" ldr	r2,=HardFault_Handler_C \n"
		" bx	r2"

		: // Outputs
		: // Inputs
		: // Clobbers
	) ;
}

// Only referenced by inline assembly, so need "used" attribute
// The user may redefine this function, so "weak" attribute

void __attribute__ ((section(".isr_handlers"),weak,used)) HardFault_Handler_C (
						exceptionStackFrame_t	* pFrame,
						uint32_t				lr)
{
	(void) pFrame ;
	(void) lr ;

	while (1)
	{
	}
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Exceptions specific to ARM ARCH 7M /  7EM

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

void __attribute__ ((section(".isr_handlers"),weak)) MemManage_Handler (void)
{
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak)) DebugMon_Handler (void)
{
	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak,naked)) BusFault_Handler (void)
{
	__ASM volatile
	(
		" tst		lr,#4	\n"
		" ite		eq		\n"
		" mrseq		r0,msp	\n"
		" mrsne		r0,psp	\n"
		" mov		r1,lr	\n"
		" ldr		r2,=BusFault_Handler_C \n"
		" bx		r2"
		: // Outputs
		: // Inputs
		: // Clobbers
	) ;
}

// Only referenced by inline assembly, so need "used" attribute
// The user may redefine this function, so "weak" attribute
void __attribute__ ((section(".isr_handlers"),weak,used)) BusFault_Handler_C (
						exceptionStackFrame_t	* pFrame,
						uint32_t				lr)
{
	(void) pFrame ;
	(void) lr ;

	#if defined (DEBUG)
		uint32_t	mmfar	= SCB->MMFAR ;	// MemManage Fault Address
		uint32_t	bfar 	= SCB->BFAR ;	// Bus Fault Address
		uint32_t	cfsr 	= SCB->CFSR ;	// Configurable Fault Status Registers

		(void) mmfar;
		(void) bfar ;
		(void) cfsr  ;
	#endif

	__DEBUG_BKPT () ;
}

// ----------------------------------------------------------------------------

void __attribute__ ((section(".isr_handlers"),weak,naked)) UsageFault_Handler (void)
{
	__ASM volatile
	(
		" tst		lr,#4	\n"
		" ite		eq		\n"
		" mrseq		r0,msp	\n"
		" mrsne		r0,psp	\n"
		" mov		r1,lr	\n"
		" ldr		r2,=UsageFault_Handler_C \n"
		" bx		r2"
		: // Outputs
		: // Inputs
		: // Clobbers
	) ;
}

// Only referenced by inline assembly, so need "used" attribute
// The user may redefine this function, so "weak" attribute
void __attribute__ ((section(".isr_handlers"),weak,used)) UsageFault_Handler_C (
						exceptionStackFrame_t	* pFrame,
						uint32_t 				lr)
{
	(void) pFrame ;
	(void) lr ;

	#if defined (DEBUG)
		uint32_t	mmfar	= SCB->MMFAR ;	// MemManage Fault Address
		uint32_t	bfar 	= SCB->BFAR ;	// Bus Fault Address
		uint32_t	cfsr 	= SCB->CFSR ;	// Configurable Fault Status Registers

		(void) mmfar;
		(void) bfar ;
		(void) cfsr ;
	#endif

	__DEBUG_BKPT () ;
}

#endif	// defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

