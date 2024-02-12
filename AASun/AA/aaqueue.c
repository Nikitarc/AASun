/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aaqueue.c	Message passing IPC

	When		Who	What
	02/07/17	ac	Creation
	28/09/19	ac	Add AA_QUEUE_POINTER
	01/06/20	ac	aaQueueCreate(): add AA_WITH_MALLOC condition

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
#include	"aalist.h"
#include	"aaqueue.h"

/*	This queue uses the copy of messages.
	To avoid copying messages, it is necessary to use a buffer pool
	and use the queue to transmit the addresses of the buffers.
	And so use a queue whose message is a pointer and the size of the message is sizeof (void *).
	The queue can allocate the array of pointers, the array of messages must be allocated by the application.
*/
//--------------------------------------------------------------------------------

#if (AA_QUEUE_MAX > 0)

// The queue array
STATIC	aaQueue_t		aaQueues [AA_QUEUE_MAX] BSP_ATTR_NOINIT ;

// The list of free queues
STATIC	aaStackNode_t		aaQueueList ;

// Forward declarations
static	uint32_t	aaQueueRemove_		(aaListHead_t * pWaitingList, uint16_t flag) ;
static	aaError_t	aaQueueTaskDelay_	(uint32_t queueIndex, uint32_t bPutList, uint32_t timeout) ;

#define	QUEUE_GET_LIST				0u
#define	QUEUE_PUT_LIST				1u

#define	MAX_QUEUE_MSG_COUNT_MAX_	0xFFFFu		// Related to msgCount type
#define	MAX_QUEUE_MSG_SIZE_MAX_		0xFFFFu		// Related to msgSize type


//--------------------------------------------------------------------------------
//	Initialize the queue package. Call only once at kernel start.

void		aaQueueInit_	(void)
{
	uint32_t	ii ;

	// Do it only once
	if (aaQueueList.pNext == NULL)
	{
		aaStackInitHead (& aaQueueList) ;
		for (ii = 0 ; ii < AA_QUEUE_MAX ; ii++)
		{
			aaQueues [ii].flags = AA_QUEUE_DELETED_ ;
			aaQueues [ii].msgCount = (uint16_t) ii ;			// Id of this queue
			aaStackPush (& aaQueueList, (aaStackNode_t *) & aaQueues [ii].putWaitingList) ;
		}
	}
}

//--------------------------------------------------------------------------------
//	Check queueId validity
//	If valid, return a pointer to the queue descriptor
//	else return NULL

static aaQueue_t *	aaQueueCheckId_ (aaQueueId_t queueId, uint32_t errorNumber)
{
	aaQueue_t	* pQ = & aaQueues [queueId & AA_HTAG_IMASK] ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		if ((queueId & AA_HTAG_TMASK) != AA_HTAG_QUEUE)
		{
			// queueId is not a queue handle. This is a fatal error
			AA_ERRORNOTIFY (errorNumber) ;
			while (1)	// Don't continue, even if AA_ERRORNOTIFY() returns
			{
			}
		}

		if (((queueId & AA_HTAG_IMASK) >= AA_QUEUE_MAX)  ||  (pQ->flags == AA_QUEUE_DELETED_))
		{
			return NULL ;	// Error handled by the caller
		}
	}
	#else
		(void) errorNumber ;
	#endif

	return pQ ;		// queueId is valid
}

