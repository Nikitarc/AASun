/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aaerror.h	Kernel error handling.
				Theses errors numbers are used with AA_ERROR_ASSERT, AA_ERRORCHECK or AA_ERRORNOTIFY

	When		Who	What
	30/11/18	ac	Creation
	03/02/19	ac	Add AA_ERROR_NORETURN_FLAG, AA_ERROR_FORCERETURN_FLAG

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

#if ! defined AA_ERROR_H_
#define AA_ERROR_H_
//--------------------------------------------------------------------------------
// Theses values are defined as DEFINE and not emum, because MISRA don�t allows bitwise operation on anonymous enum.
// Anonymous enum are essentially signed type. Bitwise operations are allowed only on operands of essentially unsigned types.

// Error number fields
#define		AA_ERROR_FATAL_FLAG			0x10000u		// For fatal error
#define		AA_ERROR_NORETURN_FLAG		0x20000u		// Error handling should not return for this error
#define		AA_ERROR_FORCERETURN_FLAG	0x40000u		// Error handling should return for this error
#define		AA_ERROR_ERROR_MASK			0x000FFu		// Mask for error number
#define		AA_ERROR_MODULE_MASK		0x0FF00u		// Mask for module number

// Module number definitions
#define		AA_ERROR_KERNEL_MOD			0x00100u		// Module number: kernel
#define		AA_ERROR_MAIN_MOD			0x00200u		// Module number: aamain
#define		AA_ERROR_MUTEX_MOD			0x00300u		// Module number: mutex
#define		AA_ERROR_SEM_MOD			0x00400u		// Module number: semaphore
#define		AA_ERROR_SIG_MOD			0x00500u		// Module number: task signal
#define		AA_ERROR_TASK_MOD			0x00600u		// Module number: aatask
#define		AA_ERROR_LOGMES_MOD			0x00700u		// Module number: aalogmes
#define		AA_ERROR_QUEUE_MOD			0x00800u		// Module number: queue
#define		AA_ERROR_BPOOL_MOD			0x00900u		// Module number: buffer pool management
#define		AA_ERROR_TIMER_MOD			0x00A00u		// Module number: software timers
#define		AA_ERROR_LIST_MOD			0x00B00u		// Module number: TLSF Dynamic memory allocator
#define		AA_ERROR_TLSF_MOD			0x00C00u		// Module number: Lists
#define		AA_ERROR_MALLOC_MOD			0x00D00u		// Module number: Malloc
#define		AA_ERROR_MALLOCB_MOD		0x00E00u		// Module number: Malloc block
#define		AA_ERROR_OTHER_MOD			0x00F00u		// Module number: Others block
#define		AA_ERROR_USER_MOD			0x0FF00u		// Module number: User errors

// Module errors numbers
#define		AA_ERROR_KERNEL_1			( 1u | AA_ERROR_KERNEL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_KERNEL_2			( 2u | AA_ERROR_KERNEL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_KERNEL_3			( 3u | AA_ERROR_KERNEL_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_MAIN_1				( 1u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MAIN_2				( 2u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MAIN_3				( 3u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MAIN_4				( 4u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MAIN_5				( 5u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MAIN_6				( 6u | AA_ERROR_MAIN_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_MTX_1				( 1u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_2				( 2u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_3				( 3u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_4				( 4u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_5				( 5u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_6				( 6u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_7				( 7u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_8				( 8u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_9				( 9u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_10				(10u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_11				(11u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_12				(12u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_13				(13u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MTX_14				(14u | AA_ERROR_MUTEX_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_SEM_1				( 1u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_2				( 2u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_3				( 3u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_4				( 4u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_5				( 5u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_8				( 8u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_9				( 9u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_11				(11u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SEM_13				(13u | AA_ERROR_SEM_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_SIG_1				( 1u | AA_ERROR_SIG_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SIG_2				( 2u | AA_ERROR_SIG_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SIG_3				( 3u | AA_ERROR_SIG_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_SIG_4				( 4u | AA_ERROR_SIG_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_TASK_1				( 1u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_2				( 2u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_3				( 3u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_4				( 4u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_5				( 5u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_6				( 6u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_7				( 7u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_8				( 8u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_9				( 9u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_10			(10u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_11			(11u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_12			(12u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_13			(13u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TASK_14			(13u | AA_ERROR_TASK_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_LOGMES_1			( 1u | AA_ERROR_LOGMES_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LOGMES_2			( 2u | AA_ERROR_LOGMES_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_MALLOC_1			( 1u | AA_ERROR_MALLOC_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MALLOC_2			( 2u | AA_ERROR_MALLOC_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_MALLOCB_1			( 1u | AA_ERROR_MALLOCB_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MALLOCB_2			( 2u | AA_ERROR_MALLOCB_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MALLOCB_3			( 3u | AA_ERROR_MALLOCB_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_MALLOCB_4			( 4u | AA_ERROR_MALLOCB_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_QUEUE_1			( 1u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_2			( 2u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_3			( 3u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_4			( 4u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_5			( 5u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_6			( 6u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_7			( 7u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_8			( 8u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_9			( 9u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_10			(10u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_11			(11u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_QUEUE_12			(12u | AA_ERROR_QUEUE_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_BPOOL_1			( 1u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_2			( 2u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_3			( 3u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_4			( 4u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_5			( 5u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_6			( 6u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_7			( 7u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_8			( 8u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_BPOOL_9			( 9u | AA_ERROR_BPOOL_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_TIMER_1			( 1u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TIMER_2			( 2u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TIMER_3			( 2u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TIMER_4			( 2u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TIMER_5			( 2u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TIMER_6			( 2u | AA_ERROR_TIMER_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_TLSF_1				( 1u | AA_ERROR_TLSF_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TLSF_2				( 2u | AA_ERROR_TLSF_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TLSF_3				( 3u | AA_ERROR_TLSF_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_TLSF_4				( 4u | AA_ERROR_TLSF_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_LIST_1				( 1u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_2				( 2u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_3				( 3u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_4				( 4u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_5				( 5u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_6				( 6u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_7				( 7u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_8				( 8u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_9				( 9u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_10			(10u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_11			(11u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_12			(12u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_LIST_13			(13u | AA_ERROR_LIST_MOD | AA_ERROR_FATAL_FLAG)

