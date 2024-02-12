/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	bsp.c		target specific "C" routines for ARM Cortex-M0(+)
				STM32G0/L0 families

	When		Who	What
	28/04/17	ac	Creation
	12/04/17	ac	Add naked to pendSV_Handler()
	21/08/19	ac	Add naked to bspStart_()
	11/10/19	ac	Remove AA_WITH_HAL
	26/02/20	ac	Enforce double word stack alignment (M3)
	13/03/20	ac	Conditional initialization of logMess 
	19/03/20	ac	Port to STM32L4+
	20/04/20	ac	Modified to use the same file for F4 and L4+
	23/04/20	ac	Start modification for armcc compatibility
	09/05/20	ac	Constraint 'h' on bspStart_() assembly parameters
	17/05/20	ac	Add CCMRAM clock enable (F4 only)
	14/01/20	ac	Port to M0+ architecture. Become specific to Cortex-M0(+)
	12/06/21	ac	Shorter bspTaskInit_()
	17/01/22	ac	Add BSP_ATTR_WEAK to bspSystemInit_() and bspHardwareInit_()
	08/06/22	ac	Modulo 8 stack alignment in bspTaskInit_()
	18/10/23	ac	Add bspJumpToBootLoader

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

//	To change the system clock configuration, you need only to change the SystemClock_Config.c file

#include	"aa.h"
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aaprintf.h"

#include	"rccbasic.h"
#include	"loguart.h"

// The ts package uses a timer
#if (WITH_BSPTS == 1)
	#if (defined STM32L0)
		#include	"stm32l0xx_ll_tim.h"

	#elif (defined STM32G0)
		#include	"stm32g0xx_ll_tim.h"

	#else
		#error No STM32 model defined
	#endif
#endif

// Include HAL definitions
#if defined USE_HAL_DRIVER
	#if (defined STM32L0)
		#include "stm32l0xx_hal.h"

	#elif (defined STM32G0)
		#include "stm32g0xx_hal.h"

	#else
		#error No STM32 model defined
	#endif
#endif

#if (AA_TICK_STRETCH == 1)
	#include	"timbasic.h"
#endif	// AA_TICK_STRETCH

//-----------------------------------------------------------------------------

// Also defined in stm32YYxx_hal_conf.h
#if ! defined USE_HAL_DRIVER

	#if (defined STM32G0)
		#define  PREFETCH_ENABLE              1u
		#define  INSTRUCTION_CACHE_ENABLE     1u
	#endif

#endif

// Define the name of the device managed by this BSP
#if (defined STM32G0)
	#if (defined STM32G061xx)
		#define	BSP_DEVICE_NAME	"STM32G061"
	#elif (defined STM32G070xx)
		#define	BSP_DEVICE_NAME	"STM32G070"
	#elif (defined STM32G071xx)
		#define	BSP_DEVICE_NAME	"STM32G071"
	#elif (defined STM32G081xx)
		#define	BSP_DEVICE_NAME	"STM32G081"
	#elif (defined STM32G0B0xx)
		#define	BSP_DEVICE_NAME	"STM32G0B0"
	#elif (defined STM32G0B1xx)
		#define	BSP_DEVICE_NAME	"STM32G0B1"
	#elif (defined STM32G0B1xx)
		#define	BSP_DEVICE_NAME	"STM32G0C1"
	#else
		#define	BSP_DEVICE_NAME	"STM32G0xx"
	#endif
#else
	#define	BSP_DEVICE_NAME	"STM32???"
#endif

extern uint32_t  _sstack_ ;			// From linker script: lower address of main stack
extern uint32_t  _estack_ ;			// From linker script: address after end of stack

//-----------------------------------------------------------------------------

STATIC	uint32_t	bspTickRate ;			// SysTick frequency

		void		bspSystemClockInit		(void) ;
static 	void		bspTsInit_				(void) ;

// Startup code support
		void		bspSystemInit_			(void) ;
		void		bspHardwareInit_ 		(void) ;
		void		bspMain_				(void) ;

// Exceptions handlers managed by the BSP
		void		SysTick_Handler 		(void) ;
		void		PendSV_Handler 			(void) __attribute__((naked)) ;

// This BSP defines the SysTick configuration function if tick stretch is not used
#if (AA_TICK_STRETCH == 0)
static	uint32_t	bspSysTick_Config		(uint32_t ticks) ;
#endif

//--------------------------------------------------------------------------------
//	AA_ASSERT support function

