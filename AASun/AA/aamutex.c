/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aamutex.c	Task exclusion : mutex

	When		Who	What
	25/11/13	ac	Creation
	01/06/20	ac	Allow AA_MUTEX_MAX to be 0
	16/12/20	ac	Bug in aaMutexPropagate_() (infinite loop)
	21/12/23	ac	Change pTask by pOwner in "pTask->state == aaQueueWaitingState" of aaMutexPropagate_()

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

#define		MUTEX_MAX_VALUE		(32767)			// Max value for mutex count
#define		MUTEX_FREE_TAG		(0xFFFFFFFFu)

// For development traces and checks
#define		IFDEBUG		if(0)

#if (AA_MUTEX_MAX > 0)

//--------------------------------------------------------------------------------
// Initialize synchronization objects free list at kernel start up

void	aaInitMutex_ ()
{
	aaMutex_t	* pMutex = & aaMutexArray [AA_MUTEX_MAX-1] ;
	uint32_t	ii ;

	// Add items in free list in reverse order
	// Simpler to debug

	aaStackInitHead (& aaMutexFreeList) ;
	for (ii = 0 ; ii < AA_MUTEX_MAX ; ii++)
	{
		pMutex->mutexIndex = (aaMutexId_t) (AA_MUTEX_MAX - 1 - ii) ;
		pMutex->pOwner = (aaTcb_t *) MUTEX_FREE_TAG ;

		// mutexNode is used as a node in aaMutexFreeList, and as a list head when the mutex is created
		aaStackPush (& aaMutexFreeList, (aaStackNode_t *) & pMutex->mutexNode) ;

		pMutex-- ;
	}
}

//--------------------------------------------------------------------------------
//	Check if a mutexId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaMutexIsId	(aaMutexId_t mutexId)
{
	uint32_t	mutexIndex = mutexId & AA_HTAG_IMASK ;
	aaMutex_t	* pMutex = & aaMutexArray [mutexIndex] ;
	aaError_t	resValue = AA_EFAIL ;

	if ((mutexId & AA_HTAG_TMASK) == AA_HTAG_MUTEX)
	{
		// The ID have the mutex tag
		if ((mutexIndex < AA_MUTEX_MAX)  &&  (pMutex->pOwner != (aaTcb_t *) MUTEX_FREE_TAG))
		{
			// Index is in the range of mutex descriptor array, and allocated
			resValue = AA_ENONE ;
		}
	}
	return resValue ;
}

//--------------------------------------------------------------------------------
// Create mutex synchronization object

aaError_t	aaMutexCreate (aaMutexId_t * pMutexId)
{
	aaMutex_t		* pMutex ;

	* pMutexId = AA_INVALID_MUTEX ;

	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_MTX_1) ;

	// Search for free mutex block
	aaCriticalEnter () ;
	if (aaStackIsEmpty (& aaMutexFreeList) == 1)
	{
		// The mutex free list is empty
		//  pAaCurrentTcb may be NULL if the mutex is created before kernel is started
		if (pAaCurrentTcb == NULL)
		{
			aaTraceMutexCreateFail (0) ;
		}
		else
		{
			aaTraceMutexCreateFail (pAaCurrentTcb->taskIndex) ;
		}
		AA_ERRORNOTIFY (AA_ERROR_MTX_2 | AA_ERROR_FORCERETURN_FLAG) ;
		aaCriticalExit () ;
		return AA_EDEPLETED ;
	}
	pMutex = (aaMutex_t *) aaStackPop (& aaMutexFreeList) ;
	aaListClearNode (& pMutex->mutexNode) ;

	aaCriticalExit () ;

	pMutex->count = 0 ;
	aaListInit (& pMutex->waitingList) ;
	pMutex->pOwner = NULL ;
	aaTraceMutexCreated (pMutex->mutexIndex) ;
	* pMutexId = pMutex->mutexIndex | AA_HTAG_MUTEX ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Free mutex: add to free list
//	Mutex must be available (count = 0) to be deleted