#define		AA_ERROR_ISRPIPE			( 1u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
/*
#define		AA_ERROR__2					( 2u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__3					( 3u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__4					( 4u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__5					( 5u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__6					( 6u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__7					( 7u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__8					( 8u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__9					( 9u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR__10				(10u | AA_ERROR_OTHER_MOD | AA_ERROR_FATAL_FLAG)
*/

// Theses error numbers are for user usage
#define		AA_ERROR_USER_1				( 1u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_2				( 2u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_3				( 3u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_4				( 4u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_5				( 5u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_6				( 6u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_7				( 7u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_8				( 8u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_9				( 9u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)
#define		AA_ERROR_USER_10			(10u | AA_ERROR_USER_MOD | AA_ERROR_FATAL_FLAG)

//--------------------------------------------------------------------------------

// This function must be provided by the user to handle fatal or non fatal errors.
// It can handle its return, depending on the flags in errorNumber
// This function is defined by the BSP with the weak attribute, and a default behavior

extern	void bspErrorNotify (uint8_t * pFileName, uint32_t line, uint32_t errorNumber) ;

// Allows to notify an error to the debugger or a user defined function.
// By  default AA_ERROR_NORETURN_FLAG is present so:
//		it is forbidden for the debugger to continue,
//		or to the user function to return.
// If the AA_ERROR_FORCERETURN_FLAG is added to errorNumber, the debugger can continue, or the user function can return.
// AA_ERROR_FORCERETURN_FLAG take precedence over AA_ERROR_NORETURN_FLAG.

#define	AA_ERRORNOTIFY(errorNumber)						\
		if (AA_WITH_ERRORCHECK != 0)					\
		{												\
			if ((AA_WITH_ERRORCHECK & AA_WITH_ERRORCHECK_ASSERT) != 0)			\
			{											\
				BSP_DEBUG_BKPT() ;						\
			}											\
														\
			if ((AA_WITH_ERRORCHECK & AA_WITH_ERRORHECK_NOTIFY) != 0)			\
			{											\
				bspErrorNotify ((uint8_t *) __FILE__, __LINE__, (errorNumber) | AA_ERROR_NORETURN_FLAG) ;	\
			}											\
		}

#define	AA_ERRORASSERT(test, errorNumber)					\
		if (AA_WITH_ERRORCHECK != 0)						\
		{													\
			if (! (test))									\
			{												\
				AA_ERRORNOTIFY (errorNumber)				\
			}												\
		}

// This macro relies heavily on the elimination of constant expressions by the compiler to generate small and fast code
// If the test condition is false then:
//		the specified action is triggered with errorNumber,
//		and returnValue is returned

#define	AA_ERRORCHECK(test, returnValue, errorNumber)		\
		if (! (test))										\
		{													\
			AA_ERRORNOTIFY ((errorNumber) | AA_ERROR_FORCERETURN_FLAG)		\
			return (returnValue) ;							\
		}


//--------------------------------------------------------------------------------
#endif	// AA_ERROR_H_
