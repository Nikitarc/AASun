/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	bspcfg.h	Target BSP specific configuration
				The API of this file is for the exclusive use of the kernel and drivers
				The BSP API usable by applications are in bsp.h

	When		Who	What
	30/12/13	ac	Creation
	24/08/19	ac	Add aaIsInCritical()
	20/04/20	ac	Add compiler barrier
	21/02/21	ac	Port to M0+. Become specific to Cortex-M0(+)
	10/03/22	ac	Reworked bspSaveAndDisableIrq() to avoid instruction reordering by the compiler

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

#if ! defined AABSPCFG_H_
#define AABSPCFG_H_
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	BSP configuration

#define	BSP_WITH_BANNER			1					// Allows to display boot informations

#define	BSP_TICK_RATE			1000u				// Initial tick rate Hz

#define	BSP_SWO_STIMULUS		1u					// SWO stimulus bit mask for terminal output

//--------------------------------------------------------------------------------
//	Stack management

#define	bspSTACK_GROWS_DOWN		1u					// 1 of stack grows from high to low addresses
#define	bspSTACK_PATTERN		0xEFBEADDEu			// Fill the stack with this value for stack usage check

//--------------------------------------------------------------------------------
// To manage task selector bitmaps

typedef	uint32_t				bspPrio_t ;				// Type used for priority accelerator bitmaps

#define	BSP_PRIO_POW			5u						// log2 (sizeof(bspPrio_t)*8), eg uint32_t => 5 so 1<<5 = 32
#define	BSP_PRIO_SIZE			(1u << BSP_PRIO_POW)	// Count of bits in bspPrio_t
#define	BSP_PRIO_MASK			(BSP_PRIO_SIZE - 1u)	// uint16_t => 0x0F, uint32_t => 0x1F

//--------------------------------------------------------------------------------
// Heap position, provided by linker script

extern	char	_heap_begin_ ;
extern	char	_heap_end_ ;

#if (AA_WITH_CRITICALSTAT == 1)
	extern	uint32_t		aaCriticalUsage ;				// Max duration of critical section
	extern	uint32_t		aaTsCriticalEnter ;				// Max duration of critical section
#endif

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// BSP functions for kernel and drivers use only

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------
// Trigger a PendSV exception to switch to next task

__ALWAYS_STATIC_INLINE void bspTaskSwitch (void)
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk ;
	// flush pipeline to ensure exception takes effect before we return from this routine
	__ASM ("isb") ;
}

__ALWAYS_STATIC_INLINE void bspTaskSwitchFromIsr (void)
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk ;
	// flush pipeline to ensure exception takes effect before we return from this routine
	__ASM ("isb") ;
}

//--------------------------------------------------------------------------------
typedef struct aaTcb_s aaTcb_t ;	// Forward declaration for Task Control Block

void BSP_ATTR_NORETURN	aaMain_			(void) ;

void			bspInit_				(void) ;
void			bspStart_				(void) ;
void			bspTaskInit_			(aaTcb_t * pTcb, aaTaskFunction_t pEntry, uintptr_t arg) ;
void			bspBoardInit_			(void) ;

//--------------------------------------------------------------------------------
// Compiler barrier: avoid the compiler to move instructions across this barrier

__ALWAYS_STATIC_INLINE void bspCompilerBarrier (void)
{
	#if defined(__CC_ARM)
		// ARMCC
		__COMPILER_BARRIER () ;
	#else
		// GCC
		__ASM volatile ("" : : : "memory") ;
	#endif
}

//--------------------------------------------------------------------------------
//	Kernel interrupt enable/disable (BSP_MAX_INT_PRIO and lower), without reentrancy
//	M/M0+ architecture doesn't have BASEPRI register

//	Unconditionally enable kernel managed interrupts
__ALWAYS_STATIC_INLINE void bspEnableIrq (void)
{
	__enable_irq () ;
}

