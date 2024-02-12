/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aasync.c	Task synchronization : semaphore

	When		Who	What
	25/11/13	ac	Creation
	01/06/20	ac	Allow AA_SEM_MAX to be 0
	12/06/21	ac	Remove simple mutex

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

#define		SEM_MAX_VALUE		(32767)			// Max value for sem count
#define		SEM_MIN_VALUE		(-32768)		// Min value for sem count

// For development traces and checks
#define		IFDEBUG		if(0)

#if (AA_SEM_MAX > 0)

//--------------------------------------------------------------------------------
// Initialize synchronization objects free list at kernel start up

void	aaInitSemaphore_ ()
{
	aaSem_t		* pSem   = & aaSemArray   [AA_SEM_MAX-1] ;
	uint32_t	ii ;

	// Add items in free list in reverse order
	// Simpler to debug


	aaStackInitHead (& aaSemFreeList) ;
	for (ii = 0 ; ii < AA_SEM_MAX ; ii++)
	{
		pSem->flags = AA_SEMTYPE_NONE_ ;		// Free
		pSem->semIndex = (aaSemId_t) (AA_SEM_MAX - 1 - ii) ;

		// waitingList is used as a node in aaSemFreeList
		aaStackPush (& aaSemFreeList, (aaStackNode_t *) & pSem->waitingList) ;

		pSem-- ;
	}
}

//--------------------------------------------------------------------------------
// Create a semaphore
// If count > 0 then the created semaphore can be taken

aaError_t	aaSemCreate (int32_t count, aaSemId_t * pSemId)
{
	aaSem_t		* pSem ;
	
	* pSemId = AA_INVALID_SEM ;

	// Search for free synchronization block
	aaCriticalEnter () ;
	if (aaStackIsEmpty (& aaSemFreeList) == 1)
	{
		// Empty sem free list
		aaTraceSemCreateFail (pAaCurrentTcb->taskIndex) ;
		aaCriticalExit () ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_1 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EDEPLETED ;
	}
	pSem = (aaSem_t *) aaStackPop (& aaSemFreeList) ;
	aaTraceSemCreated (pSem->semIndex) ;
	aaCriticalExit () ;

	pSem->count	 = count ;
	pSem->flags	 = (uint8_t) AA_SEMTYPE_SEM_ ;
	pSem->pOwner = NULL ;
	aaListInit (& pSem->waitingList) ;
	* pSemId = pSem->semIndex | AA_HTAG_SEMA ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Check aaSemId_t validity
//	If valid, return a pointer to the sem descriptor
//	else return NULL

static aaSem_t *	aaSemCheckId_ (aaSemId_t semId, uint32_t errorNumber)
{
	aaSem_t	* pSem = & aaSemArray [semId & AA_HTAG_IMASK] ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		if ((semId & AA_HTAG_TMASK) != AA_HTAG_SEMA)
		{
			// semId is not a sem handle. This is a fatal error
			AA_ERRORNOTIFY (errorNumber) ;
			while (1)	// Don't continue, even if AA_ERRORNOTIFY() returns
			{
			}
		}

		if ((semId & AA_HTAG_IMASK) >= AA_SEM_MAX  ||  (pSem->flags & AA_SEMTYPE_SEM_) == 0)
		{
			return NULL ;	// Error handled by the caller
		}
	}
	#else
		(void) errorNumber ;
	#endif

	return pSem ;		// semId is valid
}

