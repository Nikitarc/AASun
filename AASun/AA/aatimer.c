/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aatimer.c	Watchdog timer facility

	When		Who	What
	25/05/17	ac	Creation
	01/06/20	ac	Allow AA_TIMER_MAX to be 0

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
#include	"aatimer.h"

#if (AA_TIMER_MAX == 0)

//--------------------------------------------------------------------------------
//	Used by BSP for tick stretching

uint32_t	aaTimerTickToWait_			(void)
{
		return AA_INFINITE ;
}

#else

//--------------------------------------------------------------------------------
// Timer control block

typedef struct aaTimer_s
{
	aaListNode_t	node ;			// Mandatory first item in the structure
	uintptr_t		callbackArg;
	uint32_t		timeout ;

} aaTimer_t ;

//	To have a low data byte count:
//	1)	A single linked list is used for free timers.
//	2)	Free timers have a timeout value of AA_INFINITE, which is forbidden for allocated timers
//	3)	When the timer is in free list, the timer index in aaTimers array is in node.value
//	4)	The callback function pointer is memorized in node.pOwner. It is NULL for a non initialized timer

//	When a timer is in the free list or in the active timers list, aaListIsInUse (pTimer->node) is true.
//	When the timer is allocated and stopped, it is out of any list, so aaListIsInUse (pTimer->node) is false


// The timers array and free list
STATIC	aaTimer_t		aaTimers [AA_TIMER_MAX] BSP_ATTR_NOINIT ;
STATIC	aaStackNode_t	aaTimerFreeList ;

// The list of waiting timers
STATIC	aaListHead_t	aaTimerList ;

//--------------------------------------------------------------------------------
//	Initialize the timer package. Do it only once at kernel start

void		aaTimerInit_		(void)
{
	uint32_t	ii ;

	if (aaTimerFreeList.pNext == NULL)
	{
		aaListInit (& aaTimerList) ;
		aaStackInitHead (& aaTimerFreeList) ;
		for (ii = 0 ; ii < AA_TIMER_MAX ; ii++)
		{
			aaListClearNode (& aaTimers [ii].node) ;
			aaTimers [ii].node.value  = ii ;		// Timer index
			aaTimers [ii].timeout = AA_INFINITE ;	// This value indicates a free timer
			aaStackPush (& aaTimerFreeList, (aaStackNode_t *) & aaTimers [ii].node) ;
		}
	}
}

//--------------------------------------------------------------------------------
//	Used by BSP for tick stretching

uint32_t	aaTimerTickToWait_			(void)
{
	aaListNode_t	* pNode ;

	pNode = aaListGetFirst (& aaTimerList) ;
	if (aaListIsEnd (& aaTimerList, pNode) == 0)
	{
		// List is not empty
		return pNode->value ;
	}
	else
	{
		return AA_INFINITE ;
	}
}

//--------------------------------------------------------------------------------
//	Used by BSP for tick stretching

void	aaTimerUpdateTick_ (uint32_t nTick)
{
	aaListNode_t	* pNode ;

	pNode = aaListGetFirst (& aaTimerList) ;
	if (aaListIsEnd (& aaTimerList, pNode) == 0)
	{
		// List is not empty
		AA_KERNEL_ASSERT (pNode->value > nTick) ;	// Development check
		pNode->value -= nTick ;
	}
}

//--------------------------------------------------------------------------------
//	Put a timer in the waiting list
//	Internal API
//	Should be called within critical section

static	void	aaAddToTimerList_ (aaListNode_t * pTimerNode, uint32_t timeout)
{
	aaListNode_t	* pNode ;
	uint32_t		delay = timeout ;

	AA_ERRORASSERT (aaInCriticalCounter != 0,  AA_ERROR_TIMER_1) ;

	// Search in aaTimerList a node with delay higher than delay,
	// the timer will be inserted before this node.
	pNode = (aaListNode_t *) & aaTimerList ;
	// While pNode is not the last node in the list
	while (aaListIsLast (& aaTimerList,  pNode) == 0)
	{
		if (delay <= aaListGetNext (pNode)->value)
		{
			// Insert pTimerNode after pNode
			// pNode is not the last node : adjust next node value
			aaListGetNext (pNode)->value -= delay ;
			break ;
		}
		pNode = aaListGetNext (pNode) ;
		delay -= pNode->value ;
	}
	pTimerNode->value = delay ;
	aaListAddAfter (& aaTimerList, pNode, pTimerNode) ;
}

//--------------------------------------------------------------------------------
//	Called by kernel tick to handle timers timeout

//	Callback is called out of critical section, and the timer is stopped.
//	Callback return value  = 0 : timer stay stopped
//	Callback return value != 0 : the timer should be started
//	The callback can set timer parameters