aaError_t	aaMutexDelete (aaMutexId_t mutexId)
{
	uint32_t		mutexIndex = mutexId & AA_HTAG_IMASK ;
	aaMutex_t		* pMutex = & aaMutexArray [mutexIndex] ;

	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_MTX_3) ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		if ((mutexId & AA_HTAG_TMASK) != AA_HTAG_MUTEX  ||  mutexIndex >= AA_MUTEX_MAX)
		{
			// Not a valid mutex ID
			aaTraceMutexDeleteFail ((uint8_t) AA_INVALID_MUTEX) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_4 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;			// Not a valid mutex ID
		}

		if (pMutex->pOwner == (aaTcb_t *) MUTEX_FREE_TAG)
		{
			// This is not an allocated mutex
			aaTraceMutexDeleteFail (mutexIndex) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_5 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;		// This is not an allocated mutex
		}
	}
	#endif

	// Check and add to list
	aaCriticalEnter () ;
	if (pMutex->count != 0)
	{
		// Mutex not available, can't free
		aaTraceMutexDeleteFail (mutexIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_MTX_6 | AA_ERROR_NORETURN_FLAG) ;
		aaCriticalExit () ;
		return AA_ESTATE ;
	}

	pMutex->pOwner = (aaTcb_t *) MUTEX_FREE_TAG ;
	aaStackPush (& aaMutexFreeList, (aaStackNode_t *) & pMutex->mutexNode) ;
	aaTraceMutexDeleted (mutexIndex) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Propagate the priority of the task waiting for the mutex to the task that owns the mutex
//	Warning: the task that owns the mutex may itself be waiting for a mutex owned by another task, hence the loop
//	Kernel internal use only
//	Must be called from within a critical section
//	WARNING: To call only if the task pTask is waiting for a mutex

void	aaMutexPropagate_ (aaTcb_t * pTask)
{
	aaMutex_t	* pMutex = pTask->objPtr.pMutex ;
	aaTcb_t		* pOwner = pMutex->pOwner ;

	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;		// Development check

	while (pOwner->priority < pTask->priority)
	{
		// Must increase the task priority of the mutex owner
		aaTraceTaskInheritPriority (pOwner->taskIndex, pTask->priority) ;
		if (pOwner->state == aaReadyState)
		{
			aaRemoveReady_ (pOwner) ;				// Remove from ready list
			pOwner->priority = pTask->priority ;	// Set new priority
			aaAddReady_ (pOwner) ;					// Set in new ready list
		}
		else if (pOwner->state   == aaMutexWaitingState  ||
				 pOwner->state   == aaSemWaitingState    ||
				 ((pOwner->state == aaQueueWaitingState) && ((pOwner->objPtr.pQueue->flags & AA_QUEUE_PRIORITY_) != 0)))
		{
			// The owner of this mutex is in the priority ordered waiting list of a mutex or sem or queue.
			// Must increase its priority
			// If we change its priority, we must change its position in the waiting list
			aaListHead_t	* pList  ;

			if (pOwner->state == aaMutexWaitingState)
			{
				pList = & pOwner->objPtr.pMutex->waitingList ;
			}
			else if (pOwner->state == aaSemWaitingState)
			{
				pList = & pOwner->objPtr.pSem->waitingList ;
			}
			else
			{
				// Queue list, put list or get list ?
				if ((pTask->flags & AA_FLAG_QUEUEPUT_) != 0)
				{
					pList = & pTask->objPtr.pQueue->putWaitingList ;
				}
				else
				{
					pList = & pTask->objPtr.pQueue->getWaitingList ;
				}
			}

			// Set the new priority and set the tack at the right position in the list
			pOwner->priority = pTask->priority ;
			if (aaListCount1 (pList) != 1)
			{
				// There is at least one task in the mutex/sem waiting list
				// So if aaListCount1() is false, there is more than 1 item in the list
				aaListRemove (pList, & pOwner->waitNode) ;
				aaListAddOrdered_ (pList, & pOwner->waitNode) ;
			}

			if (pOwner->state == aaMutexWaitingState)
			{
				pMutex = pOwner->objPtr.pMutex ;
				pOwner = pMutex->pOwner ;
				continue ;		// Propagate the priority to the task pOwner
			}
			else
			{
				// pOwner is not waiting for a mutex, this is the end of the propagate
			}
		}
		else
		{
			// This task is in another not ordered list (delayed, suspended, deleted ...), just change its priority
			// This is the end of the propagate
			pOwner->priority = pTask->priority ;
		}
		break ;
	}
}

//--------------------------------------------------------------------------------
//	Finds and sets the priority of the task according to owned mutexes, and priority of tasks waiting for theses mutexes.
//	Kernel internal use only
//	Must be called from within a critical section
//	Return 1 if scheduling is required (pTask is ready and its priority has changed)

