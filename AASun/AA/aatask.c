/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aatask.c	Kernel tasks routines: aaTaskXxx()

	When		Who	What
	25/11/13	ac	Creation
	12/04/19	ac 	pName may be NULL for aaTaskCreate()
	10/09/19	ac	Add aaTaskCreate() AA_FLAG_SUSPENDED flag management
	26/10/19	ac	Use AA_HTAG_TASK
	21/04/20	ac	Add Newlib _reent management
	09/03/21	ac	Move aaTraceTaskInfo() in aaTaskCreate()
	05/03/22	ac	Rare race condition in aaTaskDelete()
					Add call to newlib_reclaim_reent() in aaTaskFreeZombies_


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
#include	"aakernel.h"
#include	"aalogmes.h"

//--------------------------------------------------------------------------------
//	Get task TCB from task ID
//	TCB may not be in use:  in the list of free tasks or list of deleted tasks => return AA_EARG
//	If taskId is out of range, the function will block (no good TCB value to return)

aaError_t	aaTaskGetTcb_ (aaTaskId_t taskId, aaTcb_t ** ppTcb)
{
	aaError_t	resValue = AA_ENONE ;
	uint32_t	taskIndex ;
	aaTcb_t		* pTcb ;


	if (taskId == AA_SELFTASKID)
	{
		pTcb = pAaCurrentTcb ;
	}
	else
	{
		#if (AA_WITH_ARGCHECK == 1)
		{
			if ((taskId & AA_HTAG_TMASK) != AA_HTAG_TASK)
			{
				// taskId is not a task handle. This is a fatal error
				AA_ERRORNOTIFY (AA_ERROR_TASK_1) ;
				while (1)	// Don't continue, even if AA_ERRORNOTIFY() returns
				{
				}
			}

			taskIndex = taskId & AA_HTAG_IMASK ;
			if (taskIndex < AA_TASK_MAX)
			{
				pTcb = & aaTcbArray [taskIndex] ;

				if (pTcb->state == aaNoneState)
				{
					resValue = AA_EARG ;	// This TCB is not in use, or already deleted
				}
			}
			else
			{
				// This is a fatal error
				AA_ERRORNOTIFY (AA_ERROR_TASK_1) ;
				while (1)	// Don't continue, even if AA_ERRORNOTIFY() returns
				{
				}
			}
		}
		#else
		{
			taskIndex = taskId & AA_HTAG_IMASK ;
			pTcb = & aaTcbArray [taskIndex] ;
		}
		#endif
	}

	* ppTcb = pTcb ;
	return resValue ;
}

//--------------------------------------------------------------------------------
//	Check if a taskId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaTaskIsId		(aaTaskId_t taskId)
{
	uint32_t	taskIndex = taskId & AA_HTAG_IMASK ;
	aaTcb_t		* pTcb = & aaTcbArray [taskIndex] ;
	aaError_t	resValue = AA_EFAIL ;

	if ((taskId & AA_HTAG_TMASK) == AA_HTAG_TASK)
	{
		// The ID have the task tag
		if (taskIndex < AA_TASK_MAX  &&  (pTcb->state != aaNoneState))
		{
			// index is in range, and the task is allocated (not in free list or being deleted)
			resValue = AA_ENONE ;
		}
	}
	return resValue ;
}

//--------------------------------------------------------------------------------
// Effectively release resources of TCB in aaTaskDeleteList

aaError_t	aaTaskFreeTaskStack_ (aaTcb_t * pTcb)
{
	aaError_t	res = AA_ENONE ;

	if (pTcb->flags & AA_FLAG_KERNELSTACK_)
	{
		#if AA_WITH_MALLOC == 1
		{
			// Stack allocated by the kernel
			if (AA_ENONE != aaTryFree (pTcb->pStack))
			{
				// Can't release stack
				res = AA_EFAIL ;
			}
		}
		#else
		{
			// Can't release because dynamic memory management is not allowed
			// Should never happen
			AA_ERRORNOTIFY (AA_ERROR_TASK_2) ;
		}
		#endif		// AA_WITH_MALLOC
	}
	else
	{
		// Stack allocated by user
		if (AA_EFAIL == aaUserReleaseStack ((uint8_t *) pTcb->pStack, pTcb->stackSize))
		{
			// Can't release stack
			res = AA_EFAIL ;
		}
	}
	return res ;
}