void		aaTimerTick_		(void)
{
	aaListNode_t	* pNode ;
	aaTimer_t		* pTimer ;
	uint32_t		res ;

	aaCriticalEnter () ;

	pNode = aaListGetFirst (& aaTimerList) ;
	if (aaListIsEnd (& aaTimerList, pNode) == 0)
	{
		// List is not empty, decrement delay of first node
		AA_KERNEL_ASSERT (pNode->value != 0) ;	// Development check
		pNode->value-- ;

		while (pNode != (aaListNode_t *) & aaTimerList  &&  pNode->value == 0)
		{
			// Timeout for this node
			// Remove timer from timer waiting list. delay is 0, so no next node delay adjust
			aaListRemove (& aaTimerList, pNode) ;

			// Call callback, out of critical section
			aaCriticalExit () ;
			pTimer = (aaTimer_t *) pNode ;
			aaTraceTimerExpired (pTimer - aaTimers) ;
			res = (*(aaTimerCallback) pNode->ptr.pFn) (pTimer->callbackArg) ;
			aaCriticalEnter () ;

			if (res != 0)
			{
				// Restart timer
				aaAddToTimerList_ (pNode, pTimer->timeout) ;
			}

			pNode = aaListGetFirst (& aaTimerList) ;
		}
	}

	aaCriticalExit () ;
}

//--------------------------------------------------------------------------------
//	Check if a timerId is valid
//	If valid returns AA_ENONE, else AA_EFAIL

aaError_t	aaTimerIsId	(aaTimerId_t timerId)
{
	uint32_t	timerIndex = timerId & AA_HTAG_IMASK ;
	aaTimer_t	* pTimer = & aaTimers [timerIndex] ;
	aaError_t	resValue = AA_EFAIL ;

	if ((timerId & AA_HTAG_TMASK) == AA_HTAG_TIMER)
	{
		// The ID have the timer tag
		if ((timerIndex < AA_TIMER_MAX)  &&  (pTimer->timeout != AA_INFINITE))
		{
			// Index is in the range of timer descriptor array, and allocated
			resValue = AA_ENONE ;
		}
	}
	return resValue ;
}

//--------------------------------------------------------------------------------

static	aaTimer_t *		aaTimerCheckId_	(aaTimerId_t timerId, uint32_t errorNumber)
{
	aaTimer_t	* pTimer = & aaTimers [timerId & AA_HTAG_IMASK] ;

	#if (AA_WITH_ARGCHECK == 1)
	{
		if ((timerId & AA_HTAG_TMASK) != AA_HTAG_TIMER)
		{
			// timerId is not a timer handle. This is a fatal error
			AA_ERRORNOTIFY (errorNumber) ;
			while (1)	// Don't continue, even if AA_ERRORNOTIFY() returns
			{
			}
		}

		if (((timerId & AA_HTAG_IMASK) >= AA_TIMER_MAX)  ||  (pTimer->timeout == AA_INFINITE))
		{
			return NULL ;	// Error handled by the caller
		}
	}
	#else
		(void) errorNumber ;
	#endif

	return pTimer ;		// timerId is valid

}

//--------------------------------------------------------------------------------
//	Search for a free timer and return its identifier