void BSP_ATTR_WEAK bspAssertFailed (uint8_t * pFileName, uint32_t line)
{
	(void) pFileName ;
	(void) line ;

	// Check if debug is enabled.
	// If debug is enabled, use bkpt instruction to jump to debugger
	// If debug is not enabled, a bkpt instruction triggers a HardFault exception
	#if (__CORTEX_M != 0)
	{
		// No CoreDebug on M0 / M0+
		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u)
		{
			// Debug enabled, can use BKPT instruction
			BSP_DEBUG_BKPT () ;
		}
	}
	#endif

	// No debugger: loop
	bspDisableIrqAll () ;
	while (1)
	{
	}
}

// Support for STMicroelectronics stdperiph, HAL or LL code (stm32_assert.h)
// Not the same assert_failed() prototype in all families...
// For F4 / F1 / G0
void assert_failed (uint8_t * file, uint32_t line)
{
	bspAssertFailed (file, line) ;
}

//--------------------------------------------------------------------------------
//	Default function for centralized error handling.
//	If the debugger is enabled, it jump to the debugger.
//	It returns only if AA_ERROR_FORCERETURN_FLAG is present,
//	or the error is not fatal and return is not forbidden.

//	If another behavior is necessary the user can redefine its own function

void BSP_ATTR_WEAK bspErrorNotify (uint8_t * pFileName, uint32_t line, uint32_t errorNumber)
{
	(void) pFileName ;
	(void) line ;

	// Check if debug is enabled.
	// If debug is enabled, use bkpt instruction to jump to debugger
	// If debug is not enabled, a bkpt instruction triggers a HardFault exception
	#if (__CORTEX_M != 0)
	{
		// No CoreDebug on M0 / M0+
		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u)
		{
			// Debug enabled, can use BKPT instruction
			BSP_DEBUG_BKPT () ;
		}
	}
	#endif

	// If non fatal error, and return allowed, then return
	if ((errorNumber & AA_ERROR_FORCERETURN_FLAG) != 0u  ||
		(errorNumber & (AA_ERROR_FATAL_FLAG | AA_ERROR_NORETURN_FLAG)) == 0u)
	{
		return ;
	}

	// No debugger and can't return: forever loop
	bspDisableIrqAll () ;
	while (1)
	{
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	Startup code support

//-----------------------------------------------------------------------------
//	Prepare the startup code to jump to system boot loader
//	G0 specific
//	https://community.st.com/t5/stm32-mcus/how-to-jump-to-system-bootloader-from-application-code-on-stm32/ta-p/49424
//	https://stm32world.com/wiki/STM32_Jump_to_System_Memory_Bootloader

#define	BOOT_MAGIC			0x434F4F54	// 'BOOT'
#define BOOT_ADDR			0x1FFF0000	// For STM32G0
#define	BOOT_FLAG_OFFSET	100			// Offset of boot flags, offset below end of main stack

// The system boot loader vector table
typedef struct
{
    uint32_t	stackEnd ;
    void (* Reset_Handler) (void) ;

} bootVecTable_t ;

#define pBootVtable	((bootVecTable_t *) BOOT_ADDR)
#define	pBootFlags  ((uint32_t *) ((&_estack_) - BOOT_FLAG_OFFSET))

void	bspJumpToBootLoader (void)
{
	bspDisableIrq () ;			// So main stack won't be used
	* pBootFlags = BOOT_MAGIC ;
	bspResetHardware () ;
}

//-----------------------------------------------------------------------------
//	Very early minimum hardware initialization (right after reset)
//	If you use ITCM, DTCM, backup RAM... you must enable clocks of theses devices here

void	BSP_ATTR_WEAK bspSystemInit_			(void)
{
	// Check if there is a request to switch to the system boot loader
	if (* pBootFlags == BOOT_MAGIC)
	{
		SYSCFG->CFGR1 = 0x01 ;				// System memory is aliased in the boot memory space (0x0000 0000)
		* pBootFlags = 0 ;					// So next boot won't be affected
		__set_MSP (pBootVtable->stackEnd) ;
		pBootVtable->Reset_Handler () ;
	}

	// On platform which start with the watchdog enabled : disable it

	// Call the CSMSIS system initialization routine in system_stm32f4xx.c
	// This can configure external RAM before filling the BSS section
	SystemInit () ;
}

//-----------------------------------------------------------------------------
//	This is the application hardware initialization routine,
//	Called early from bspMain_(), right after data and bss init, before C++ constructors.

void	BSP_ATTR_WEAK bspHardwareInit_ (void)
{
	// Configure Flash prefetch, Instruction cache, Data cache
	#if (defined STM32G0)
		#if (INSTRUCTION_CACHE_ENABLE != 0U)
			FLASH->ACR |= FLASH_ACR_ICEN ;		// Enable I cache
		#else
			FLASH->ACR &= ~FLASH_ACR_ICEN ;		// Disable I cache
		#endif

		#if (PREFETCH_ENABLE != 0U)
			FLASH->ACR |= FLASH_ACR_PRFTEN ;
		#else
			FLASH->ACR &= ~FLASH_ACR_PRFTEN ;
		#endif
	#endif

	// Configure system clocks
	bspSystemClockInit () ;		// In SystemClock_Config.c

	// Call the CSMSIS system clock routine to store the clock frequency
	// in the SystemCoreClock global RAM location.
	SystemCoreClockUpdate () ;

	// Set interruption priority grouping to be preemptive : no sub-priority
	NVIC_SetPriorityGrouping (7 - __NVIC_PRIO_BITS) ;
}

//-----------------------------------------------------------------------------
//	Come here right after startup initialization (end of startup):
//	Thread mode, use MSP, privileged
//	Some time ago this function was nacked and noreturn. But if it came back => crash.
//	Now if it comes back, it goes back to the startup where it causes an assert or a loop.

void bspMain_ (void)
{
	// Continue the PCU initializations
	bspHardwareInit_ () ;

	aaMain_ () ;	// Call kernel entry point
}

//--------------------------------------------------------------------------------
// Initialize hardware used by BSP

void	bspInit_ ()
{
	// Disable interrupts
	bspDisableIrqAll () ;

	// Enable ITM/UART trace
	// Set aaLogMes() output device, user can override this later
	// DEBUG is defined by the makefile
	#if defined (DEBUG)
	{
		// No ITM on M0(+) core, an UART may be used as SWO
		#if (AA_WITH_LOGMES == 1)
		{
			bspLogUartInit () ;
			aaLogMesSetPutChar (0u, bspLogUartPut) ;		// Initialize aaLogMes to use UART
		}
		#endif
	}
	#endif

	bspTickRate = BSP_TICK_RATE ;	// Allows to test tick rate before sysTick is configured

	// Initialize board standard I/O
	bspBoardInit_ () ;

	// Initialize time stamp package
	bspTsInit_ () ;

	// Set PendSV and systick exceptions to lowest priority
	// NVIC_SetPriority use non shifted priority value
	NVIC_SetPriority (PendSV_IRQn,  BSP_MIN_INT_PRIO) ;

	NVIC_SetPriority (SysTick_IRQn, BSP_MIN_INT_PRIO) ;
}

//--------------------------------------------------------------------------------

void	bspBanner_ ()
{
	uint32_t 	cpuid ;
	uint32_t	var, rev;
	uint32_t	temp;

	aaPrintf ("%s Edition\n\n", BSP_DEVICE_NAME) ;

	// Display ARM core info
	cpuid = SCB->CPUID ;
	aaPrintf("CPUID %08X ", cpuid) ;

	var = (cpuid & 0x00F00000) >> 20 ;
	rev = (cpuid & 0x0000000F) ;

	if ((cpuid & 0xFF000000) == 0x41000000)
	{
		switch ((cpuid & 0x0000FFF0) >> 4)
		{
			case 0xC20: aaPrintf("Cortex M0") ;  break ;
			case 0xC60: aaPrintf("Cortex M0+") ; break ;

			default: aaPrintf("Unknown CORE");
	    }
		aaPrintf(" r%dp%d", var, rev);
	}

	#if (defined DBG)
		temp  = DBG->IDCODE ;
	#else
		temp  = DBGMCU->IDCODE ;
	#endif
	aaPrintf (" - DEVID %03X  REV %04X  %u KB FLASH\n",
			temp & 0xFFF, temp >> 16, * (uint16_t *) FLASHSIZE_BASE) ;

	temp = bspGetSysClock () ;
	aaPrintf ("System clock  %u Hz\n", temp) ;

	temp = bspGetTickRate () ;
	aaPrintf ("Tick rate     %u Hz\n", temp) ;
}

//-----------------------------------------------------------------------------
//	Configure kernel tick frequency
//	Source clock = HCLK/8 to allow low tick frequency
//	Param: ticks is a count of HCLK tick (not a frequency)

#if (AA_TICK_STRETCH == 0)

static	uint32_t bspSysTick_Config (uint32_t ticks)
{
	ticks /= 8 ;
	if ((ticks - 1u) > SysTick_LOAD_RELOAD_Msk)
	{
		AA_ASSERT (0) ;
		return (1u) ;	// Reload value impossible
	}

	SysTick->LOAD  = (uint32_t)(ticks - 1UL) ;			// Set reload register
	SysTick->VAL   = 0UL ;								// Load the SysTick Counter Value
	SysTick->CTRL  =	SysTick_CTRL_TICKINT_Msk   |	// Enable interrupt
//						SysTick_CTRL_CLKSOURCE_Msk |	// Divider 8 if 0
						SysTick_CTRL_ENABLE_Msk ;		// Enable SysTick IRQ and SysTick Timer
	return 0u ;	// Function successful
}

#endif

//-----------------------------------------------------------------------------
// Not used, only to avoid linker message when unused code is not removed

uint32_t SysTick_Config(uint32_t ticks)
{
	(void) ticks ;
	return 0u ;
}

//-----------------------------------------------------------------------------
//	Configure kernel tick frequency

BSP_ATTR_USED aaError_t	bspSetTickRate (uint32_t tickHz)
{
	#if (AA_TICK_STRETCH == 1)
	{
		bspTickStretchInit_ (tickHz) ;
		return AA_ENONE ;
	}
	#else
	{
		uint32_t	temp = SystemCoreClock / tickHz ;

		if (bspSysTick_Config (temp) == 0u)
		{
			// SysTick configured without error
			bspTickRate = tickHz ;
			return AA_ENONE ;
		}
		return AA_EFAIL ;
	}
	#endif
}

//-----------------------------------------------------------------------------
//	Get kernel tick frequency (Hz) (Hz)

uint32_t	bspGetTickRate	(void)
{
	return bspTickRate ;
}

//--------------------------------------------------------------------------------
//	Get system core clock frequency (Hz)

uint32_t	bspGetSysClock 		(void)
{
	return SystemCoreClock ;
}

// ----------------------------------------------------------------------------
//	Allows to reset the hardware from software
//	Use NVIC_SystemReset from core_cmXX.h

void BSP_ATTR_WEAK BSP_ATTR_NORETURN bspResetHardware ()
{
	NVIC_SystemReset () ;
	while (1)	// To avoid message about noreturn
	{
	}
}

// ----------------------------------------------------------------------------
//	Allows to get an interrupt priority value.
//	The value is computed relative to the max allowed priority (higher urgency)
//	Offset 0 returns the max allowed priority
//	Offset 1 returns the priority just below max, etc
//	The lowest priority is bounded by BSP_MIN_INT_PRIO

uint32_t	bspIrqPrioMaxMinus		(uint32_t offset)
{
	uint32_t	priority ;

	priority = BSP_MAX_INT_PRIO + offset ;
	if (priority > BSP_MIN_INT_PRIO)
	{
		priority = BSP_MIN_INT_PRIO ;
	}
	return priority ;
}

// ----------------------------------------------------------------------------
//	Allows to get an interrupt priority value.
//	The value is computed relative to the min allowed priority (lower urgency)
//	Offset 0 returns the min allowed priority
//	Offset 1 returns the priority just above min, etc
//	The highest priority is bounded by BSP_MAX_INT_PRIO

uint32_t	bspIrqPrioMinPlus		(uint32_t offset)
{
	uint32_t	priority ;

	priority = BSP_MIN_INT_PRIO - offset ;
	if (priority > BSP_MIN_INT_PRIO)
	{
		priority = BSP_MAX_INT_PRIO ;
	}
	return priority ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	The time stamp package use the DWT->CYCNT counter when available
//	On M0(+) core use a timer

void	bspTsInit_ (void)
{
#if (WITH_BSPTS == 1)
	// Initialize the timer as frequency generator
	rccEnableTimClock (BSPTS_TIMER) ;	// Enable timer clock
	rccResetTim (BSPTS_TIMER) ;			// Reset timer

	// Default values:
	// LL_TIM_COUNTERMODE_UP
	// ARR not buffered

	LL_TIM_SetPrescaler (BSPTS_TIMER, (SystemCoreClock / BSPTS_FREQ) - 1u) ;

	BSPTS_TIMER->CNT = 0 ;
	BSPTS_TIMER->ARR = 0xFFFF ;

	LL_TIM_EnableCounter (BSPTS_TIMER) ;

#endif
}

//--------------------------------------------------------------------------------
//	Lets busy wait for delays of some microseconds (min 1 us, 0 is forbidden)
//	Force optimization level to be independent of the general setting
//	BEWARE: At low timer frequency the 1us delay may be impossible

#if defined(__CC_ARM)
	#pragma push
	#pragma Otime
#else
	#pragma GCC push_options
	#pragma GCC optimize ("O3")
#endif

void bspDelayUs (uint32_t us)
{
	#if (WITH_BSPTS == 1)
	{
		volatile uint32_t start = BSPTS_TIMER->CNT ;
		volatile uint32_t count = (BSPTS_MHZ * us) - 2u ;	// Adjust for instructions

		while (((BSPTS_TIMER->CNT - start) & 0xFFFFu) < count) ;
	}
	#else
	{
		(void) us ;
		AA_ASSERT (0) ;		// Time stamp not configured
	}
	#endif
}

#if defined(__CC_ARM)
	#pragma pop
#else
	#pragma GCC pop_options
#endif

//--------------------------------------------------------------------------------
//	Check main stack: return unused words count
//	The main stack is from _sstack_ to _estack_ (which isnext byte above the last stack word)
//	Theses values are provided by the linker

uint32_t	bspMainStackCheck (void)
{
	uint32_t	freeCount = 0 ;
	uint32_t	* pStack = (uint32_t *) & _sstack_ ;

	while (pStack != (uint32_t *) & _estack_)
	{
		if (* pStack != 0u)
		{
			break ;
		}
		freeCount++ ;
		pStack++;
	}

	return freeCount ;
}

//--------------------------------------------------------------------------------
//	Create task switch environment

void	bspTaskInit_ (aaTcb_t * pTcb, aaTaskFunction_t pEntry, uintptr_t arg)
{
	bspStackType_t	* pStack ;

	// Simulate a stack who grows from top to bottom
	pStack = & pTcb->pStack [pTcb->stackSize] ;

	// Modulo 8 stack alignment
	pStack = (bspStackType_t *) (((uintptr_t) pStack) & ~0x07) ;

	// No need of switch environment for idle task
	if (pTcb->taskIndex != 0u)
	{
		// Build the task register context in the stack
#if (1)
		* (--pStack) = 0x01000000u ;			// xPSR : thumb
		* (--pStack) = (uint32_t) pEntry ;		// PC
		* (--pStack) = (uint32_t) aaTaskEnd_ ;	// LR Return value to terminate the thread
		pStack -= 4 ;							// R12 R3 R2 R1
		* (--pStack) = arg ;					// R0
		pStack -= 8 ;							// R11 R10 R9 R8 R7 R6 R5 R4
		* (--pStack) = 0xFFFFFFFDu ;			// LR
		--pStack ;								// R4
#else
		* (--pStack) = 0x01000000u ;			// xPSR : thumb
		* (--pStack) = (uint32_t) pEntry ;		// PC
		* (--pStack) = (uint32_t) aaTaskEnd_ ;	// LR Return value to terminate the thread
		* (--pStack) = 0x0C0Cu ;				// R12
		* (--pStack) = 0x0303u ;				// R3
		* (--pStack) = 0x0202u ;				// R2
		* (--pStack) = 0x0101u ;				// R1
		* (--pStack) = arg ;					// R0

		* (--pStack) = 0x0B0Bu ;				// R11
		* (--pStack) = 0x0A0Au ;				// R10
		* (--pStack) = 0x0909u ;				// R9
		* (--pStack) = 0x0808u ;				// R8
		* (--pStack) = 0x0707u ;				// R7
		* (--pStack) = 0x0606u ;				// R6
		* (--pStack) = 0x0505u ;				// R5
		* (--pStack) = 0x0404u ;				// R4
		* (--pStack) = 0xFFFFFFFDu ;			// LR
		* (--pStack) = 0x4444u ;				// R4
#endif
	}
	pTcb->pTopOfStack = pStack ;	// Pointer to the lower stack word used
}

//-----------------------------------------------------------------------------
//	Switch to first task context, start tick timer, and enter idle task
//	Load PSP with idle task stack pointer and switch to PSP =>  thread mode,  privileged
//	pAaCurrentTcb, which is idle task, was set by aaStart_()

// WARNING: We change the stack pointer in this function
// So the push at the beginning does't match the pop at the end.
// The compiler should not insert prologue/epilogue: declare bspStart_() as naked (GCC only)

#if defined(__CC_ARM)
	__ASM void	bspStart_		(void)
#else
	void BSP_ATTR_NAKED  bspStart_	(void)
#endif
{
	__ASM volatile
	(
		"LDR	R0, =pAaCurrentTcb	\n"		// R0 is the address of pAaCurrentTcb
		"LDR	R0, [R0]			\n"		// R0 is the value of pAaCurrentTcb (@ of TCB)
		"LDR	R0, [R0]			\n"		// R0 is the value of pAaCurrentTcb->pStack
		"MSR	PSP, R0				\n"		// Set PSP
		"MOV	R0, #2				\n"		// 2: Use PSP,  1: unprivileged
		"MSR	CONTROL, R0			\n"		// Set CONTROL to R0 => Now SP is PSP, privileged thread mode
		"ISB						\n"		// Required when changing the stack pointer

											// Reset MSP to high memory
		"MOV	R0, %[VTOR]			\n"		// NVIC VTOR, locate the vector table
		"LDR	R0, [R0]			\n"		// Read VTOR offset
		"LDR	R0, [R0]			\n"		// Read first entry of VTOR: MSP value
		"MSR	MSP, R0				\n"		// Set MSP back to top of RAM

		"MOV	R0, %[TICKRATE]		\n"		// Start SysTick
		"BL		bspSetTickRate		\n"

		"CPSIE	I					\n"		// Enable interrupts

											// SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk ;
		"MOV	R0, %[ICSR]			\n"		// R0 is the address of the SCB->ICSR registers
		"LDR	R1, [R0]			\n"		// R1 is the value of ICSR
		"MOV	R2, %[PENDMSK]		\n"		// R2 is SCB_ICSR_PENDSVSET_Msk
		"ORR	R1, R1, R2			\n"		// R1 |= SCB_ICSR_PENDSVSET_Msk
		"STR	R1, [R0]			\n"		// Write ICSR
		"ISB						\n"		// flush pipeline to ensure exception takes effect before we return from this routine

		"LDR	R0, =aaIdleTask_	\n"
		"BX		R0					\n"		// Jump to idle task (parameter not used)

		:
		:	[ICSR]     "h" (&SCB->ICSR),	// Constraint 'h' to use registers r8..r15
			[VTOR]     "h" (&SCB->VTOR),
			[TICKRATE] "h" (BSP_TICK_RATE),
			[PENDMSK]  "h" (SCB_ICSR_PENDSVSET_Msk)
		:"r0", "r1", "r2"
	) ;
}

//--------------------------------------------------------------------------------
//	PendSV exception handler

// WARNING: We change the stack pointer in this function
// So the push at the beginning does't match the pop at the end.
// The compiler should not insert prologue/epilogue: declare PendSV_Handler() as naked (GCC only)

// Thanks to "The Definitive Guide to ARM Cortex-M0 and Cortex-M0+ Processors" by Joseph Yiu

#if defined(__CC_ARM)
	__ASM void	PendSV_Handler (void)
#else
	void	BSP_ATTR_NAKED PendSV_Handler (void)
#endif
{
 	__ASM volatile
	(
		"MRS	R0, PSP				\n"		// Get task SP (the handler is running on MSP)
		"SUB	R0, R0, #40			\n"		// Space for 8 registers
		"MOV	R2, LR				\n"
		"STMIA	R0!, {R1-R2}		\n"		// Save LR with EXC_RETURN (8 byte aligned)
		"STMIA	R0!, {R4-R7}		\n"		// Save R4-R7 in low memory space
		"MOV	R4, R8				\n"
		"MOV	R5, R9				\n"
		"MOV	R6, R10				\n"
		"MOV	R7, R11				\n"
		"STMIA	R0!, {R4-R7}		\n"		// Save R8-R11 in high memory space
		"SUB	R0, R0, #40			\n"		// R0 is SP of the current task

		"LDR	R4, =pAaCurrentTcb	\n"		// R4 is the address of pCurrentTcb
		"LDR	R2, [R4]			\n"		// R2 is a pointer to current TCB
		"STR	R0, [R2]			\n"		// Save SP in first word of current TCB

		"BL		aaTaskSwitch_		\n"		// Do context switch: set the new value of pAaCurrentTcb

		"LDR	R4, =pAaCurrentTcb	\n"		// R4 is the address of pCurrentTcb
		"LDR	R2, [R4]			\n"		// R2 is a pointer to next TCB
		"LDR	R0, [R2]			\n"		// R0 is SP of pNextTask
		"LDMIA	R0!, {R1-R2}		\n"		// Restore LR with EXC_RETURN (in R2 because LDMIA can't use LR)

		"ADD	R0, R0, #16			\n"
		"LDMIA	R0!, {R4-R7}		\n"		// R8-R11 in high memory space
		"MOV	R8, R4				\n"
		"MOV	R9, R5				\n"
		"MOV	R10, R6				\n"
		"MOV	R11, R7				\n"
		"MSR	PSP, R0				\n"		// Restore PSP of pNextTask (its at the future final value)
		"SUB	R0, R0, #32			\n"
		"LDMIA	R0!, {R4-R7}		\n"		// R4-R7 in low memory space

		"BX		R2					\n"		// Return from exception
		:
		:
		:
	) ;
}

//-----------------------------------------------------------------------------
//  SysTick interrupt handler, periodic tick

void	SysTick_Handler (void)
{
	aaIntEnter () ;

	#if defined USE_HAL_DRIVER
	{
		HAL_IncTick () ;
	}
	#endif
	aaTick_ () ;

	aaIntExit () ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
#if (AA_TICK_STRETCH == 1)

// Define the timer to use for tick stretching
// The ratio between the frequency of the timer and the tick frequency should be at least 2
// So: timer frequency >= 2 x tick frequency
// Use TIM7: clock 64 MHz with prescaler 32000 => timer clock is 2 Khz.
// To get tick of 1 KHz, the timer divider TKS_DIV is 2
// In timbasic.h: WITH_TIM_7 should be 0, because the interrupt handler is defined in this source

//	This structure describe the tick stretching state. It is BSP specific.

typedef struct tickStretch_s
{
	uint32_t	arrStart ;		// Counter value of next periodic tick
	uint32_t	arrEnd ;		// Counter value of last periodic tick
	uint32_t	tickNext ; 		// Count of tick if the counter reach arrEnd and overflow

} tickStretch_t ;

static	tickStretch_t		tickState ;
static	TIM_TypeDef			* pTim ;			// Pointer to TIM registers

#define	TKS_DIV				2u
#define	TKS_ARR_MAX			32000u				// Max stretch is 32000 periodic tick => 32 seconds if tick is 1 ms
#define	TKS_PRESCALER		32000u				// Timer prescaler value
#define	TKS_FREQUENCY		1000u				// Tick frequency
/*
//	Another combination
#define	TKS_DIV				4u
#define	TKS_ARR_MAX			16000u				// Max stretch is 16000 periodic tick => 16 seconds if tick is 1 ms
#define	TKS_PRESCALER		16000u				// Timer prescaler value
#define	TKS_FREQUENCY		1000u				// Tick frequency
*/

// Timer descriptor
static const timDesc_t		timDesc =
{
	7u,									// Descriptor identifier
	7u,									// Timer number 1..14
	TIM_CAP_TYPE_BASIC | TIM_CAP_DMA, 	// Capabilities flags
	TIM7,								// Pointer to TIM registers
#if (defined LPTIM2)
	TIM7_LPTIM2_IRQn,
#else
	TIM7_IRQn,
#endif
	{
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch1
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch2
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch3
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch4
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch1N
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch2N
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// Ch3N
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// ETR
		{	0u,		0u,	AA_GPIO_AF_1,	0u },	// BKIN
	},
} ;

//--------------------------------------------------------------------------------

void	TIM7_LPTIM2_IRQHandler (void)
{
	aaIntEnter () ;

	if ((pTim->SR & TIM_SR_UIF) != 0u)
	{
		pTim->SR &= ~TIM_SR_UIF ;	// clear UIF flag
	}

	// Configure the timer for 1 tick
	tickState.arrStart	= 0u ;
	tickState.arrEnd	= (BSP_TICK_RATE * TKS_DIV) - 1u ;
	tickState.tickNext	= 1u ;		// Tick count to wait

	aaTick_ () ;

	aaIntExit () ;
}

//--------------------------------------------------------------------------------
//	Configure and start timer with frequency BSP_TICK_RATE

void	bspTickStretchInit_ (uint32_t tickHz)
{
	(void) tickHz  ;

	pTim = timDesc.pTim ;

	// If timer already started, ignore
	if ((pTim->CR1 & TIM_CR1_CEN) != 0u)
	{
		// Already configured
		return ;
	}

	// Configure timer frequency. No preload: we want to set ARR on the fly
	timTimeBaseInit  (& timDesc, TKS_PRESCALER, TKS_FREQUENCY, 0u) ;	// Not preload mode!

	// Configure interrupt without callback (we define our own irq handler)
	timConfigureIntr (& timDesc, BSP_MIN_INT_PRIO, NULL, 0u, TIM_INTR_UPDATE) ;

	tickState.arrStart	= 0u ;
	tickState.arrEnd	= timGetReload (& timDesc) ;
	tickState.tickNext	= 1u ;		// Tick count to wait

	// Clear SLEEPDEEP bit of Cortex System Control Register (No deep sleep)
	SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk ;

	timStart (& timDesc) ;
}

//--------------------------------------------------------------------------------

void	bspTickStretch_ (void)
{
	uint32_t	nTick ;			// Periodic tick count to wait
	uint32_t	cnt ;			// Current value of the timer counter
	uint32_t	arrNext ;		// Value of ARR next to cnt: the timer will fire when CNT become arrNext+1 and loop back to 0
	uint32_t	arr ;

	aaCriticalEnter () ;

	// Get tick count to wait
	nTick = aaTickToWait_ () ;
	if (nTick == AA_INFINITE)
	{
		nTick = TKS_ARR_MAX / TKS_DIV ;	// Maximum allowed
	}

#if (AA_WITH_LOGMES == 1)
	// Don't sleep is there is log messages to display
	if (nTick >= 2u  &&  aaLogMesCheck_ () == 0u)
#else
	if (nTick >= 2u)
#endif
	{
		cnt = pTim->CNT ;
		arrNext = ((cnt / TKS_DIV) + 1u) * TKS_DIV - 1u ;
		if ((cnt != arrNext)  &&  (pTim->SR & TIM_SR_UIF) == 0u)
		{
			// cnt is not the last clk of the tick, and timer interrupt is not pending
			arr = arrNext + ((nTick - 1u) * TKS_DIV) ;
			if (arr > TKS_ARR_MAX)
			{
				nTick -= (arr - TKS_ARR_MAX) / TKS_DIV ;
				arr = TKS_ARR_MAX ;
			}
			pTim->ARR = arr ;
			tickState.arrStart = arrNext ;
			tickState.arrEnd   = arr ;
			tickState.tickNext = nTick ;
#if (__CORTEX_M != 0u)
			bspDisableIrqAll () ;
			aaCriticalExit () ;
#endif
			// Sleep
			__DSB () ;	// Armv7-M Architectural requirement
			__WFI () ;

#if (__CORTEX_M != 0u)
			aaCriticalEnter() ;
			bspEnableIrqAll () ;
#endif

			// Awake: Check if a timer interrupt is pending
			cnt = pTim->CNT ;
			if ((cnt == tickState.arrEnd)  ||  ((pTim->SR & TIM_SR_UIF) != 0u))
			{
				// Interrupt pending or in last clock tick
				nTick = tickState.tickNext - 1u ;	// Simulate past ticks, the last one will be managed by IRQ handler
			}
			else
			{
				// No timer pending interrupt: Early awakening
				// We have at least 1 clock tick to reconfigure the timer
				// Compute count of elapsed ticks
				nTick = 0u ;
				if (cnt > tickState.arrStart)
				{
					nTick = ((cnt - tickState.arrStart - 1u) / TKS_DIV) + 1u ;
				}
				// Set timer for 1 tick
				arrNext = ((cnt / TKS_DIV) + 1u) * TKS_DIV - 1u ;
				pTim->ARR = arrNext ;
			}

			// Update ticks counters
			aaUpdateTick_ (nTick) ;
			#if defined USE_HAL_DRIVER
			{
				for (cnt = 0u ; cnt < nTick ; cnt++)
				{
					HAL_IncTick () ;
				}
			}
			#endif

			tickState.tickNext = 1u ;
		}
	}
	else
	{
	}

	aaCriticalExit () ;
}

#endif	// AA_TICK_STRETCH

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Forbidden functions from HAL

void HAL_SuspendTick (void)
{
	AA_ASSERT (0) ;
}

void HAL_ResumeTick (void)
{
	AA_ASSERT (0) ;
}

//--------------------------------------------------------------------------------