//	Walk through aaTaskDeleteList to free the tasks

void	aaTaskFreeZombies_ ()
{
	aaTcb_t			* pTcb ;
	aaTcb_t			* pUnReleasedTcb = NULL ;
	aaError_t		res ;
	uint32_t		ii ;

	for (ii = 0 ; ii < 4 ; ii++)
	{
		aaCriticalEnter () ;
		if (aaListIsEmpty (& aaTaskDeleteList) != 0)
		{
			aaCriticalExit () ;	// No task to delete
			break ;
		}

		// There is a task to delete
		pTcb = (aaTcb_t *) (aaListRemoveHead (& aaTaskDeleteList)->ptr.pOwner) ;
		// Check if we have already tried to release this task
		if (pTcb == pUnReleasedTcb)
		{
			aaListAddHead (& aaTaskDeleteList, & pTcb->listNode) ;
			aaCriticalExit () ;
			break ;
		}
		aaCriticalExit () ;

#if (AA_WITH_NEWLIB_REENT == 1)
		// Release Newlib buffers of the deleted task
		if ((pTcb->flags & AA_FLAG_RECLAIM_REENT_) != 0u)
		{
			newlib_reclaim_reent (& pTcb->newlibReent) ;
			pTcb->flags &= ~AA_FLAG_RECLAIM_REENT_ ;
		}
#endif
		// Free task stack
		res = aaTaskFreeTaskStack_ (pTcb) ;

		aaCriticalEnter () ;
		if (res == AA_EFAIL)
		{
			// Can't release stack, put TCB in aaTaskDeleteList again
			// Memorize the first non released task to avoid infinite loop
			aaListAddTail (& aaTaskDeleteList, & pTcb->listNode) ;
			if (pUnReleasedTcb == NULL)
			{
				pUnReleasedTcb = pTcb ;
			}
		}
		else
		{
			// Stack released, add TCB to free list
			aaStackPush (& aaTaskFreeList, (aaStackNode_t *) & pTcb->listNode) ;
//aaLogMes ("aaTaskFreeZombies_ %08X -----------------> %s\n", (uintptr_t) pTcb->pStack-4, (uintptr_t) pTcb->name, 0, 0, 0) ;
		}
		aaCriticalExit () ;
	}
}

//--------------------------------------------------------------------------------
//	Create new task
//	pStack may be NULL => stack allocated by the kernel
//	stackSize is a count of bspStackType_t
//	flags : AA_FLAG_STACKCHECK