aaError_t	aaTimerCreate	(aaTimerId_t * pTimerId)
{
	aaTimer_t		* pTimer ;

	if (pTimerId == NULL)
	{
		return AA_EARG ;
	}

	aaCriticalEnter (); 
	if (aaStackIsEmpty (& aaTimerFreeList) == 1)
	{
		// Timer free list is empty
		aaTraceTimerCreateFail (pAaCurrentTcb->taskIndex) ;
		aaCriticalExit ();
		AA_ERRORNOTIFY (AA_ERROR_TIMER_2 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EDEPLETED ;
	}

	pTimer = (aaTimer_t *) aaStackPop (& aaTimerFreeList) ;
	aaTraceTimerCreated (pAaCurrentTcb->taskIndex, pTimer->node.value) ;
	aaCriticalExit () ;

	// Default setting
	pTimer->timeout = 1 ;
	pTimer->node.ptr.pFn = NULL ;

	* pTimerId = (aaTimerId_t) (pTimer->node.value | AA_HTAG_TIMER) ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Stop the timer and put it in free state

aaError_t	aaTimerDelete	(aaTimerId_t timerId)
{
	aaTimer_t	* pTimer ;
	uint32_t	timerIndex = timerId & AA_HTAG_IMASK ;

	// Check id value
	pTimer = aaTimerCheckId_ (timerId, AA_ERROR_TIMER_3) ;
	if (pTimer == NULL)
	{
		aaTraceTimerDeleteFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_TIMER_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	// Timer should be allocated
	if (pTimer->timeout == AA_INFINITE)
	{
		return AA_ESTATE ;
	}

	// Remove timer from waiting list
	(void) aaTimerStop (timerId) ;

	// Set timer in free list
	pTimer->timeout = AA_INFINITE ;
	pTimer->node.value = timerIndex ;

	aaCriticalEnter () ;
	aaTraceTimerDeleted (timerIndex) ;
	aaStackPush (& aaTimerFreeList, (aaStackNode_t *) & pTimer->node) ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Set timer parameters
//	timeout from 1 to AA_INFINITE-1

aaError_t	aaTimerSet		(aaTimerId_t timerId, aaTimerCallback callback, uintptr_t arg, uint32_t timeout)
{
	aaTimer_t	* pTimer ;
	uint32_t	timerIndex = timerId & AA_HTAG_IMASK ;
	
	// Check id value
	pTimer = aaTimerCheckId_ (timerId, AA_ERROR_TIMER_4) ;
	if (pTimer == NULL)
	{
		aaTraceTimerSetFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_TIMER_4 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	if (timeout == 0  ||  timeout == AA_INFINITE  ||  callback == NULL)
	{
		return AA_EARG ;
	}
	pTimer = & aaTimers [timerIndex] ;

	// Timer should be allocated and not running
	if (pTimer->timeout == AA_INFINITE  ||  aaListIsInUse (& pTimer->node) != 0)
	{
		return AA_ESTATE ;
	}

	pTimer->timeout			= timeout ;
	pTimer->callbackArg		= arg ;
	pTimer->node.ptr.pFn	= (aaListFnPtr) callback ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Insert the timer in the timers waiting list.
//	If the timer is already in the waiting list, it is restarted with its initial timeout value

aaError_t	aaTimerStart	(aaTimerId_t timerId)
{
	aaTimer_t	* pTimer ;
	
	// Check id value
	pTimer = aaTimerCheckId_ (timerId, AA_ERROR_TIMER_5) ;
	if (pTimer == NULL)
	{
		aaTraceTimerStartFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_TIMER_5 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	aaCriticalEnter (); 

	// Timer should be allocated, and it must have been settled by aaTimerSet()
	if (pTimer->timeout == AA_INFINITE  ||  pTimer->node.ptr.pFn == NULL)
	{
		aaCriticalExit () ;
		return AA_ESTATE ;
	}

	// If the timer is running, stop it
	if (aaListIsInUse (& pTimer->node) != 0)
	{
		aaListNode_t	* pNode = & pTimer->node ;

		// Remove the timer from the timer waiting list
		if (aaListIsLast (& aaTimerList, pNode) == 0)
		{
			// This is not the last timer in the list, add this timer timeout to next timer timeout
			aaListGetNext (pNode)->value += pNode->value ;
		}
		aaListRemove (& aaTimerList, pNode) ;
	}

	// Add the timer to the timers waiting list
	aaAddToTimerList_ (& pTimer->node, pTimer->timeout) ;
	aaTraceTimerStart (pAaCurrentTcb->taskIndex) ;

	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	To stop the timer remove it from the timers waiting list
//	Removing an already stopped timer is not an error

aaError_t	aaTimerStop		(aaTimerId_t timerId)
{
	aaTimer_t		* pTimer ;
	aaListNode_t	* pNode ;
	aaError_t		retValue ;
	
	// Check id value
	pTimer = aaTimerCheckId_ (timerId, AA_ERROR_TIMER_6) ;
	if (pTimer == NULL)
	{
		aaTraceTimerStopFail (pAaCurrentTcb->taskIndex) ;
		AA_ERRORNOTIFY (AA_ERROR_TIMER_6 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	pNode = & pTimer->node ;

	aaCriticalEnter (); 

	// Timer should be allocated
	if (pTimer->timeout == AA_INFINITE)
	{
		retValue = AA_ESTATE ;
	}
	else
	{
		// Check if the timer is in the waiting list
		if (aaListIsInUse (pNode) == 1)
		{
			// Timer in waiting list: remove it
			if (aaListIsLast (& aaTimerList, pNode) == 0)
			{
				// This is not the last timer in the list, add this timer timeout to next timer timeout
				aaListGetNext (pNode)->value += pNode->value ;
			}
			aaListRemove (& aaTimerList, pNode) ;
		}
		retValue = AA_ENONE ;
	}

	aaCriticalExit () ;

	return retValue ;
}

//--------------------------------------------------------------------------------
#endif // AA_TIMER_MAX > 0