//--------------------------------------------------------------------------------
//	Check if a queueId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaQueueIsId		(aaQueueId_t queueId)
{
	aaQueue_t	* pQ = & aaQueues [queueId & AA_HTAG_IMASK] ;
	aaError_t	resValue = AA_EFAIL ;

	if ((queueId & AA_HTAG_TMASK) == AA_HTAG_QUEUE)
	{
		// The ID have the queue tag
		if (((queueId & AA_HTAG_IMASK) < AA_QUEUE_MAX)  &&  (pQ->flags != AA_QUEUE_DELETED_))
		{
			// Index is in the range of queue descriptor array, and allocated
			resValue = AA_ENONE ;
		}
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
// Get a queue from the list of free queues

aaError_t	aaQueueCreate	(aaQueueId_t * pQueueId, uint32_t msgSize, uint32_t msgCount, uint8_t * pBuffer, uint32_t flags)
{
	aaQueue_t	* pQ ;

	* pQueueId = AA_INVALID_QUEUE ;

	if (msgCount > MAX_QUEUE_MSG_COUNT_MAX_  ||  msgSize > MAX_QUEUE_MSG_SIZE_MAX_)
	{
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_1 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	#if (AA_WITH_MALLOC != 1)
	{
		if (pBuffer == NULL)
		{
			return AA_EARG ;			// Dynamic memory management not allowed
		}
	}
	#endif

	// Find a queue descriptor block in the free list
	aaCriticalEnter () ;
	if (aaStackIsEmpty (& aaQueueList) == 1)
	{
		aaTraceQueueCreateFail (pAaCurrentTcb->taskIndex) ;
		aaCriticalExit ();
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_2 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EDEPLETED ;
	}

	pQ = (aaQueue_t *) aaStackPop (& aaQueueList) ;
	aaCriticalExit () ;

	* pQueueId		= pQ->msgCount | AA_HTAG_QUEUE ;
	pQ->msgCount	= (uint16_t) msgCount ;
	pQ->msgSize		= (uint16_t) msgSize ;
	pQ->msgUsed		= 0 ;
	aaListInit (& pQ->putWaitingList) ;	// The node become a list head
	aaListInit (& pQ->getWaitingList) ;
	pQ->flags = 0 ;
	if ((flags & AA_QUEUE_PRIORITY) != 0)
	{
		pQ->flags |= AA_QUEUE_PRIORITY_ ;
	}
	if ((flags & AA_QUEUE_POINTER) != 0)
	{
		pQ->flags |= AA_QUEUE_POINTER_ ;
		pQ->msgSize	= (uint16_t) sizeof (void *) ;
	}

	// Allocate message buffer if needed
	pQ->pBuffer = pBuffer ;
	if (pBuffer == NULL)
	{
		// Dynamically allocate the message buffer
		#if (AA_WITH_MALLOC == 1)
		{
			pQ->pBuffer = (uint8_t *) aaMalloc (msgCount * pQ->msgSize) ;
			if (pQ->pBuffer == NULL)
			{
				// Memory error, return the queue to the free list
				aaCriticalEnter () ;
				aaStackPush (& aaQueueList, (aaStackNode_t *) & pQ->putWaitingList) ;
				aaCriticalExit () ;
				return AA_EMEMORY ;
			}
			pQ->flags |= AA_QUEUE_DYNAMICBUFFER_ ;		// Queue buffer allocated by the kernel
		}
		#else
		{
			// Dynamic memory management not allowed
			AA_ERRORNOTIFY (AA_ERROR_QUEUE_12) ;
		}
		#endif		// AA_WITH_MALLOC
	}
	pQ->pLast 	= & pQ->pBuffer [pQ->msgSize * msgCount] ;
	pQ->pWrite	= pQ->pBuffer ;
	pQ->pRead	= pQ->pBuffer ;

	aaTraceQueueCreated (pAaCurrentTcb->taskIndex, (* pQueueId) & AA_HTAG_IMASK) ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
// Revert a queue to the list of free queues

aaError_t	aaQueueDelete	(aaQueueId_t queueId)
{
	aaQueue_t	* pQ ;
	uint32_t	bRequestSchedule ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_3) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueueDeleteFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;

	// Flush waiting tasks
	bRequestSchedule = 0 ;
	while (aaListIsEmpty (& pQ->putWaitingList) == 0)
	{
		bRequestSchedule |= aaQueueRemove_ (& pQ->putWaitingList, AA_FLAG_FLUSH_) ;
	}
	while (aaListIsEmpty (& pQ->getWaitingList) == 0)
	{
		bRequestSchedule |= aaQueueRemove_ (& pQ->getWaitingList, AA_FLAG_FLUSH_) ;
	}

	#if AA_WITH_MALLOC == 1
	{
		if ((pQ->flags & AA_QUEUE_DYNAMICBUFFER_) != 0)
		{
			aaFree (pQ->pBuffer) ;
		}
	}
	#endif		// AA_WITH_MALLOC

	pQ->pBuffer = NULL ;
	pQ->flags = AA_QUEUE_DELETED_ ;

	// Put queue in free list
	pQ->msgCount = queueId & AA_HTAG_IMASK ;
	aaStackPush (& aaQueueList, (aaStackNode_t *) & pQ->putWaitingList) ;

	aaTraceQueueDeleted (pQ->msgCount) ;

	aaCriticalExit () ;

	if (bRequestSchedule != 0)
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Remove the first task in the queue waiting task list, and set it ready
//	Must be called from critical section
//	The flag parameter is the cause of awakening: 0 or AA_FLAG_FLUSH_
//	Returns 1 if scheduling is requested, else 0

static	uint32_t	aaQueueRemove_ (aaListHead_t * pWaitingList, uint16_t flag)
{
	aaTcb_t			* pTcb ;
	aaListNode_t	* pNode ;
	uint32_t		bRequestSchedule = 0 ;

	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;	// Development check

	// Remove TCB from waiting list
	// For FIFO or priority ordering list remove the item at head of list
	pNode = aaListRemoveHead (pWaitingList) ;
	pTcb = (aaTcb_t *) pNode->ptr.pOwner ;
	pTcb->flags |= flag ;	// The cause of awakening

	// Remove the task from delayed tasks list
	aaRemoveFromDelayedList_ (& pTcb->listNode) ;

	// If suspend is requested add the task to suspended list, else add it to ready list
	bRequestSchedule = aaTcbPutInList_ (pTcb) ;

	return bRequestSchedule ;
}

//--------------------------------------------------------------------------------
//	Set current task in queue waiting task list
//	Must be called from critical section
//	bPutList is not 0 if the putWaitingList should be used, else use getWaitingList
//	Returns E_NONE if the task was woken up normally (not by timeout, not by flush, ...)

static	aaError_t	aaQueueTaskDelay_ (uint32_t queueIndex, uint32_t bPutList, uint32_t timeout)
{
	aaQueue_t * pQ = & aaQueues [queueIndex] ;

	AA_KERNEL_ASSERT (aaInCriticalCounter != 0) ;	// Development check

	if (timeout == 0)
	{
		// No timeout, return immediately
		return AA_EWOULDBLOCK ;
	}

	// Suspend is not allowed for ISR
	if (aaIsInIsr () != 0)
	{
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_4 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EWOULDBLOCK ;
	}

	// Suspend pAaCurrentTcb task
	aaRemoveReady_ (pAaCurrentTcb) ;
	pAaCurrentTcb->state = aaQueueWaitingState ;
	pAaCurrentTcb->flags &= (uint16_t) ~(AA_FLAG_QUEUEPUT_ | AA_FLAG_FLUSH_ | AA_FLAG_DELAYTMO_) ;

	// Put pAaCurrentTcb in the right queue waiting list
	pAaCurrentTcb->objPtr.pQueue = pQ ;
	pAaCurrentTcb->waitNode.value = pAaCurrentTcb->priority ;
	if (bPutList == QUEUE_PUT_LIST)
	{
		// Put list
		pAaCurrentTcb->flags |= AA_FLAG_QUEUEPUT_ ;
		if ((pQ->flags & AA_QUEUE_PRIORITY_) != 0)
		{
			// Priority ordered list
			aaListAddOrdered_ (& pQ->putWaitingList, & pAaCurrentTcb->waitNode) ;
		}
		else
		{
			// FIFO list
			aaListAddTail (& pQ->putWaitingList, & pAaCurrentTcb->waitNode) ;
		}
	}
	else
	{
		// Get list
		if ((pQ->flags & AA_QUEUE_PRIORITY_) != 0)
		{
			// Priority ordered list
			aaListAddOrdered_ (& pQ->getWaitingList, & pAaCurrentTcb->waitNode) ;
		}
		else
		{
			// FIFO list
			aaListAddTail (& pQ->getWaitingList, & pAaCurrentTcb->waitNode) ;
		}
	}

	// Put the task in the delayed tasks list
	aaAddToDelayedList_ (pAaCurrentTcb, timeout) ;
	aaTraceQueueWait (pAaCurrentTcb->taskIndex, queueIndex) ;
	aaCriticalExit () ;

	aaSchedule_ () ;

	aaCriticalEnter () ;
	if ((pAaCurrentTcb->flags & AA_FLAG_FLUSH_) != 0)
	{
		// Awakened by flush queue or delete queue
		pAaCurrentTcb->flags &= (uint16_t) ~(AA_FLAG_FLUSH_) ;
		return AA_EFLUSH ;
	}
	if ((pAaCurrentTcb->flags & AA_FLAG_DELAYTMO_) != 0)
	{
		// Awakened by delay timeout
		pAaCurrentTcb->flags &= (uint16_t) ~(AA_FLAG_DELAYTMO_) ;
		return AA_ETIMEOUT ;
	}

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Put a message in the queue
//	If AA_QUEUE_POINTER is not used then pData is a pointer to the data to put in the queue
//	If AA_QUEUE_POINTER is used then pData itself is the data to put in the queue
//	A size of 0 implies using the native size of the queue message

aaError_t	aaQueueGive		(aaQueueId_t queueId, void * pData, uint32_t size, uint32_t timeout)
{
	aaQueue_t	* pQ ;
	uint32_t	bRequestSchedule = 0 ;
	uint32_t	msgSize ;
	aaError_t	result ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_5) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueueGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_QUEUE) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_5 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if ((pQ->flags & AA_QUEUE_POINTER_) != 0)
	{
		msgSize = sizeof (void *) ;
	}
	else
	{
		msgSize = size ;
		if (msgSize == 0)
		{
			msgSize = pQ->msgSize ;
		}

		if (msgSize > pQ->msgSize)
		{
			aaTraceQueueGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) queueId & AA_HTAG_IMASK) ;
			AA_ERRORNOTIFY (AA_ERROR_QUEUE_6 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;
		}
	}

	while (1)
	{
		aaCriticalEnter () ;
		if (pQ->msgUsed != pQ->msgCount)
		{
			break ;
		}

		// Buffer is full
		result = aaQueueTaskDelay_ (queueId & AA_HTAG_IMASK, QUEUE_PUT_LIST, timeout) ;
		if (result == AA_EWOULDBLOCK  ||  result != AA_ENONE)
		{
			aaTraceQueueGiveFail (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;
			aaCriticalExit () ;
			return result ;
		}
		// May send message, check if buffer is not full
		aaCriticalExit () ;
	}

	// Buffer is not full, copy message
	if ((pQ->flags & AA_QUEUE_POINTER_) != 0)
	{
		* (void **) ((void *) pQ->pWrite) = pData ;
	}
	else
	{
		aaMEMCPY (pQ->pWrite, pData, msgSize) ;
	}

	// Adjust writing pointer
	pQ->pWrite = & pQ->pWrite [pQ->msgSize] ;
	if (pQ->pWrite == pQ->pLast)
	{
		pQ->pWrite = pQ->pBuffer ;
	}
	pQ->msgUsed++ ;
	aaTraceQueueGive (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;

	// One new message in the queue, check if a getting task is waiting
	if (aaListIsEmpty (& pQ->getWaitingList) == 0)
	{
		// Awake one getting task
		bRequestSchedule = aaQueueRemove_ (& pQ->getWaitingList, 0) ;
	}

	aaCriticalExit () ;
	if ((bRequestSchedule != 0)  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Get a message from the queue
//	pData is always a pointer to the place to put the extracted data
//	A size of 0 implies using the native size of the queue message

aaError_t	aaQueueTake		(aaQueueId_t queueId, void * pData, uint32_t size, uint32_t timeout)
{
	aaQueue_t	* pQ ;
	uint32_t	bRequestSchedule = 0 ;
	uint32_t	takeSize ;
	aaError_t	result ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_7) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueueTakeFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_QUEUE) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_7 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if ((pQ->flags & AA_QUEUE_POINTER_) != 0)
	{
		takeSize = sizeof (void *) ;
	}
	else
	{
		takeSize = size ;
		if (size == 0  ||  size > pQ->msgSize)
		{
			takeSize = pQ->msgSize ;
		}
	}

	while (1)
	{
		aaCriticalEnter () ;
		if (pQ->msgUsed != 0)
		{
			break ;
		}

		// Buffer is empty
		result = aaQueueTaskDelay_ (queueId & AA_HTAG_IMASK, QUEUE_GET_LIST, timeout) ;
		if (result == AA_EWOULDBLOCK  ||  result != AA_ENONE)
		{
			aaTraceQueueTakeFail (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;
			aaCriticalExit () ;
			return result ;
		}
		// May get message, check if buffer is not empty
		aaCriticalExit () ;
	}

	// Read message
	if ((pQ->flags & AA_QUEUE_POINTER_) != 0)
	{
		* (void **) pData = * (void **) ((void *) pQ->pRead) ;
	}
	else
	{
		aaMEMCPY (pData, pQ->pRead, takeSize) ;
	}
	aaTraceQueueTake (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;

	// Adjust reading pointer
	pQ->pRead = & pQ->pRead [pQ->msgSize] ;
	if (pQ->pRead == pQ->pLast)
	{
		pQ->pRead = pQ->pBuffer ;
	}
	pQ->msgUsed-- ;

	// One less message in the queue, check if one putting task is waiting
	if (aaListIsEmpty (& pQ->putWaitingList) == 0)
	{
		// Awake one putting task
		bRequestSchedule = aaQueueRemove_ (& pQ->putWaitingList, 0) ;
	}

	aaCriticalExit () ;
	if ((bRequestSchedule != 0)  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Get a message address from the queue. The message is not removed from the queue.
//	Warning: use aaQueuePeek() with caution when there is more than one get task

aaError_t	aaQueuePeek		(aaQueueId_t queueId, void ** ppData, uint32_t timeout)
{
	aaQueue_t	* pQ ;
	aaError_t	result ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_8) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueuePeekFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_QUEUE) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_8 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	while (1)
	{
		aaCriticalEnter () ;
		if (pQ->msgUsed != 0)
		{
			break ;
		}

		// Buffer is empty
		result = aaQueueTaskDelay_ (queueId & AA_HTAG_IMASK, QUEUE_GET_LIST, timeout) ;
		if (result == AA_EWOULDBLOCK  ||  result != AA_ENONE)
		{
			aaTraceQueuePeekFail (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;
			aaCriticalExit () ;
			return result ;
		}
		// May get message, check if buffer is not empty
		aaCriticalExit () ;
	}

	* ppData = pQ->pRead ;

	aaTraceQueuePeek (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}


//--------------------------------------------------------------------------------
//	Remove one message from the queue
//	Warning: use aaQueuePurge() with caution when there is more than one getting task

aaError_t	aaQueuePurge		(aaQueueId_t queueId)
{
	aaQueue_t	* pQ ;
	uint32_t	bRequestSchedule = 0 ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_9) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueuePurgeFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_QUEUE) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_9 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;
	if (pQ->msgUsed != 0)
	{
		// Adjust reading pointer
		pQ->pRead = & pQ->pRead [pQ->msgSize] ;
		if (pQ->pRead == pQ->pLast)
		{
			pQ->pRead = pQ->pBuffer ;
		}
		pQ->msgUsed-- ;

		// One less message in the queue, check if one putting task is waiting
		if (aaListIsEmpty (& pQ->putWaitingList) == 0)
		{
			// Awake one putting task
			bRequestSchedule = aaQueueRemove_ (& pQ->putWaitingList, 0) ;
		}
	}

	aaTraceQueuePurge (pAaCurrentTcb->taskIndex, queueId & AA_HTAG_IMASK) ;
	aaCriticalExit () ;
	if ((bRequestSchedule != 0)  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}

	return AA_ENONE ;
}


//--------------------------------------------------------------------------------
// Get count of message present in the queue buffer

aaError_t	aaQueueGetCount	(aaQueueId_t queueId, uint32_t * pCount)
{
	aaQueue_t	* pQ ;

	// Check handle and context
	pQ = aaQueueCheckId_ (queueId, AA_ERROR_QUEUE_10) ;
	if (pQ == NULL)
	{
		// Not a valid queue id
		aaTraceQueueGetCountFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_QUEUE) ;
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_10 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (pCount == NULL)
	{
		AA_ERRORNOTIFY (AA_ERROR_QUEUE_11 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	* pCount = pQ->msgUsed ;
	return AA_ENONE ;
}
#endif	// AA_QUEUE_MAX > 0

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Buffer pool management
//	Manage an array of buffers which have all the same size

#if (AA_BUFPOOL_MAX > 0)

#define	AA_BUFPOOL_DYNAMICBUFFER_	1u		// The buffer is allocated by the kernel
#define	AA_BUFPOOL_FREE_			0x8000u	// The buffer is not allocated (it is in free list)

#define	AA_POOL_COUNT_MAX_			0xFFFFu	// Related to bufferCount type
#define	AA_POOL_SIZE_MAX_			0xFFFFu	// Related to bufferSize type

typedef struct aaBufferPool_s
{
	aaStackNode_t	freeList ;				// Free buffers list. Mandatory first item in structure
	uint8_t			* pBufferPool ;			// The pool of buffers
	uint16_t		bufferSize ;			// The size of a buffer in pBufferPool
	uint16_t		bufferCount ;			// Count of buffers in pBufferPool
	uint16_t		bufferFree ;			// Count of free buffers in freeList
	uint16_t		flags ;					// When the pool is free, flags contain the pool index in aaBufferPools

} aaBufferPool_t ;

// The pool array
STATIC	aaBufferPool_t	aaBufferPools [AA_BUFPOOL_MAX] BSP_ATTR_NOINIT ;

// The list of free pools
STATIC	aaStackNode_t		aaBufferPoolFreeList ;

//--------------------------------------------------------------------------------
//	Initialize the buffer pool package. Call only once at kernel start.

void		aaBufferPoolInit_	(void)
{
	uint32_t	ii ;

	// Do it only once
	if (aaBufferPoolFreeList.pNext == NULL)
	{
		aaStackInitHead (& aaBufferPoolFreeList) ;
		for (ii = 0 ; ii < AA_BUFPOOL_MAX ; ii++)
		{
			aaBufferPools [ii].flags = (uint16_t) (ii | AA_BUFPOOL_FREE_) ;
			aaStackPush (& aaBufferPoolFreeList, & aaBufferPools [ii].freeList) ;
		}
	}
}

//--------------------------------------------------------------------------------
//	Check if pPoolId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaBufferPoolIsId		(aaBuffPoolId_t bufPoolId)
{
	aaBufferPool_t	* pPool = & aaBufferPools [bufPoolId & AA_HTAG_IMASK] ;
	aaError_t		resValue = AA_EFAIL ;

	if ((bufPoolId & AA_HTAG_TMASK) == AA_HTAG_BPOOL)
	{
		// The ID have the buffer pool tag
		if (((bufPoolId & AA_HTAG_IMASK) < AA_BUFPOOL_MAX)  &&  ((pPool->flags & AA_BUFPOOL_FREE_) == 0u))
		{
			// Index is in the range of pool descriptor array, and  allocated
			resValue =  AA_ENONE ;
		}
	}

	return resValue ;
}

//--------------------------------------------------------------------------------
//	Check validity of bufPoolId
//	If bufPoolId is valid returns a pointer to the pool, else NULL

static	aaBufferPool_t *	aaBufferPoolCheck_ (aaBuffPoolId_t bufPoolId, uint32_t errorNumber)
{
	uint32_t		bufPoolIndex = bufPoolId & AA_HTAG_IMASK ;
	aaBufferPool_t	* pPool = & aaBufferPools [bufPoolIndex] ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		if (((bufPoolId & AA_HTAG_TMASK) != AA_HTAG_BPOOL)  ||  (bufPoolIndex >= AA_BUFPOOL_MAX))
		{
			AA_ERRORNOTIFY (errorNumber | AA_ERROR_FORCERETURN_FLAG) ;
			return NULL ;
		}

		if ((pPool->flags & AA_BUFPOOL_FREE_) == AA_BUFPOOL_FREE_)
		{
			// This buffer is not allocated
			return NULL ;
		}
	}
	#else
		(void) errorNumber ;
	#endif

	return pPool ;
}

//--------------------------------------------------------------------------------
//	Create a pool, if there is a free pool
//	Min value of bufferSize if sizeof (aaStackNode_t), and pBuffer must be word aligned
//	The pool buffer pBuffer should have a size of bufferCount*bufferSize
//	If pBuffer is NULL, the pool allocate the pool buffer on create, and releases the pool buffer on delete


aaError_t	aaBufferPoolCreate		(aaBuffPoolId_t * pBufPoolId, uint32_t bufferCount, uint32_t bufferSize, void * pBuffer)
{
	aaBufferPool_t	* pPool ;

	* pBufPoolId = AA_INVALID_BUFFERPOOL ;

	if (((uintptr_t) pBuffer & 0x03u) != 0  ||  bufferCount > AA_POOL_COUNT_MAX_  ||
		bufferSize > AA_POOL_SIZE_MAX_  ||  bufferSize < sizeof (aaStackNode_t))
	{
		aaTraceBuffPoolCreateFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_1 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	// Find a pool descriptor block in the free list
	aaCriticalEnter () ;
	if (aaStackIsEmpty (& aaBufferPoolFreeList) == 1u)
	{
		aaTraceBuffPoolCreateFail (pAaCurrentTcb->taskIndex) ;
		aaCriticalExit ();
		return AA_EDEPLETED ;
	}

	pPool = (aaBufferPool_t *) aaStackPop (& aaBufferPoolFreeList) ;
	aaCriticalExit () ;

	* pBufPoolId = (aaBuffPoolId_t) ((pPool->flags & ~AA_BUFPOOL_FREE_) | AA_HTAG_BPOOL) ;
	pPool->flags		= 0u ;
	pPool->bufferSize	= (uint16_t) bufferSize ;
	pPool->bufferCount	= (uint16_t) bufferCount ;

	// Allocate pool buffer if needed
	if (pBuffer == NULL)
	{
		// Should allocate pool buffer
		#if (AA_WITH_MALLOC == 1)
		{
			pPool->pBufferPool = (uint8_t *) aaMalloc (bufferCount * bufferSize) ;
			if (pPool->pBufferPool == NULL)
			{
				// Memory error, return the pool to the free list
				aaCriticalEnter () ;
				aaStackPush (& aaBufferPoolFreeList, (aaStackNode_t *) & pPool->freeList) ;
				aaCriticalExit () ;
				return AA_EMEMORY ;
			}
			pPool->flags |= AA_BUFPOOL_DYNAMICBUFFER_ ;		// Pool of buffer allocated by the kernel
		}
		#else
		{
			// Dynamic memory management not allowed
			AA_ERRORNOTIFY (AA_ERROR_BPOOL_2 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;
		}
		#endif		// AA_WITH_MALLOC
	}
	else
	{
		pPool->pBufferPool = (uint8_t *) pBuffer ;
	}

	// Build buffers free list
	aaBufferPoolReset (* pBufPoolId) ;

	aaTraceBuffPoolCreated (pAaCurrentTcb->taskIndex, (* pBufPoolId) & AA_HTAG_IMASK) ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Delete a buffer pool
//	If force is 1 : The pool is unconditionally deleted, even if there are buffers in use
//	If force is 0 : The pool is deleted only if all buffers are free

aaError_t	aaBufferPoolDelete	(aaBuffPoolId_t bufPoolId, uint32_t bForce)
{
	aaBufferPool_t	* pPool ;
	uint32_t		bufPoolIndex = bufPoolId & AA_HTAG_IMASK ;

	pPool = aaBufferPoolCheck_ (bufPoolId, AA_ERROR_BPOOL_3) ;
	if (pPool == NULL)
	{
		aaTraceBuffPoolDeleteFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if ((pPool->bufferCount != pPool->bufferFree)  &&  (bForce == 0u))
	{
		// Not all buffers free, and bForce is 0
		return AA_ESTATE ;
	}

	#if (AA_WITH_MALLOC == 1)
	{
		if ((pPool->flags & AA_BUFPOOL_DYNAMICBUFFER_) != 0u)
		{
			aaFree (pPool->pBufferPool) ;
		}
	}
	#endif		// AA_WITH_MALLOC

	pPool->pBufferPool = NULL ;
	pPool->flags = (uint16_t) (bufPoolIndex | AA_BUFPOOL_FREE_) ;

	aaCriticalEnter () ;
	aaStackPush (& aaBufferPoolFreeList, & pPool->freeList) ;
	aaTraceBuffPoolDeleted (pAaCurrentTcb->taskIndex, bufPoolIndex) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Obtain a buffer if one is available in the pool

aaError_t	aaBufferPoolTake	(aaBuffPoolId_t bufPoolId, void ** ppBuffer)
{
	aaBufferPool_t	* pPool ;

	pPool = aaBufferPoolCheck_ (bufPoolId, AA_ERROR_BPOOL_4) ;
	if (pPool == NULL)
	{
		aaTraceBuffPoolTakeFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_BUFFERPOOL) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_4 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	#if (AA_WITH_ARGCHECK == 1)
	{
		if (ppBuffer == NULL)
		{
			aaTraceBuffPoolTakeFail (pAaCurrentTcb->taskIndex, bufPoolId & AA_HTAG_IMASK) ;
			AA_ERRORNOTIFY (AA_ERROR_BPOOL_5 | AA_ERROR_FORCERETURN_FLAG) ;
			return AA_EARG ;
		}
	}
	#endif

	* ppBuffer = NULL ;

	aaCriticalEnter () ;
	if (aaStackIsEmpty (& pPool->freeList) == 1u)
	{
		aaCriticalExit () ;
		return AA_EDEPLETED ;
	}

	* ppBuffer = aaStackPop (& pPool->freeList) ;
	pPool->bufferFree-- ;
	aaTraceBuffPoolTake (pAaCurrentTcb->taskIndex, bufPoolId & AA_HTAG_IMASK) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Return a buffer to the pool

aaError_t	aaBufferPoolGive	(aaBuffPoolId_t bufPoolId, void * pBuffer)
{
	aaBufferPool_t	* pPool ;

	pPool = aaBufferPoolCheck_ (bufPoolId, AA_ERROR_BPOOL_6) ;
	if (pPool == NULL)
	{
		aaTraceBuffPoolGiveFail (pAaCurrentTcb->taskIndex, (uint8_t) AA_INVALID_BUFFERPOOL) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_6 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	// Check if pBuffer is valid: in the buffer address interval
	if ((uint8_t *)pBuffer < pPool->pBufferPool  ||  (uint8_t *)pBuffer >= & pPool->pBufferPool [pPool->bufferCount * pPool->bufferSize])
	{
		aaTraceBuffPoolGiveFail (pAaCurrentTcb->taskIndex, bufPoolId & AA_HTAG_IMASK) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_7 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter () ;
	aaStackPush (& pPool->freeList, (aaStackNode_t *) pBuffer) ;
	pPool->bufferFree++ ;
	aaTraceBuffPoolGive (pAaCurrentTcb->taskIndex, bufPoolId & AA_HTAG_IMASK) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Return count of free buffers

aaError_t	aaBufferPoolGetCount	(aaBuffPoolId_t bufPoolId, uint32_t * pCount)
{
	aaBufferPool_t	* pPool ;

	pPool = aaBufferPoolCheck_ (bufPoolId, AA_ERROR_BPOOL_8) ;
	if (pPool == NULL)
	{
		aaTraceBuffPoolGetCountFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_8 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	* pCount = pPool->bufferFree ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Unconditionally restores the pool to its initial state

aaError_t	aaBufferPoolReset	(aaBuffPoolId_t bufPoolId)
{
	aaBufferPool_t	* pPool ;
	uint8_t			* pBuf ;
	uint32_t		ii ;

	pPool = aaBufferPoolCheck_ (bufPoolId, AA_ERROR_BPOOL_9) ;
	if (pPool == NULL)
	{
		aaTraceBuffPoolResetFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_BPOOL_9 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}


	// Build pools's buffers free list
	aaStackInitHead (& pPool->freeList) ;
	pBuf = pPool->pBufferPool ; 
	for (ii = 0 ; ii < pPool->bufferCount ; ii++)
	{
		aaStackPush (& pPool->freeList, (aaStackNode_t *) (void *) pBuf) ;
		pBuf = & pBuf[pPool->bufferSize] ;	// Ugly bus MISRA compliant!!
	}
	pPool->bufferFree = pPool->bufferCount ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
#endif	// AA_BUFPOOL_MAX > 0