uint32_t	aaMutexNewPrio_ (aaTcb_t * pTask)
{
	aaListNode_t	* pNode ;
	uint32_t		highestPrio ;
	uint8_t			bRequestSchedule = 0 ;

	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;		// Development check

	//	Finds the highest priority of tasks waiting for mutexes in pTask mutexList.
	//	The lowest priority value is pTask basePriority (can rise, can't drop base priority of task)
	highestPrio = pTask->basePriority ;

	if (aaListIsEmpty (& pTask->mutexList) == 1)
	{
		// List of owned mutexes not empty
		pNode = aaListGetFirst (& pTask->mutexList) ;
		while (aaListIsEnd (& pTask->mutexList, pNode) == 0)
		{
			if (aaListIsEmpty (& (((aaMutex_t *) pNode)->waitingList)) == 0)
			{
				// If the waiting task list of this mutex is not empty, check prio value of task at head of the waiting list
				if (aaListGetFirst (& ((aaMutex_t *) pNode)->waitingList)->value > highestPrio)
				{
					highestPrio = aaListGetFirst (& ((aaMutex_t *) pNode)->waitingList)->value ;
				}
			}
			pNode = aaListGetNext (pNode) ;
		}
	}

	// Should change pTask current priority ?
	if (pTask->priority != highestPrio)
	{
		// Yes.
		aaListHead_t	* pList ;

		// Trace only inherited priority
		if (pTask->priority != highestPrio)
		{
			// Inherited priority
			aaTraceTaskInheritPriority (pTask->taskIndex, highestPrio) ;
IFDEBUG { aaLogMes ("MGive lower %s p=%u -> p=%u\n", (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, highestPrio, 0, 0) ; }
		}

		if (pTask->state == aaReadyState)
		{
			aaRemoveReady_ (pTask) ;			// We are executing, so remove from the ready list
			pTask->priority = (uint8_t) highestPrio ;	// Change priority
			aaAddReady_ (pTask) ;			// Set in new ready list
			bRequestSchedule = 1 ;
		}
		else if (pTask->state == aaMutexWaitingState  ||
				 pTask->state == aaSemWaitingState	  ||
				 ((pTask->state == aaQueueWaitingState) && ((pTask->objPtr.pQueue->flags & AA_QUEUE_PRIORITY_) != 0)))
		{
			// The task is in a priority ordered waiting list, and we should change the task priority.
			// If we change its priority, we should change its position in waiting tasks list
			// If there is only one item in the list, no need to remove and insert again
			if (pTask->state == aaMutexWaitingState)
			{
				pList = & pTask->objPtr.pMutex->waitingList ;
			}
			else if (pTask->state == aaSemWaitingState)
			{
				pList = & pTask->objPtr.pSem->waitingList ;
			}
			else
			{
				// Queue list
				if ((pTask->flags & AA_FLAG_QUEUEPUT_) != 0)
				{
					pList = & pTask->objPtr.pQueue->putWaitingList ;
				}
				else
				{
					pList = & pTask->objPtr.pQueue->getWaitingList ;
				}
			}

			pTask->priority = (uint8_t) highestPrio ;
			pTask->waitNode.value = highestPrio ;
			if (aaListCount1 (pList) == 0)
			{
				aaListRemove     (pList, & pTask->waitNode) ;
				aaListAddOrdered_ (pList, & pTask->waitNode) ;
			}
		}
		else
		{
			// Delayed, suspended or deleted
			pTask->priority = (uint8_t) highestPrio ;
		}
	}
	return bRequestSchedule ;
}

//--------------------------------------------------------------------------------
//	If timeout is 0 : this is a aaMutexTryTake

aaError_t	aaMutexTake (aaMutexId_t mutexId, uint32_t timeOut)
{
	uint32_t		mutexIndex = mutexId & AA_HTAG_IMASK ;
	aaMutex_t		* pMutex = & aaMutexArray [mutexIndex] ;

	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_MTX_7) ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		// Valid mutex ID ?
		if ((mutexId & AA_HTAG_TMASK) != AA_HTAG_MUTEX  ||   mutexIndex >= AA_MUTEX_MAX)
		{
			// Not a valid mutex ID
			aaTraceMutexTakeFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_MUTEX) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_8 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;			// Not a valid mutex ID
		}

		if (pMutex->pOwner == (aaTcb_t *) MUTEX_FREE_TAG)
		{
			// This is not an allocated mutex
			aaTraceMutexTakeFail (pAaCurrentTcb->taskIndex, mutexIndex) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_9 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;
		}
	}
	#endif

	if (aaIsRunning == 0)
	{
		return AA_ENONE ;	// Always OK if kernel is not running
	}

	aaCriticalEnter () ;
	if (pMutex->count == 0)
	{
		// Mutex is available, take it
		pMutex->count ++ ;
		pMutex->pOwner = pAaCurrentTcb ;
		aaListAddHead (& pAaCurrentTcb->mutexList, & pMutex->mutexNode) ;	// Add mutex at owned list
IFDEBUG { aaLogMes ("MTake 0     %d  %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }
		aaTraceMutexTake (pAaCurrentTcb->taskIndex, mutexIndex) ;
		aaCriticalExit () ;
		return AA_ENONE ;
	}

	if (pMutex->pOwner == pAaCurrentTcb)
	{
		// Mutex is already owned by this TCB
IFDEBUG { aaLogMes ("MTake rec   %d %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }
		if (pMutex->count == MUTEX_MAX_VALUE)
		{
			// Mutex counter already at max value, may indicate a wrong loop in the application
			aaTraceMutexTakeRecFail (pAaCurrentTcb->taskIndex, mutexIndex) ;
			aaCriticalExit () ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_10 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EFAIL ;
		}
		pMutex->count ++ ;
		aaTraceMutexTakeRec (pAaCurrentTcb->taskIndex, mutexIndex) ;
		aaCriticalExit () ;
		return AA_ENONE ;
	}

	// Mutex not available
	if (timeOut == 0)
	{
		// No timeout, return
		aaCriticalExit () ;
		return AA_EWOULDBLOCK ;
	}

	// Wait for mutex
	pAaCurrentTcb->objPtr.pMutex = pMutex ;		// Memorize mutex in TCB, allow priority inheritance management

	// Check for priority inheritance: if the task owner of the mutex has a lower priority than this task
	// then rise the owner task priority to priority of this task.
	if (pAaCurrentTcb->priority > pMutex->pOwner->priority)
	{
IFDEBUG { aaLogMes ("MTake rise  %d %s %s %u -> %u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, (uintptr_t) pMutex->pOwner->name, pMutex->pOwner->priority, pAaCurrentTcb->priority) ; }
		aaMutexPropagate_ (pAaCurrentTcb) ;
	}

	// Wait until mutex is released or timeout
	aaRemoveReady_ (pAaCurrentTcb) ;			// Remove from ready list
	pAaCurrentTcb->state = aaMutexWaitingState ;
	pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;	// Clear awake cause flags

	// Put pAaCurrentTcb in mutex waiting list ordered by priority
	pAaCurrentTcb->waitNode.value = pAaCurrentTcb->priority ;
	aaListAddOrdered_ (& pMutex->waitingList, & pAaCurrentTcb->waitNode) ;

	// Put task in delayed tasks list
	aaAddToDelayedList_ (pAaCurrentTcb, timeOut) ;
	aaTraceMutexWait (pAaCurrentTcb->taskIndex, pMutex->mutexIndex) ;

	aaCriticalExit () ;
	aaSchedule_ () ;
	aaCriticalEnter () ;

	// The current task was awakened by aaMutexGive, or by timeout, or by aaTaskResume
	// The task state is aaReadyState
	// The task was removed from mutex waiting list and delayed tasks list
	AA_KERNEL_ASSERT (pAaCurrentTcb->state == aaReadyState) ;	// Development check

	if ((pAaCurrentTcb->flags & AA_FLAG_DELAYTMO_) != 0)
	{
		// Awakened by timeout, mutex not acquired
IFDEBUG { aaLogMes ("MTake wtmo  %d %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }
		pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;

		aaTraceMutexTakeFail (pAaCurrentTcb->taskIndex, pMutex->mutexIndex) ;
		aaCriticalExit () ;
		return AA_ETIMEOUT ;
	}

// The compiler need tab before this line, else it throw misleading-indentation warning
 IFDEBUG { aaLogMes ("MTake ok    %d %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }

	// Mutex obtained.
	// Another task has given the mutex, without decrementing count, so we don't have to increment.
	// The counter having never been 0, no other task was able to take the mutex.
	// We got the mutex, so we are the highest priority task waiting for this mutex,
	// we may have inherited priority because of owned mutexes, so no priority change.
	// Add mutex to owned mutex list:
	aaListAddHead (& pAaCurrentTcb->mutexList, & pMutex->mutexNode) ;
	aaTraceMutexTake (pAaCurrentTcb->taskIndex, mutexIndex) ;

	aaCriticalExit () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Available mutex: count = 0
//	Taken mutex : count > 0
//	Mutex can be recursively taken by the same task

aaError_t	aaMutexGive (aaMutexId_t mutexId)
{
	uint32_t		mutexIndex = mutexId & AA_HTAG_IMASK ;
	aaMutex_t		* pMutex = & aaMutexArray [mutexIndex] ;
	aaListNode_t	* pListNode ;
	aaTcb_t			* pTcb ;
	uint32_t		bRequestSchedule = 0 ;

	// Check context
	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_MTX_11) ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		// Valid mutex ID ?
		if ((mutexId & AA_HTAG_TMASK) != AA_HTAG_MUTEX  ||   mutexIndex >= AA_MUTEX_MAX)
		{
			// Not a valid mutex ID
			aaTraceMutexGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_MUTEX) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_12 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;			// Not a valid mutex ID
		}

		if (pMutex->pOwner == (aaTcb_t *) MUTEX_FREE_TAG)
		{
			aaTraceMutexGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) mutexIndex) ;
			AA_ERRORNOTIFY (AA_ERROR_MTX_13 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;		// This is not an allocated mutex
		}
	}
	#endif

	if (aaIsRunning == 0)
	{
		return AA_ENONE ;		// Always OK if kernel is not running
	}

	aaCriticalEnter () ;

	// Check for erroneous call
	if (pMutex->count == 0  ||  pMutex->pOwner != pAaCurrentTcb)
	{
		// Mutex already free, or not owned by pAaCurrentTcb
		aaTraceMutexGiveFail (pAaCurrentTcb->taskIndex, mutexIndex) ;
		aaCriticalExit () ;
		AA_ERRORNOTIFY (AA_ERROR_MTX_14 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_ESTATE ;
	}

	// Reentrant mutex ?
	if (pMutex->count > 1)
	{
IFDEBUG { aaLogMes ("MGive rec   %d %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }
		pMutex->count -- ;
		aaTraceMutexGiveRec (pAaCurrentTcb->taskIndex, mutexId) ;
		aaCriticalExit () ;
		return AA_ENONE ;
	}

	// Count = 1 : should transmit mutex to another task, if any is waiting
	aaTraceMutexGive (pAaCurrentTcb->taskIndex, mutexIndex) ;

	// Remove mutex from owned list
	aaListRemove (& pAaCurrentTcb->mutexList, & pMutex->mutexNode) ;

	//	Priority inheritance: To set the priority of pAaCurrentTcb,
	bRequestSchedule = aaMutexNewPrio_ (pAaCurrentTcb) ;

	// Check if a task is waiting for this mutex
	if (aaListIsEmpty (& pMutex->waitingList) == 0)
	{
		// Found one task,
		// Remove the waiting task at head of the waiting list
		// Among the tasks waiting for the mutex, it is the one with the highest priority
		pListNode = aaListRemoveHead (& pMutex->waitingList) ;
		pTcb = (aaTcb_t *) pListNode->ptr.pOwner ;
		pTcb->objPtr.pMutex = NULL ;
IFDEBUG { aaLogMes ("MGive found %d %s p=%u -> %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, (uintptr_t) pTcb->name, pTcb->priority) ; }

		// Remove task from the list of delayed tasks
		aaRemoveFromDelayedList_ (& pTcb->listNode) ;

		// If suspend is requested for this task, add it to suspended list, else add it to ready list
		// pTcb task may be suspended, owning the mutex
		bRequestSchedule = aaTcbPutInList_ (pTcb) ;

		// The mutex count was not decremented, it is ok for the new task
		pMutex->pOwner = pTcb ;
	}
	else
	{
		// No waiting task, mutex is not owned
IFDEBUG { aaLogMes ("MGive none  %d %s p=%u\n", mutexIndex, (uintptr_t) pAaCurrentTcb->name, pAaCurrentTcb->priority, 0, 0) ; }
		pMutex->count -- ;
		pMutex->pOwner = NULL ;
	}

	aaCriticalExit () ;

	if (bRequestSchedule != 0)
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
#endif // AA_MUTEX_MAX > 0