//--------------------------------------------------------------------------------
//	Check if a semId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaSemIsId	(aaSemId_t semId)
{
	aaSem_t		* pSem = & aaSemArray [semId & AA_HTAG_IMASK] ;
	aaError_t	resValue = AA_EFAIL ;

	if ((semId & AA_HTAG_TMASK) == AA_HTAG_SEMA)
	{
		// The ID have the sem tag
		if ((semId & AA_HTAG_IMASK) < AA_SEM_MAX  &&  (pSem->flags & AA_SEMTYPE_SEM_) != 0)
		{
			// Index is in the range of sem descriptor array, and this sem is allocated
			resValue = AA_ENONE ;
		}
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
// Free the sem: add this sem to the free sem list.
// Warning: No task should use the semaphore during this operation

aaError_t	aaSemDelete (aaSemId_t semId)
{
	aaSem_t		* pSem ;

	pSem = aaSemCheckId_ (semId, AA_ERROR_SEM_2) ;
	if (pSem == NULL)
	{
		aaTraceSemDeleteFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_2 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (aaListIsEmpty (& pSem->waitingList) == 0)
	{
		// There is tasks waiting for this sem
		(void) aaSemFlush (semId) ;
	}

	// Add to free list
	aaCriticalEnter () ;

	pSem->flags = AA_SEMTYPE_NONE_ ;
	aaStackPush (& aaSemFreeList, (aaStackNode_t *) & pSem->waitingList) ;
	aaTraceSemDeleted (pSem->semIndex) ;

	aaCriticalExit () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	If the timeout is 0 : this is a aaSemTryTake()

aaError_t	aaSemTake (aaSemId_t semId, uint32_t timeOut)
{
	aaSem_t		* pSem ;

	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_SEM_3) ; // Not allowed from ISR

	pSem = aaSemCheckId_ (semId, AA_ERROR_SEM_3) ;
	if (pSem == NULL)
	{
		aaTraceSemTakeFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_SEM) ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (aaIsRunning == 0)
	{
		pSem->count -- ;
		return AA_ENONE ;	// Always OK if kernel is not running
	}

	aaCriticalEnter () ;

	if (pSem->count > 0)
	{
		// The sem is available
		pSem->count -- ;
		AA_ERRORASSERT (pSem->count != SEM_MIN_VALUE, AA_ERROR_SEM_4) ;
 IFDEBUG { aaLogMes ("SMTake Ok     %u  %s\n", pSem->semIndex, (uintptr_t) pAaCurrentTcb->name, 0, 0, 0) ; }
		aaTraceSemTake (pAaCurrentTcb->taskIndex, pSem->semIndex) ;
		aaCriticalExit () ;
		return AA_ENONE ;
	}

	// Sem not available
	if (timeOut == 0)
	{
		// No timeout then return
		aaCriticalExit () ;
		return AA_EWOULDBLOCK ;
	}

	// Wait until semx is available or timeout
	aaRemoveReady_ (pAaCurrentTcb) ;			// Remove from ready list
	pAaCurrentTcb->state = aaSemWaitingState ;
	pAaCurrentTcb->objPtr.pSem = pSem ;			// Memorize current synchro object in TCB
	pAaCurrentTcb->flags &= (uint16_t) ~(AA_FLAG_FLUSH_ | AA_FLAG_DELAYTMO_) ;	// Clear awake cause flags

	// Put pAaCurrentTcb in sem object waiting list ordered by priority
	pAaCurrentTcb->waitNode.value = pAaCurrentTcb->priority ;
	aaListAddOrdered_ (& pSem->waitingList, & pAaCurrentTcb->waitNode) ;

	// Put task in delayed tasks list
	aaAddToDelayedList_ (pAaCurrentTcb, timeOut) ;

 IFDEBUG { aaLogMes ("SMTake Wait   %u  %s\n", pSem->semIndex, (uintptr_t) pAaCurrentTcb->name, 0, 0, 0) ; }
	aaTraceSemWait (pAaCurrentTcb->taskIndex, pSem->semIndex) ;

	aaCriticalExit () ;
	aaSchedule_ () ;
	aaCriticalEnter () ;

	// The current task was awakened by aaSemGive, or by timeout, or by flush, or by aaTaskResume
	// The task state is aaReadyState
	// The task was removed from sem waiting list and delayed tasks list
	AA_KERNEL_ASSERT (pAaCurrentTcb->state == aaReadyState) ;	// Development check

	if ((pAaCurrentTcb->flags & AA_FLAG_FLUSH_) != 0)
	{
		// Awakened by sem flush, sem not acquired
		pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_FLUSH_ ;
		aaTraceSemTakeFail (pAaCurrentTcb->taskIndex, pSem->semIndex) ;
		aaCriticalExit () ;
		return AA_EFLUSH ;
	}
	
	if ((pAaCurrentTcb->flags & AA_FLAG_DELAYTMO_) != 0)
	{
		// Awakened by timeout, sem not acquired
		pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;
		aaTraceSemTakeFail (pAaCurrentTcb->taskIndex, pSem->semIndex) ;
		aaCriticalExit () ;
		return AA_ETIMEOUT ;
	}

	//	Sem obtained.
	//	Another task has given the sem, without incrementing count, so we don't have to decrement.
	//	The counter having never been > 0, no other task was able to take the sem.
 IFDEBUG { aaLogMes ("SMTake Ok     %u  %s\n", pSem->semIndex, (uintptr_t) pAaCurrentTcb->name, 0, 0, 0) ; }
	aaTraceSemTake (pAaCurrentTcb->taskIndex, pSem->semIndex) ;
	aaCriticalExit () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Available semaphore: count > 0

aaError_t	aaSemGive (aaSemId_t semId)
{
	aaSem_t			* pSem ;
	aaListNode_t	* pListNode ;
	aaTcb_t			* pTcb ;
	uint8_t			bRequestSchedule = 0 ;

	// Check handle and context
	pSem = aaSemCheckId_ (semId, AA_ERROR_SEM_5) ;
	if (pSem == NULL)
	{
		aaTraceSemGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_SEM) ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_5 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (aaIsRunning == 0)
	{
		pSem->count ++ ;
		return AA_ENONE ;	// Always OK if the kernel is not running
	}

	aaCriticalEnter () ;
	aaTraceSemGive (pAaCurrentTcb->taskIndex, pSem->semIndex) ;

	// if count < 0 then increment count
	// else try to give the sem to a waiting task
	if (pSem->count < 0)
	{
		// No waiting task
		pSem->count ++ ;
		aaCriticalExit () ;
		return AA_ENONE ;
	}

	// Give semaphore to a waiting task (if any)
	// Check if a task is waiting for this sem
	if (aaListIsEmpty (& pSem->waitingList) == 0)
	{
		// Found one waiting task,
		// Remove the waiting task from the waiting list (it is at head of the waiting list)
		pListNode = aaListRemoveHead (& pSem->waitingList) ;
		pTcb = (aaTcb_t *) pListNode->ptr.pOwner ;
		pTcb->objPtr.pSem = NULL ;
IFDEBUG { aaLogMes ("SMGive Ok     %u  %s\n", pSem->semIndex, (uintptr_t) pTcb->name, 0, 0, 0) ; }

		// The semaphore count is not incremented, it is OK for the new owner task
		pSem->pOwner = pTcb ;

		// Remove the task from the delayed tasks list
		aaRemoveFromDelayedList_ (& pTcb->listNode) ;

		// If suspend is requested for this task, add it to suspended list, else add it to ready list
		// pTcb task may be suspended, owning the semaphore
		bRequestSchedule = aaTcbPutInList_ (pTcb) ;
	}
	else
	{
		// No waiting task,
IFDEBUG { aaLogMes ("SMGive none   %u\n", pSem->semIndex, 0, 0, 0, 0) ; }

		// Semaphore counter at max value may indicate a wrong loop in the application
		pSem->count ++ ;
		AA_ERRORASSERT (pSem->count != SEM_MAX_VALUE, AA_ERROR_SEM_8) ;
	}

	aaCriticalExit () ;

	if (bRequestSchedule  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Atomically unblock all waiting tasks, all tasks will be unblocked before any is allowed to run
//	Sem state is unchanged
//	Useful as a mean of broadcast in synchronization applications

aaError_t	aaSemFlush	(aaSemId_t semId)
{
	aaSem_t			* pSem ;
	aaListNode_t	* pNode ;
	aaTcb_t			* pTcb ;
	uint8_t			bRequestSchedule = 0 ;

	// Check handle and context
	pSem = aaSemCheckId_ (semId, AA_ERROR_SEM_9) ;
	if (pSem == NULL)
	{
		aaTraceSemFlushFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_SEM) ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_9 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;

	while (aaListIsEmpty (& pSem->waitingList) == 0)
	{
		// Remove the TCB from the sem waiting list
		pNode = aaListRemoveHead (& pSem->waitingList) ;
		pTcb = (aaTcb_t *) pNode->ptr.pOwner ;
		pTcb->objPtr.pSem = NULL ;
		pTcb->flags |= AA_FLAG_FLUSH_ ;	// The cause of awakening

		// Remove the task from the delayed tasks list
		aaRemoveFromDelayedList_ (& pTcb->listNode) ;

		// If suspend is requested for this task, add it to suspended list, else add it to ready list
		bRequestSchedule = aaTcbPutInList_ (pTcb) ;
	}

	aaCriticalExit () ;

	if (bRequestSchedule  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Should be used only if there is no task waiting to take the sem

aaError_t	aaSemReset	(aaSemId_t semId, int32_t count)
{
	aaSem_t		* pSem ;

	// Check context: valid handle of semaphore
	pSem = aaSemCheckId_ (semId, AA_ERROR_SEM_11) ;
	if (pSem == NULL)
	{
		aaTraceSemResetFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_SEM) ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_11 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;

	if (aaListIsEmpty (& pSem->waitingList) == 0)
	{
		aaCriticalExit () ;
		AA_ERRORNOTIFY (AA_ERROR_SEM_13 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_ESTATE ;
	}

	pSem->count = (uint16_t) count ;

	aaCriticalExit () ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
#endif // AA_SEM_MAX > 0