// Unconditionally disable kernel managed interrupts
__ALWAYS_STATIC_INLINE void bspDisableIrq (void)
{
	__disable_irq () ;
}

//--------------------------------------------------------------------------------
//	Global  interrupt enable/disable, without reentrancy

//	Unconditionally enable all interrupts
__ALWAYS_STATIC_INLINE void bspEnableIrqAll (void)
{
	__enable_irq () ;
}

// Unconditionally disable all interrupts
__ALWAYS_STATIC_INLINE void bspDisableIrqAll (void)
{
	__disable_irq () ;
}

//--------------------------------------------------------------------------------
//	Interrupt management allowing reentrancy : lightweight critical section
//	WARNING: Use theses functions with caution, don't call kernel API from inside lightweight critical section

//	Not for M/M0+ architecture, who doesn't have BASEPRI register => equivalent to bspEnableIrqAll/bspDisableIrqAll

// Type of interrupt status
typedef	uint32_t aaIrqStatus_t ;

// Restore interrupt status
__ALWAYS_STATIC_INLINE void bspRestoreIrq (aaIrqStatus_t status)
{
	__set_PRIMASK (status) ;
}

//	Save interrupt status and disable interrupts levels BSP_MAX_INT_PRIO and above
//	Both instructions are in the same __ASM to avoid instruction reordering
//	https://gcc.gnu.org/legacy-ml/gcc-help/2017-10/msg00075.htmlat
__ALWAYS_STATIC_INLINE aaIrqStatus_t bspSaveAndDisableIrq (void)
{
	aaIrqStatus_t	status ;
	__ASM volatile
	(
		"MRS   %[STATUS], PRIMASK	\n"
		"CPSID i					\n"
		: [STATUS]	"=r" (status)	// Out
		: 							// In
		: "memory"					// Clobber
	) ;
	return status ;
}

//--------------------------------------------------------------------------------
// Enter in critical section

extern	uint8_t			aaInCriticalCounter	;				// Critical section nesting count

__ALWAYS_STATIC_INLINE void aaCriticalEnter (void)
{
	bspDisableIrq () ;
	aaInCriticalCounter ++ ;
	if (aaInCriticalCounter == 1)
	{
		#if (AA_WITH_CRITICALSTAT == 1)
		{
			aaTsCriticalEnter = bspTsGet () ;
		}
		#endif
	}
}

//--------------------------------------------------------------------------------
// Exit critical section

__ALWAYS_STATIC_INLINE void aaCriticalExit (void)
{
	// To catch aaCriticalExit without aaCriticalEnter
	AA_ASSERT(aaInCriticalCounter != 0) ;

	aaInCriticalCounter -- ;
	if (aaInCriticalCounter == 0)
	{
		// Measure critical section duration
		#if (AA_WITH_CRITICALSTAT == 1)
		{
			uint32_t delta = bspTsDelta (& aaTsCriticalEnter) ;
			if (delta > aaCriticalUsage)
			{
				aaCriticalUsage = delta ;
			}
		}
		#endif
		bspEnableIrq () ;
	}
}

//--------------------------------------------------------------------------------
//	Returns 1 if in critical section, else 0

__ALWAYS_STATIC_INLINE uint32_t aaIsInCritical (void)
{
	return aaInCriticalCounter == 0 ? 0 : 1 ;
}

//--------------------------------------------------------------------------------
//	Return most significant bit position in word
//	1 => 0, 3 => 1, 0x7FFFFFFF => 30
//	Word value 0 should return value >= bspPRIO_SIZE

// https://www.state-machine.com/fast-deterministic-and-portable-clz/

__ALWAYS_STATIC_INLINE  uint32_t	bspMsbPos		(uint32_t word)
{
	return (31u - (uint32_t) __CLZ (word)) ;
}

// Insert a NOP instruction
#define	bspNop()	__NOP ()

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AABSPCFG_H_
