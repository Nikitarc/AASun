/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aakernel.c	Kernel base routines

	When		Who	What
	25/11/13	ac	Creation
	16/05/20	ac	V 1.8	Changed C++ static initialization
							Lightweight critical section added in aaTaskSwitch_()
	01/06/20	ac	V 1.9	Allow AA_MUTEX_MAX, AA_SEM_MAX, AA_TIMER_MAX, AA_QUEUE_MAX, AA_BUFPOOL_MAX to be 0
	12/06/21	ac	V 1.10	Remove simple mutex in aasema.c
							No more logMess task. The messages are displayed by the idle task.
	09/06/22	ac	V 1.11	TLSF: Fail to align to 0x04 or 0x0C in tlsfInit()
	17/04/24	ac	V 1.12	add aaIoResumeWaitingTask()

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

#define	AA_KERNEL_MAIN
#include	"aa.h"
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aatimer.h"

//--------------------------------------------------------------------------------
//	Return RTK version, with 2 numbers: Version - Release, eg 0x00010002 for 1.2

#define	AA_VERSION	((1u << 16u) | 11u)

uint32_t	aaVersion	(void)
{
	return AA_VERSION ;
}

//--------------------------------------------------------------------------------
// Task selector accelerator bitmap.
// In aaPrioGroup: 1 bit per priority level. Bit value is 1 when the corresponding ready list is not empty.
// Word 0 bit 0 is priority 0, word 1 bit 0 is priority bspPRIO_SIZE, etc.

#if (AA_PRIO_COUNT  > (BSP_PRIO_SIZE * BSP_PRIO_SIZE))
	#error AA_PRIO_COUNT should be lower than (BSP_PRIO_SIZE * BSP_PRIO_SIZE)
#endif

// Count of group words in aaPrioGroup. Can optimize code if value is 1 or 2
#define	aaPRIO_GROUP_COUNT	((AA_PRIO_COUNT+BSP_PRIO_SIZE-1) / BSP_PRIO_SIZE)

STATIC	bspPrio_t	aaPrioGroup [aaPRIO_GROUP_COUNT] ;

#if (aaPRIO_GROUP_COUNT > 2)
	// In aaPrioGroupIndex, a bit is 1 when the corresponding word in aaPrioGroup is not 0.
	STATIC	bspPrio_t	aaPrioGroupIndex ;
#endif

//--------------------------------------------------------------------------------

const char * const aaTaskStateName [aaMaxState] =
{
	"None", "Ready", "Delayed", "Suspended", "MutexWait", "SemWait", "SigWait", "QueueWait", "IoWait"
} ;

//--------------------------------------------------------------------------------
//	Return the highest priority with ready task
//	Priority 0 is the lowest

__STATIC_INLINE uint32_t	aaGetHighestPrio (void)
{
	#if (AA_PRIO_COUNT <= BSP_PRIO_SIZE)
	{
		// Only one word in accelerator bitmap
		AA_KERNEL_ASSERT (aaPrioGroup [0] != 0) ;		// Development check
		return bspMsbPos (aaPrioGroup [0]);
	}
	#elif (AA_PRIO_COUNT <= (2 * BSP_PRIO_SIZE))
	{
		// Only two words in accelerator bitmap
		if (aaPrioGroup [1] != 0)
		{
			// Some ready tasks are in the higher group
			return BSP_PRIO_SIZE + bspMsbPos (aaPrioGroup [1]) ;
		}
		else
		{
			// Ready tasks are only in lower group
			AA_KERNEL_ASSERT (aaPrioGroup [0] != 0) ;		// Development check
			return bspMsbPos (aaPrioGroup [0]) ;
		}
	}
	#else
	{
		// General slow case, with a aaPrioGroupIndex handling
		uint8_t	group, prio ;

		AA_KERNEL_ASSERT (aaPrioGroupIndex != 0) ;		// Development check

		group	= bspMsbPos (aaPrioGroupIndex) ;
		prio	= bspMsbPos (aaPrioGroup [group]) ;
		return prio	+ (group << 3) ;
	}
	#endif
}

//--------------------------------------------------------------------------------
//	Should be called from inside critical section
//	Remove a TCB from the priority accelerator tables, and from its ready list.
//	Whatever TCB position in the list.