aaError_t	aaTaskCreate (uint32_t prio, const char * pName, aaTaskFunction_t pEntry, uintptr_t arg, bspStackType_t * pStack, uint32_t stackSize, uint32_t flags, aaTaskId_t * pTaskId)
{
	aaListNode_t	* pListNode ;
	aaTcb_t			* pTcb ;
	uint32_t		ii ;

	// Free old tasks resources
	// This can free up a TCB and memory if a task is waiting to be freed
	aaTaskFreeZombies_ () ;

	if (pTaskId != NULL)
	{
		* pTaskId = AA_INVALID_TASK ;
	}

	// Check alignment of user supplied stack pointer
// TODO : define alignment in config. ARM require 8 byte alignment
	if ((pStack != NULL) &&  ((uintptr_t) pStack % sizeof (bspStackType_t) != 0))
	{
		if (pAaCurrentTcb != NULL)
		{
			aaTraceTaskCreateFail (pAaCurrentTcb->taskIndex) ;
		}
		AA_ERRORNOTIFY (AA_ERROR_TASK_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (prio >= AA_PRIO_COUNT)
	{
		if (pAaCurrentTcb != NULL)
		{
			aaTraceTaskCreateFail (pAaCurrentTcb->taskIndex) ;
		}
		AA_ERRORNOTIFY (AA_ERROR_TASK_4 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	// Task priority must not be equal to idle task prio
	if (prio == 0)
	{
		// The idle task is the first created task, so it use aaTcbArray [0]
		// Allows priority 0 only if we are creating the first task
		if ((aaListNode_t *) aaStackPeek (& aaTaskFreeList) != & aaTcbArray [0].listNode)
		{
			if (pAaCurrentTcb != NULL)
			{
				aaTraceTaskCreateFail (pAaCurrentTcb->taskIndex) ;
			}
			AA_ERRORNOTIFY (AA_ERROR_TASK_5 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;
		}
	}

	// Get free TCB
	aaCriticalEnter () ;
	if (aaStackIsEmpty (& aaTaskFreeList) == 1)
	{
		if (pAaCurrentTcb != NULL)
		{
			// Warning: don't trace if idle task not created
			aaTraceTaskCreateFail (pAaCurrentTcb->taskIndex) ;
		}
		AA_ERRORNOTIFY (AA_ERROR_TASK_6 | AA_ERROR_FORCERETURN_FLAG) ;
		aaCriticalExit () ;
		return AA_EDEPLETED ;
	}

	pListNode = (aaListNode_t *) aaStackPop (& aaTaskFreeList) ;
	aaCriticalExit () ;

	// Initialize TCB
	pTcb = (aaTcb_t *) pListNode->ptr.pOwner ;
	aaListInit (& pTcb->mutexList) ;
	pTcb->priority		= (uint8_t) prio ;
	pTcb->basePriority	= (uint8_t) prio ;
	pTcb->flags			= (uint16_t) (flags & AA_FLAG_USERMASK_) ;
	pTcb->cpuUsage		= 0 ;

	// Initialize stack
	pTcb->stackSize	= (uint16_t) stackSize ;
	if (pStack == NULL)
	{
		// Should allocate stack
		#if AA_WITH_MALLOC == 1
		{
			pTcb->pStack = (bspStackType_t *) aaMalloc (stackSize * sizeof (bspStackType_t)) ;
			if (pTcb->pStack == NULL)
			{
				// Memory allocation error, release TCB
				pTcb->state = aaNoneState ;
				aaCriticalEnter () ;
				aaStackPush (& aaTaskFreeList, (aaStackNode_t *) & pTcb->listNode) ;
				aaCriticalExit () ;
				return AA_EMEMORY ;
			}
			pTcb->flags |= AA_FLAG_KERNELSTACK_ ;		// Stack allocated by the kernel
		}
		#else
		{
			// Dynamic memory management not allowed
			AA_ERRORNOTIFY (AA_ERROR_TASK_7) ;
		}
		#endif		// AA_WITH_MALLOC
	}
	else
	{
		// The application supply the stack
		pTcb->pStack = (bspStackType_t *) pStack ;
	}

	// Initialize stack for monitoring only if it is requested
	if ((pTcb->flags & AA_FLAG_STACKCHECK) != 0)
	{
		for (ii = 0 ; ii < pTcb->stackSize ; ii++)
		{
			pTcb->pStack [ii] = (bspStackType_t) bspSTACK_PATTERN ;
		}
	}

	// Copy task name, pName may be NULL
	ii = 0 ;
	if (pName != NULL)
	{
		for ( ; (pName[ii] != 0  &&  ii < AA_TASK_NAME_SIZE-1) ; ii++)
		{
			pTcb->name[ii] = pName[ii] ;
		}
	}
	pTcb->name[ii] = 0 ;

	// Initialize task stack context
	bspTaskInit_ (pTcb, pEntry, arg) ;

#if (AA_WITH_NEWLIB_REENT == 1)
	_REENT_INIT_PTR (& pTcb->newlibReent) ;
#endif

	// Send traces before the task can be scheduled
	aaTraceTaskCreated (pTcb->taskIndex) ;

	if (pTaskId != NULL)
	{
		* pTaskId = pTcb->taskIndex | AA_HTAG_TASK ;
	}

	// Put TCB in ready or suspended list
	aaCriticalEnter () ;
	if ((flags & AA_FLAG_SUSPENDED) != 0)
	{
		pTcb->state = aaSuspendedState ;							// Change task state to suspended
		aaListAddHead (& aaTaskSuspendedList, & pTcb->listNode) ;	// Add to suspended tasks list
	}
	else
	{
		aaAddReady_ (pTcb) ;
	}
	aaTraceTaskInfo    (pTcb->taskIndex | AA_HTAG_TASK) ;
	aaCriticalExit () ;

	if ((flags & AA_FLAG_SUSPENDED) == 0)
	{
		aaSchedule_ () ;
	}

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	WARNING: this function doesn't use critical section.
//	The task should not be deleted while calling this function

aaError_t	aaTaskCheckStack (aaTaskId_t taskId, uint32_t * pFreeSpace)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;
	uint32_t		ii ;

	AA_ERRORASSERT (pFreeSpace != NULL, AA_ERROR_TASK_8) ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue == AA_ENONE)
	{
		* pFreeSpace = 0 ;
		if ((pTcb->flags & AA_FLAG_STACKCHECK) != 0)
		{
			// Stack check was requested for this task by aaTaskCreate()
			for (ii = 0 ; ii < pTcb->stackSize ; ii++)
			{
				if (pTcb->pStack [ii] != (bspStackType_t) bspSTACK_PATTERN)
				{
					* pFreeSpace = ii ;		// Free stack words
					resValue = AA_ENONE ;
					break ;
				}
			}
		}
		else
		{
			// No stack check requested for this task by aaTaskCreate()
			resValue = AA_ESTATE ;
		}
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
//	Remove the task from all delayed lists. Ready lists and suspended list are not delayed lists
//	The state of the task is not changed
//	To call from critical section

aaError_t	aaRemoveTaskFromLists_ (aaTcb_t * pTcb)
{
	aaListHead_t	* pList ;

	// Remove task from the list of delayed tasks if it's necessary
	if (0 != aaListIsInUse (& pTcb->listNode))
	{
		aaRemoveFromDelayedList_ (& pTcb->listNode) ;
	}
		
	if (pTcb->state == aaDelayedState  ||  pTcb->state == aaSignalWaitingState)
	{
		return AA_ENONE ;	// Removed from lists
	}

	if (pTcb->state == aaQueueWaitingState)
	{
		// Task is waiting for queue: Remove task from queue waiting list
		if ((pTcb->flags & AA_FLAG_QUEUEPUT_) != 0)
		{
			aaListRemove (& pTcb->objPtr.pQueue->putWaitingList, & pTcb->waitNode) ;
			pTcb->flags &= (uint16_t) ~AA_FLAG_QUEUEPUT_ ;
		}
		else
		{
			aaListRemove (& pTcb->objPtr.pQueue->getWaitingList, & pTcb->waitNode) ;
		}
		pTcb->objPtr.pQueue = NULL ;
	}
	else if (pTcb->state == aaMutexWaitingState)
	{
		// If AA_MUTEX_MAX==0, then a task can't be in aaMutexWaitingState.
#if (AA_MUTEX_MAX > 0)
		aaTcb_t		* pOwner = pTcb->objPtr.pMutex->pOwner ;

		// The task is in mutex waiting list, remove from this list, the cause is timeout
		pList = & pTcb->objPtr.pMutex->waitingList ;
		aaListRemove (pList, & pTcb->waitNode) ;

		// Check for priority inheritance:
		// If the priority of the owner task of the mutex equals the priority of pTcb
		if (pOwner->priority == pTcb->priority)
		{
			// And if the owner task of the mutex has an inherited priority
			if (pOwner->priority != pOwner->basePriority)
			{
				// Then we should compute the new priority for the owner task of the mutex
				aaMutexNewPrio_ (pOwner) ;
			}
		}
		pTcb->objPtr.pMutex = NULL ;
#endif
	}
	else if (pTcb->state == aaSemWaitingState)
	{
		// The task is in sem waiting list, remove
		pList = & pTcb->objPtr.pSem->waitingList ;
		aaListRemove (pList, & pTcb->waitNode) ;
		pTcb->objPtr.pSem = NULL ;
	}
	else if (pTcb->state == aaIoWaitingState)
	{
		// Remove from driver list
		aaListRemove (& pTcb->objPtr.pIoList->waitingList, & pTcb->waitNode) ;
	}
	else
	{
		// Other states
		return AA_ESTATE ;	// The task is not in any delayed list
	}

	return AA_ENONE ;	// Removed from lists
}

//--------------------------------------------------------------------------------
//	A task is not destroyed immediately:
//	Its state is changed : it is placed in the list of deleted tasks and become zombie
//	The task is really destroyed later by the aaTaskFreeZombies_() function.

//	If the current task id is used (the task destroys itself), this function will never return.

aaError_t	aaTaskDelete	(aaTaskId_t taskId)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;

	// Get target task
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue != AA_ENONE)
	{
		return resValue ;
	}

	// Should not delete idle task
	if (pTcb->basePriority == 0)
	{
		AA_ERRORNOTIFY (AA_ERROR_TASK_9 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;

	aaTraceTaskDeleted (pTcb->taskIndex) ;

	// Remove TCB from lists
	if (pTcb->state == aaReadyState)
	{
		// In ready state
		aaRemoveReady_ (pTcb) ;
	}
	else if (pTcb->state == aaSuspendedState)
	{
		// Suspended
		aaListRemove (& aaTaskSuspendedList, & pTcb->listNode) ;
	}
	else
	{
		// Other states
	 	aaRemoveTaskFromLists_ (pTcb) ;
	}

	pTcb->state = aaNoneState ;		// Unused or being deleted task

	if (pTcb == pAaCurrentTcb)
	{
		// The task destroys itself, then it is necessary to delegate the task disposal
		// then switch to another ready task.
		pTcb->flags |= AA_FLAG_RECLAIM_REENT_ ;	// The release of newlib's buffers is required
		aaListAddTail (& aaTaskDeleteList, & pTcb->listNode) ;
		aaCriticalExit () ;
		aaSchedule_() ;	// Will never return.
	}

	aaCriticalExit () ;

	// The task delete another task: try to free the task immediately

	// We want to call aaTaskFreeTaskStack_() out of a critical section
	// because it calls a user callback potentially long
	resValue = aaTaskFreeTaskStack_ (pTcb) ;

#if (AA_WITH_NEWLIB_REENT == 1)
	// Release Newlib buffers of the deleted task
	newlib_reclaim_reent (& pTcb->newlibReent) ;
#endif

	// Put the deleted task TCB in the appropriate list
	aaCriticalEnter () ;

	if (resValue == AA_EFAIL)
	{
		// Can't release stack, put TCB in aaTaskDeleteList, the stack will be freed by aaTaskFreeZombies_()
		aaListAddTail (& aaTaskDeleteList, & pTcb->listNode) ;
	}
	else
	{
		// Stack and newlib released, add tcb to free tcblist
		aaStackPush (& aaTaskFreeList, (aaStackNode_t *) & pTcb->listNode) ;
//aaLogMes ("task deleted %08X -----------------> %s\n", (uintptr_t) pTcb->pStack-4, (uintptr_t) pTcb->name, 0, 0, 0) ;
	}

	aaCriticalExit () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------

aaError_t		aaTaskGetName		(aaTaskId_t taskId, const char ** ppName)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		AA_ERRORCHECK (ppName != NULL, AA_EARG, AA_ERROR_TASK_11) ;
	}
	#endif

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue == AA_ENONE)
	{
		* ppName =  pTcb->name ;
	}
	return resValue ;
}

//--------------------------------------------------------------------------------
//	Schedule another task of same priority

void	aaTaskYield		(void)
{
	uint8_t	bNeedSchedule = 0 ;

	if (aaIsInIsr () != 0)
	{
		// Not allowed from ISR
		AA_ERRORNOTIFY (AA_ERROR_TASK_12 | AA_ERROR_FORCERETURN_FLAG) ;
		return ;
	}

	if (aaIsInCritical() != 0)
	{
		AA_ERRORNOTIFY (AA_ERROR_TASK_14 | AA_ERROR_FORCERETURN_FLAG) ;
		return ;	// In critical section, ignore
	}

	aaCriticalEnter () ;
	if (aaListCount1 (& aaTaskReadyList [pAaCurrentTcb->priority]) == 0)
	{
		// There is at least one task in the queue (pAaCurrentTcb)
		// So if aaListCount1() is false, there is more than 1 item in the list
		aaRemoveReady_ (pAaCurrentTcb) ;		// Remove this task from head
		aaAddReady_ (pAaCurrentTcb) ;		// Add to tail
		bNeedSchedule = 1 ;
	}
	aaCriticalExit () ;

	if (bNeedSchedule != 0)
	{
		aaSchedule_ () ;
	}
}

//--------------------------------------------------------------------------------
//	Put current task in delayed tasks list

void	aaTaskDelay (uint32_t delay)
{
	if (aaIsInIsr () != 0)
	{
		// Not allowed from ISR
		AA_ERRORNOTIFY (AA_ERROR_TASK_13 | AA_ERROR_FORCERETURN_FLAG) ;
		return ;
	}

	aaCriticalEnter () ;

	// The current task is in a ready list (at head)
	aaRemoveReady_ (pAaCurrentTcb) ;					// Remove from ready list
	pAaCurrentTcb->state = aaDelayedState ;			// Change task state to delayed
	aaAddToDelayedList_ (pAaCurrentTcb, delay) ;
	aaTraceTaskDeleyed  (pAaCurrentTcb->taskIndex) ;

	aaCriticalExit () ;

	aaSchedule_ () ;

	pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;	// For clarity only: clear AA_FLAG_DELAYTMO_ on return from delay
}

//--------------------------------------------------------------------------------
//	Wake up a task that is in a delayed or waiting state
//	On return AA_FLAG_DELAYTMO_ flag is set

aaError_t	aaTaskWakeUp		(aaTaskId_t taskId)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;
	uint8_t			bRequestSchedule = 0 ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue == AA_ENONE)
	{
		aaCriticalEnter () ;
		if (pTcb->state == aaReadyState  ||  pTcb->state == aaSuspendedState  ||  pTcb->state == aaNoneState)
		{
			resValue = AA_ESTATE ;
		}
		else
		{
			// Other states
		 	aaRemoveTaskFromLists_ (pTcb) ;
			pTcb->flags |= AA_FLAG_DELAYTMO_ ;		// The cause of woken up is: end of delay

			// If suspend is requested for this task, add it to suspended list, else add it to a ready list
			bRequestSchedule =  aaTcbPutInList_ (pTcb) ;
		}

		aaCriticalExit () ;
		if (bRequestSchedule != 0)
		{
			aaSchedule_ () ;
		}
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
//	Return the base priority of a task
//	The base priority may not be the same as current priority (mutex priority inheritance)

aaError_t	aaTaskGetBasePriority (aaTaskId_t taskId, uint8_t * pBasePriority)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue == AA_ENONE)
	{
		* pBasePriority = pTcb->basePriority ;
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
//	Return the current priority of a task
//	The base priority may not be the same as current priority (mutex priority inheritance)

aaError_t	aaTaskGetRealPriority (aaTaskId_t taskId, uint8_t * pRealPriority)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue == AA_ENONE)
	{
		* pRealPriority = pTcb->priority ;
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
//	Change the base priority of a task

aaError_t	aaTaskSetPriority (aaTaskId_t taskId, uint8_t newBasePriority)
{
	aaTcb_t			* pTcb ;	// The task whose priority must be changed
	aaError_t		resValue ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue != AA_ENONE)
	{
		return resValue ;
	}

	// Can't change idle task priority, can't set TCB to priority 0
	if (pTcb->basePriority == 0  ||  newBasePriority == 0  || newBasePriority >= AA_PRIO_COUNT)
	{
		return AA_EARG ;
	}

	aaCriticalEnter () ;

	// Priority inheritance must be taken into account
	// If the task own mutexes, adjust the task effective priority to priorities of tasks waiting for the mutexes
	pTcb->basePriority = newBasePriority ;
	aaMutexNewPrio_ (pTcb) ;	// This will set pTcb->priority, and set the task in the appropriate lists (ready, waiting, ...)

	// If the task is waiting for a mutex, then it must propagate its new priority to the mutex's owner task
	if (pTcb->state == aaMutexWaitingState)
	{
		aaMutexPropagate_ (pTcb) ;
	}

	// Trace only if no inherited priority
	if (pTcb->basePriority == pTcb->priority)
	{
		aaTraceTaskNewPrio (pTcb->taskIndex, newBasePriority) ;
	}

	aaCriticalExit () ;

	aaSchedule_ () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Return the id of the current task
//	This function can't be inlined because applications doesn't includes kernel.h, hence doesn't know pAaCurrentTcb

aaTaskId_t		aaTaskSelfId	(void)
{
	return pAaCurrentTcb->taskIndex | AA_HTAG_TASK ;
}

//--------------------------------------------------------------------------------
//	Suspend a specified task, suspension is additive:
//	Delayed task: remain in delayed tasks tasks list, when delay expire go in suspended tasks list
//	Waiting for sync object: remain in waiting tasks list, when sync is obtained or timed out go in suspended tasks list
//	Ready task : go in suspended tasks list then eventually schedule.

//	Warning: If a task is suspended while having sync object (sem, mutex, ...) the facilities engaged are unavailable.
//	This can be catastrophic if the sync object guard some system resource.

aaError_t	aaTaskSuspend (aaTaskId_t taskId)
{
	aaTcb_t		* pTcb ;
	aaError_t	resValue  ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue != AA_ENONE)
	{
		return resValue ;
	}

	aaCriticalEnter () ;

	if (pTcb->state == aaReadyState)
	{
		aaRemoveReady_ (pTcb) ;										// Remove from ready tasks list
		pTcb->state = aaSuspendedState ;							// Change task state to suspended
		aaListAddHead (& aaTaskSuspendedList, & pTcb->listNode) ;	// Add to suspended tasks list
		aaTraceTaskSuspended (pTcb->taskIndex) ;
	}
	else
	{
		pTcb->flags |= AA_FLAG_SUSPEND_ ;	// Mark task for suspension
	}

	aaCriticalExit () ;

	// If we self suspend, schedule another ready task 
	if (pTcb == pAaCurrentTcb)
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Remove task from suspended tasks list, and add it to ready tasks list

aaError_t	aaTaskResume (aaTaskId_t taskId)
{
	aaTcb_t		* pTcb ;
	uint8_t		bScheduleRequest = 0 ;
	aaError_t	resValue  ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	if (resValue != AA_ENONE)
	{
		return resValue ;
	}
	if (pTcb == pAaCurrentTcb)
	{
		return AA_ENONE ;	// The current task is not suspended!
	}

	aaCriticalEnter () ;

	if (pTcb->state == aaSuspendedState)
	{
		// In suspended tasks list, ready this task
		aaListRemove (& aaTaskSuspendedList, & pTcb->listNode) ;
		aaTraceTaskResumed (pTcb->taskIndex) ;
		aaAddReady_ (pTcb) ;
		if (pTcb->priority > pAaCurrentTcb->priority)
		{
			bScheduleRequest = 1 ;
		}
	}
	else
	{
		// Not in suspended list, only remove suspended flag
		pTcb->flags &= (uint16_t) ~AA_FLAG_SUSPEND_ ;
	}

	aaCriticalExit () ;

	if (bScheduleRequest != 0)
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------

aaError_t	aaTaskInfo	(aaTaskInfo_t * pInfo, uint32_t size, uint32_t * pReturnSize, uint32_t * pCpuTotal, uint32_t * pCriticalUsage, uint32_t flags)
{
	uint32_t		ii ;		// Loop index
	uint32_t		nn ;		// Count of items in pInfo ;
	aaTaskInfo_t	* pI ;		// Pointer in pInfo array
	aaTcb_t			* pTcb ;	// Pointer in aaTcbArray

	// Recover info from task, except stack free
	aaCriticalEnter () ;
	pTcb = & aaTcbArray [0] ;
	pI   = & pInfo [0] ;
	nn   = 0 ;
	for (ii = 0 ; ii < AA_TASK_MAX  &&  nn < size ; ii++)
	{
		if (pTcb->state != aaNoneState)
		{
			pI->taskId			= pTcb->taskIndex | AA_HTAG_TASK ;
			pI->state			= pTcb->state ;
			pI->priority		= pTcb->priority ;
			pI->basePriority	= pTcb->basePriority ;
			pI->cpuUsage		= pTcb->cpuUsage ;
			pI->stackFree		= 0 ;
			pI++ ;
			nn++ ;
		}
		pTcb++ ;
	}
	#if (AA_WITH_CRITICALSTAT == 1)
	{
		* pCriticalUsage = aaCriticalUsage ;
	}
	#else
	{
		* pCriticalUsage = 0 ;
	}
	#endif
	* pCpuTotal = aaCpuUsage ;
	* pReturnSize = nn ;
	aaCriticalExit () ;

	// Recover tasks stack free space, can be lengthy
	pI = pInfo  ;
	for (ii = 0 ; ii < nn ; ii++)
	{
		(void) aaTaskCheckStack (pI->taskId, & pI->stackFree) ;
		pI++ ;
	}

	(void) flags;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------

void	aaTaskStatClear	(void)
{
	uint32_t	ii ;		// Loop index
	aaTcb_t		*  pTcb ;	// Pointer in aaTcbArray

	aaCriticalEnter () ;
	pTcb = & aaTcbArray [0] ;
	for (ii = 0 ; ii < AA_TASK_MAX  ; ii++)
	{
		pTcb->cpuUsage = 0 ;
		pTcb++ ;
	}
	aaCpuUsage = 0 ;

	#if (AA_WITH_CRITICALSTAT == 1)
		aaCriticalUsage = 0 ;
	#endif

	aaCriticalExit () ;
}

//--------------------------------------------------------------------------------