void	aaRemoveReady_ (aaTcb_t * pTcb)
{
	uint8_t			prio = pTcb->priority ;
	aaListHead_t	* pList = & aaTaskReadyList [prio] ;

	AA_ERRORASSERT (pTcb != & aaTcbArray [0], AA_ERROR_KERNEL_1) ;		// The idle task !

	// Optimize code size and speed if aaPRIO_GROUP_COUNT is less than 2
	#if (aaPRIO_GROUP_COUNT == 1)
	{
		if (aaListCount1 (pList) == 1)
		{
			// There is only one task in this list, it will be empty
			// So clear the accelerator bit of this list
			aaPrioGroup [0]	&= ~(1u << prio) ;
		}
	}
	#endif

	#if (aaPRIO_GROUP_COUNT == 2)
	{
		if (aaListCount1 (pList) == 1)
		{
			// There is only one task in this list, so it will be empty
			// So clear the accelerator bit of this list
			if (prio < BSP_PRIO_SIZE)
			{
				aaPrioGroup [0]	&= ~(1 << prio) ;
			}
			else
			{
				aaPrioGroup [1]	&= ~(1 << (prio - BSP_PRIO_SIZE)) ;
			}
		}
	}
	#endif

	#if (aaPRIO_GROUP_COUNT > 2)
	{
		if (aaListCount1 (pList) == 1)
		{
			// This ready task list has only 1 task, remove its bit in accelerator table
			uint8_t	groupIndex ;

			groupIndex					 = prio >> BSP_PRIO_POW ;
			aaPrioGroup [groupIndex]	&= ~(1 << (prio & BSP_PRIO_MASK)) ;
			if (aaPrioGroup [groupIndex] == 0)
			{
				aaPrioGroupIndex &= ~(1 << groupIndex) ;
				AA_KERNEL_ASSERT (aaPrioGroupIndex != 0) ;		// Development check, there is never 0 task
			}
		}
	}
	#endif

	// Remove task from ready list
	aaListRemove (pList, & pTcb->listNode) ;
}

//--------------------------------------------------------------------------------
//	Add a TCB to the priority accelerator tables and to tail of its ready list.
//	Should be called from inside critical section

void	aaAddReady_ (aaTcb_t * pTcb)
{
	uint8_t	prio = pTcb->priority ;

	// Optimize code size and speed if aaPRIO_GROUP_COUNT is 1 or 2
	#if (aaPRIO_GROUP_COUNT == 1)
	{
		aaPrioGroup [0]	|= 1u << prio ;
	}
	#endif

	#if (aaPRIO_GROUP_COUNT == 2u)
	{
		if (prio < BSP_PRIO_SIZE)
		{
			aaPrioGroup [0]	|= 1u << prio ;
		}
		else
		{
			aaPrioGroup [1]	|= 1u << (prio - BSP_PRIO_SIZE) ;
		}
	}
	#endif

	#if (aaPRIO_GROUP_COUNT > 2u)
	{
		if (aaListIsEmpty (& aaTaskReadyList [prio]) == 1u)
		{
			// This is the first task added to this list, add bits to accelerator table
			uint8_t	groupIndex ;

			groupIndex					 = prio >> BSP_PRIO_POW ;
			aaPrioGroup [groupIndex]	|= 1u << (prio & BSP_PRIO_MASK) ;
			aaPrioGroupIndex			|= 1u << groupIndex ;
		}
	}
	#endif

	pTcb->state = aaReadyState ;		// Clear suspended flag
	aaListAddTail (& aaTaskReadyList [prio], & pTcb->listNode) ;
}

//--------------------------------------------------------------------------------
//	Put list node in list ordered by decreasing value of node value field.
//	For mutex/sem list value is a priority, so head of list is the highest priority TCB,
//	and tasks of equal priority are Last In First Out (LIFO)
//	Must be called from critical section

void	aaListAddOrdered_ (aaListHead_t * pList, aaListNode_t * pNodeAdd)
{
	aaListNode_t	* pNode ;

	pNode = (aaListNode_t *) pList ;
	while (aaListIsLast (pList, pNode) == 0)
	{
		pNode = aaListGetNext (pNode) ;
		if (pNode->value < pNodeAdd->value)
		{
			// Insert before this node
			aaListAddAfter (pList, aaListGetPrev (pNode), pNodeAdd) ;
			return ;
		}
	}
	// Add at end of list
	aaListAddAfter (pList, pNode, pNodeAdd) ;
}

//--------------------------------------------------------------------------------
//	This is the most used routine, so it is the most accelerated

void	aaSchedule_ ()
{
	aaCriticalEnter () ;

	// No scheduling if kernel not started or in interrupt service routine
	if ((aaIsRunning != 0)  &&  (aaIsInIsr () == 0))
	{
		pAaNextTcb = (aaTcb_t *) aaListGetFirst (& aaTaskReadyList [aaGetHighestPrio ()])->ptr.pOwner ;

		if (pAaNextTcb != pAaCurrentTcb)
		{
			// Switch tasks
			bspTaskSwitch () ;
#if defined AA_WIN32_PORT
			// WIN32 port: aaCriticalExit done by bspTaskSwitch
			return ;
#endif
		}
	}
	aaCriticalExit () ;
	// The switch is effectively done here
	// flush pipeline to ensure exception takes effect before we return from this routine
	__ASM ("isb") ;
}

//--------------------------------------------------------------------------------
//	To be called at end of every interrupt handler

void	aaIntExit			(void)
{
	aaIrqStatus_t	intState ;

	intState = bspSaveAndDisableIrq () ;

	AA_ERRORASSERT (aaIsInIsr () != 0, AA_ERROR_KERNEL_3) ;
	aaInIsrCounter -- ;
	if (aaInIsrCounter == 0)
	{
		pAaNextTcb = (aaTcb_t *) aaListGetFirst (& aaTaskReadyList [aaGetHighestPrio ()])->ptr.pOwner ;

		if (pAaNextTcb != pAaCurrentTcb)
		{
			bspTaskSwitchFromIsr () ;
		}
	}

	aaTraceInterruptExit () ;

	bspRestoreIrq (intState) ;
}

//--------------------------------------------------------------------------------
//	This function is called by the context switching ISR at bsp level
//	Only referenced by inline assembly, so need "used" attribute

void	__attribute__ ((section(".after_vectors"),used)) aaTaskSwitch_ (void)
{
	aaIrqStatus_t	intState ;

	// Check the stack overflow of the current task before swapping it
	if ((pAaCurrentTcb->flags & AA_FLAG_STACKCHECK) != 0)
	{
		bspStackType_t	* pStack ;

		#if (bspSTACK_GROWS_DOWN != 0)
		{
			// For CPU whose stack grows from high to low addresses
			pStack = pAaCurrentTcb->pStack ;
			if (pStack >= pAaCurrentTcb->pTopOfStack)
			{
				// Stack overflow. Notify only once
				if ((pAaCurrentTcb->flags |= AA_FLAG_STACKOVFL_) != 0)
				{
					aaUserNotify (AA_NOTIFY_STACKOVFL, pAaCurrentTcb->taskIndex | AA_HTAG_TASK) ;
					pAaCurrentTcb->flags |= AA_FLAG_STACKOVFL_ ;
				}
			}
			else
			{
				// Check stack threshold
				if (pStack [8] != bspSTACK_PATTERN  ||  pStack [7] != bspSTACK_PATTERN)
				{
					// Stack threshold reached. Notify only once
					if ((pAaCurrentTcb->flags |= AA_FLAG_STACKTHR_) != 0)
					{
						aaUserNotify (AA_NOTIFY_STACKTHR, pAaCurrentTcb->taskIndex | AA_HTAG_TASK) ;
						pAaCurrentTcb->flags |= AA_FLAG_STACKTHR_ ;
					}
				}
			}
		}
		#else
		{
			// For CPU whose stack grows from low to high addresses
			pStack = & pAaCurrentTcb->pStack [pAaCurrentTcb->stackSize-1];
			if (pStack <= pAaCurrentTcb->pTopOfStack)
			{
				// Stack overflow. Notify only once
				if ((pAaCurrentTcb->flags |= AA_FLAG_STACKOVFL_) != 0)
				{
					aaUserNotify (AA_NOTIFY_STACKOVFL, pAaCurrentTcb->taskId) ;
					pAaCurrentTcb->flags |= AA_FLAG_STACKOVFL_ ;
				}
			}
			else
			{
				// Check stack threshold
				if (pStack [-8] != bspSTACK_PATTERN  ||  pStack [-9] != bspSTACK_PATTERN)
				{
					// Stack threshold reached. Notify only once
					if ((pAaCurrentTcb->flags |= AA_FLAG_STACKTHR_) != 0)
					{
						aaUserNotify (AA_NOTIFY_STACKTHR, pAaCurrentTcb->taskId) ;
						pAaCurrentTcb->flags |= AA_FLAG_STACKTHR_ ;
					}
				}
			}
		}
		#endif
	}

	#if (AA_WITH_TASKSTAT == 1)
	{
		// Compute task running time
		uint32_t delta = bspTsDelta (& aaTsTaskSwitch) ;
		pAaCurrentTcb->cpuUsage += delta ;
		aaCpuUsage += delta ;
	}
	#endif

	// Now we will use pAaNextTcb.
	// It can be changed by a nested interrupt up to this point.
	// We don't want it to change while we use it => lightweight critical section
	intState = bspSaveAndDisableIrq () ;

#if (AA_WITH_NEWLIB_REENT == 1)
	// Set newlib impure ptr for the next task
	_impure_ptr = & pAaNextTcb->newlibReent ;
#endif

	aaTraceTaskSwitch (pAaCurrentTcb->taskIndex,  pAaNextTcb->taskIndex) ;

	pAaCurrentTcb = pAaNextTcb ;

	bspRestoreIrq (intState) ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Tick and delay handling

//--------------------------------------------------------------------------------
//	Put a task in delayed list
//	Internal API
//	Must be called from critical section

void	aaAddToDelayedList_ (aaTcb_t * pTcb, uint32_t delay)
{
	aaListNode_t	* pNode ;

	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;		// Development check
	if (delay == 0)
	{
		return ;
	}

	if (delay == AA_INFINITE)
	{
		// Special case for infinite delay: add at end of list
		pTcb->listNode.value = delay ;
		aaListAddTail (& aaTaskDelayedList, & pTcb->listNode) ;
	}
	else if (aaListIsEmpty (& aaTaskDelayedList) == 1)
	{
		// Empty list add
		pTcb->listNode.value = delay ;
		aaListAddTail (& aaTaskDelayedList, & pTcb->listNode) ;
	}
	else
	{
		// Add non infinite delay to non empty list
		pNode = (aaListNode_t *) & aaTaskDelayedList ;
		while (aaListIsLast (& aaTaskDelayedList, pNode) == 0)
		{
			if (delay <= aaListGetNext (pNode)->value)
			{
				break ;
			}
			delay -= aaListGetNext (pNode)->value ;
			pNode = aaListGetNext (pNode) ;
		}
		// Insert after pNode
		// If pNode is not the last node with finite delay value : adjust next node value
		if (aaListIsLast (& aaTaskDelayedList, pNode) == 0  &&  aaListGetNext (pNode)->value != AA_INFINITE)
		{
			aaListGetNext (pNode)->value -= delay ;
		}
		pTcb->listNode.value = delay ;
		aaListAddAfter (& aaTaskDelayedList, pNode, & pTcb->listNode) ;
	}
}

//--------------------------------------------------------------------------------
//	Remove one node from delayed task list, adjust next task delay
//	Internal API
//	Should be called within critical section

void	aaRemoveFromDelayedList_ (aaListNode_t * pNode)
{
	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;	// Development check

	if (aaListIsLast (& aaTaskDelayedList, pNode) == 0  &&  aaListGetNext (pNode)->value != AA_INFINITE)
	{
		aaListGetNext (pNode)->value += pNode->value ;
	}
	aaListRemove (& aaTaskDelayedList, pNode) ;
}

//--------------------------------------------------------------------------------
//	Tick handling, called by tick interrupt.

void	aaTick_ ()
{
	aaListNode_t	* pNode ;
	aaTcb_t			* pTcb ;

	aaCriticalEnter () ;

	aaTickCount++ ;
	aaTraceTick (aaTickCount) ;

	pNode = aaListGetFirst (& aaTaskDelayedList) ;
	if (aaListIsEnd (& aaTaskDelayedList, pNode) == 0  &&  pNode->value != AA_INFINITE)
	{
		// List is not empty and contain non infinite delay
		pNode->value-- ;
		AA_KERNEL_ASSERT (pNode->value < AA_INFINITE) ;	// Development check looping from 0 to FFFFFFFF

		while (aaListIsEnd (& aaTaskDelayedList, pNode) == 0  &&  pNode->value == 0)
		{
			// Timeout for this node, remove this task from the delayed list.
			pTcb = (aaTcb_t *) pNode->ptr.pOwner ;
			(void) aaRemoveTaskFromLists_ (pTcb) ;
			pTcb->flags |= AA_FLAG_DELAYTMO_ ;		// The cause of woken up is: end of delay

			// If suspend is requested for this task, add it to suspended list, else add it to ready list
			(void) aaTcbPutInList_ (pTcb) ;

			aaCriticalExit () ;
			bspNop () ;
			aaCriticalEnter () ;

			pNode = aaListGetFirst (& aaTaskDelayedList) ;
		}
	}
	aaCriticalExit () ;

	// Timers handling, if software timers are enabled
	#if (AA_TIMER_MAX > 0)
		aaTimerTick_ () ;
	#endif
}

//--------------------------------------------------------------------------------
//	Used by BSP for tick stretching

#if (AA_TICK_STRETCH == 1)

uint32_t	aaTickToWait_		(void)
{
	uint32_t		nTick = AA_INFINITE ;
	aaListNode_t	* pNode ;
	uint32_t		nn ;

	// This code is executed by the idle task: no other task is ready to run, they are all delayed
	pNode = aaListGetFirst (& aaTaskDelayedList) ;
	if (aaListIsEnd (& aaTaskDelayedList, pNode) == 0  &&  pNode->value != AA_INFINITE)
	{
		// The list is not empty and contain non infinite delay
		nTick = pNode->value ;
	}

	nn = aaTimerTickToWait_ () ;
	if (nn < nTick)
	{
		nTick = nn ;
	}

	return nTick ;
}

#endif	// AA_TICK_STRETCH

//--------------------------------------------------------------------------------
//	Used by BSP for tick stretching

#if (AA_TICK_STRETCH == 1)

void		aaUpdateTick_			(uint32_t nTick)
{
	aaListNode_t	* pNode ;

	aaTickCount += nTick ;

	pNode = aaListGetFirst (& aaTaskDelayedList) ;
	if (aaListIsEnd (& aaTaskDelayedList, pNode) == 0  &&  pNode->value != AA_INFINITE)
	{
		// List is not empty and contain non infinite delay
		AA_KERNEL_ASSERT (pNode->value > nTick) ;	// Development check
		pNode->value -= nTick ;
	}

#if (AA_TIMER_MAX != 0)
	aaTimerUpdateTick_ (nTick) ;
#endif
}

#endif	// AA_TICK_STRETCH

//--------------------------------------------------------------------------------
//	If suspend is requested for this task, add it to suspended list,
//	else add it to its ready list
//	Must be called from critical section

uint8_t		aaTcbPutInList_ (aaTcb_t * pTcb)
{
	uint8_t		bRequestSchedule = 0 ;
		
	if ((pTcb->flags & AA_FLAG_SUSPEND_) != 0)
	{
		// Suspend requested, add to suspended list
		pTcb->flags &= (uint16_t) ~AA_FLAG_SUSPEND_ ;
		pTcb->state = aaSuspendedState ;
		aaListAddHead (& aaTaskSuspendedList, & pTcb->listNode) ;
		aaTraceTaskSuspended (pTcb->taskIndex) ;
	}
	else
	{
		// Not suspended, add to ready list
		aaAddReady_ (pTcb) ;
		aaTraceTaskReady (pTcb->taskIndex) ;

		if (pTcb->priority > pAaCurrentTcb->priority)
		{
			bRequestSchedule = 1 ;	// Schedule only if pTcb has higher priority than current task
		}
	}
	return bRequestSchedule ;
}

//--------------------------------------------------------------------------------
//	Return the current tick count

uint32_t	aaGetTickCount ()
{
	uint32_t	tickCount ;

	if (sizeof (aaTickCount) != sizeof (bspBaseType))
	{
		aaIrqStatus_t irqSts ;

		irqSts = bspSaveAndDisableIrq () ;
		tickCount = aaTickCount ;
		bspRestoreIrq (irqSts) ;
	}
	else
	{
		tickCount = aaTickCount ;
	}

	return tickCount ;
}

//--------------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	Set current task in aaIoWaitingState, using parameter driver descriptor
//	Must be called from within a critical section
//	On returning from the function the critical section has been left

//	Timeout can be 0, then the task is not inserted in the delayed list, so 0 is infinite wait.
//		Timeout=0 doesn't mean no wait!
//	If bOrdered != 0, then the task is inserted in ordered driver list.
//		pAaCurrentTcb->waitNode.value must contain the sort value (usually the task priority)
//	Else the task is inserted at tail of the driver list (FIFO)

aaError_t	aaIoWait (aaDriverDesc_t * pDriverDesc, uint32_t bOrdered, uint32_t timeOut)
{
	aaRemoveReady_ (pAaCurrentTcb) ;			// Remove from ready list
	pAaCurrentTcb->state = aaIoWaitingState ;
	if (bOrdered == 0)
	{
		aaListAddTail (& pDriverDesc->waitingList, & pAaCurrentTcb->waitNode) ;
	}
	else
	{
		aaListAddOrdered_ (& pDriverDesc->waitingList, & pAaCurrentTcb->waitNode) ;
	}
	pAaCurrentTcb->objPtr.pIoList = pDriverDesc ;
	aaTraceIoWait (pAaCurrentTcb->taskIndex) ;

	if (timeOut != 0)
	{
		// Put task in delayed tasks list
		pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;	// Clear awake cause flags
		aaAddToDelayedList_ (pAaCurrentTcb, timeOut) ;
	}

	aaCriticalExit () ;
	aaSchedule_ () ;

	// The current task was awakened by the driver, or by timeout, or by aaTaskResume()
	// The task state is aaReadyState
	//	TCB was removed from delayed list and I/O waiting list
	if (timeOut != 0  &&  (pAaCurrentTcb->flags & AA_FLAG_DELAYTMO_) != 0)
	{
		pAaCurrentTcb->flags &= (uint16_t) (~AA_FLAG_DELAYTMO_) ;
		return AA_ETIMEOUT ;
	}
	return AA_ENONE ;
}

//-----------------------------------------------------------------------------
//	Remove a task from aaIoWaitingState, using parameter driver descriptor
//	Can be used by ISR
//	Returns the resumed task TCB

//	Must be called from within a critical section

aaTcb_t *	aaIoResume (aaDriverDesc_t * pDriverDesc)
{
	aaTcb_t		* pTcb ;
	uint32_t	bRequestSchedule ;

	// Sanity check: the waiting list should not be empty
	AA_ASSERT (aaListIsEmpty (& pDriverDesc->waitingList) == 0) ;

	// Remove from head of driver list
	pTcb = (aaTcb_t *) aaListRemoveHead (& pDriverDesc->waitingList)->ptr.pOwner ;

	// Remove task from the list of delayed tasks if it's necessary
	if (0 != aaListIsInUse (& pTcb->listNode))
	{
		aaRemoveFromDelayedList_ (& pTcb->listNode) ;
	}

	// If suspend is requested for this task, add it to suspended list, else add it to ready list
	// pTcb task may be suspended, owning the driver
	bRequestSchedule = aaTcbPutInList_ (pTcb) ;

	// Reschedule if necessary
	if (aaIsInIsr () == 0  &&  bRequestSchedule != 0)
	{
		aaSchedule_ () ;
	}
	return pTcb ;
}

//--------------------------------------------------------------------------------
// Check if a task is waiting for this driver resource, then activate the 1st task

aaTcb_t *	aaIoResumeWaitingTask (aaDriverDesc_t * pDriverDesc)
{
	aaTcb_t		* pTcb = NULL ;

	aaCriticalEnter () ;
	if (aaIsIoWaitingTask (pDriverDesc) != 0u)
	{
		// A thread is waiting to write
		pTcb = aaIoResume (pDriverDesc) ;
	}
	aaCriticalExit () ;
	return pTcb ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// The idle task routine

BSP_ATTR_USED void	aaIdleTask_ (uintptr_t arg)
{
	(void) arg ;

	while (1)
	{
		aaTaskFreeZombies_ () ;

		#if (AA_WITH_LOGMES == 1)
			aaLogMesNext_ () ;
		#endif

		#if (AA_TICK_STRETCH == 1)
		{
			bspTickStretch_ () ;
		}
		#endif // AA_TICK_STRETCH

	}
}

//--------------------------------------------------------------------------------
//	This function is called by a task when it returns from its entry function
//	This function delete the task
//	Can't inline this function because we need a pointer to this function

void	aaTaskEnd_ ()
{
	aaTaskDelete (AA_SELFTASKID) ;
}

//--------------------------------------------------------------------------------
//	First function to call to initialize core kernel state:
//	tasks, scheduler and synchronization objects.

aaError_t	aaInit_ ()
{
	int32_t		ii ;
	uint32_t	jj ;
	aaTcb_t		* pTcb ;

	aaIsRunning			= 0 ;		// Stopped
	aaInIsrCounter		= 0 ;		// Not in ISR
	aaInCriticalCounter	= 0 ;		// Not in critical section
	aaTickCount			= 0 ;		// Init tick counter

	#if (AA_WITH_TASKSTAT == 1)
	{
		aaTsTaskSwitch = 0 ;					// To compute tasks running time
	}
	#endif

	// Initialize lists to empty
	aaStackInitHead (& aaTaskFreeList) ;
	aaListInit    (& aaTaskSuspendedList) ;
	aaListInit    (& aaTaskDeleteList) ;
	aaListInit    (& aaTaskDelayedList) ;

	// Initialize ready list array
	for (jj = 0 ; jj < AA_PRIO_COUNT ; jj++)
	{
		aaListInit (& aaTaskReadyList [jj]) ;
	}

	// Link free task list, from last to the first (free task list is a stack)
	for (ii = AA_TASK_MAX-1 ; ii >= 0 ; ii--)
	{
		pTcb = & aaTcbArray [ii] ;

		pTcb->name[0]	= 't' ;
		pTcb->name[1]	= '_' ;
		pTcb->name[2]	= (char) ((ii / 10) + '0') ;	// Assume no more than 99 tasks!
		pTcb->name[3]	= (char) ((ii % 10) + '0') ;
		pTcb->name[4]	= 0 ;

		pTcb->taskIndex	= (uint16_t) ii ;
		pTcb->state		= aaNoneState ;
		pTcb->cpuUsage	= 0 ;
		pTcb->sigsWait	= 0 ;
		pTcb->sigsRecv	= 0 ;
		aaListClearNode (& pTcb->listNode) ;
		aaListClearNode (& pTcb->waitNode) ;
		pTcb->listNode.ptr.pOwner = pTcb ;		// Link list node to container TCB
		pTcb->waitNode.ptr.pOwner = pTcb ;		// Link list node to container TCB
		aaStackPush (& aaTaskFreeList, (aaStackNode_t *) & pTcb->listNode) ;
	}

	// Clear ready task selector accelerator
	for (jj = 0  ; jj  < aaPRIO_GROUP_COUNT ; jj++)
	{
		aaPrioGroup [jj] = 0 ;
	}
	#if (aaPRIO_GROUP_COUNT > 2)
		aaPrioGroupIndex = 0 ;
	#endif

	// Initialize synchronization objects package
	#if (AA_MUTEX_MAX > 0)
		aaInitMutex_ () ;
	#endif

	#if (AA_SEM_MAX > 0)
		aaInitSemaphore_ () ;
	#endif

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	This function starts the kernel, called only once by bspStart()

void	aaStart_ ()
{
	if (aaIsRunning == 0)
	{
		//	Get the highest priority ready task
		// pAaCurrentTcb already set by aaMain()
		pAaNextTcb = (aaTcb_t *) aaListGetFirst (& aaTaskReadyList [aaGetHighestPrio ()])->ptr.pOwner ;
		aaIsRunning = 1 ;
		bspStart_ () ;
	}
}

//--------------------------------------------------------------------------------
