/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	tasksignal.c	Task signal handling

	When		Who	What
	25/11/13	ac	Creation
	13/08/19	ac	Add aaSignalPulse()

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

//--------------------------------------------------------------------------------
//	The current task waits for non 0 signal

aaError_t	aaSignalWait	(aaSignal_t sigsIn, aaSignal_t * pSigsOut, uint8_t mode, uint32_t timeOut)
{
	aaSignal_t sigs = sigsIn ;

	AA_ERRORCHECK (aaIsInIsr () == 0, AA_ENOTALLOWED, AA_ERROR_SIG_1) ;

	if (pSigsOut != NULL)
	{
		* pSigsOut = 0 ;
	}

	aaCriticalEnter () ;

	pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_SIGNALAND_ ;
	if (mode == AA_SIGNAL_AND)
	{
		pAaCurrentTcb->flags |= AA_FLAG_SIGNALAND_ ;
	}
	pAaCurrentTcb->sigsWait = sigs ;

	// Check if the expected signals are received
	sigs &= pAaCurrentTcb->sigsRecv ;
	if ((pAaCurrentTcb->flags & AA_FLAG_SIGNALAND_) != 0)
	{
		// AND : Check if all signals are received
		if (sigs != pAaCurrentTcb->sigsWait)
		{
			sigs = 0 ;
		}
	}

	if (sigs != 0)
	{
		// Some or all expected signals have arrived
		pAaCurrentTcb->sigsRecv &= (aaSignal_t) ~sigs ;		// Clear expected and obtained signals
		aaCriticalExit () ;
		if (pSigsOut != NULL)
		{
			* pSigsOut = sigs ;
		}
		return AA_ENONE ;
	}

	// The expected signal combination is not there
	if (timeOut == 0)
	{
		// No timeout so return: no signal
		aaCriticalExit () ;
		return AA_EWOULDBLOCK;
	}

	// Wait for signal
	aaTraceSignalWait (pAaCurrentTcb->taskIndex, pAaCurrentTcb->sigsWait) ;
	aaRemoveReady_ (pAaCurrentTcb) ;						// Remove from ready list
	pAaCurrentTcb->state = aaSignalWaitingState ;
	pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;	// Clear awake cause flags

	// Put task in delayed tasks list
	aaAddToDelayedList_ (pAaCurrentTcb, timeOut) ;

	aaCriticalExit () ;
	aaSchedule_ () ;

	// The current task was awakened by aaSignalSend/Pulse, or by timeout, or by aaTaskResume
	// The task state is aaReadyState
	// The task was removed from delayed tasks lists
	if ((pAaCurrentTcb->flags & AA_FLAG_DELAYTMO_) != 0)
	{
		// Awakened by timeout, no signal to return
		pAaCurrentTcb->flags &= (uint16_t) ~AA_FLAG_DELAYTMO_ ;
		return AA_ETIMEOUT ;
	}

	if (pSigsOut != NULL)
	{
		* pSigsOut = pAaCurrentTcb->objPtr.sigsWakeUp ;
	}
	return AA_ENONE ;
}
//--------------------------------------------------------------------------------
// Send signals (memorized) or pulse signals (not memorized) to a task
// pulseMask is not 0 and equal to sigs for pulsed signals

aaError_t	aaSignalSendPulse_	(aaTaskId_t taskId, aaSignal_t sigs, aaSignal_t pulseMask)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;
	aaSignal_t		sigsWakeUp ;
	uint8_t			bRequestSchedule = 0 ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	AA_ERRORCHECK (resValue == AA_ENONE, AA_EARG, AA_ERROR_SIG_2) ;

	aaCriticalEnter () ;

	if (pulseMask == 0)
	{
		aaTraceSignalSend (pTcb->taskIndex, sigs) ;
	}
	else
	{
		aaTraceSignalPulse (pTcb->taskIndex, sigs) ;
	}

	// Check if the expected signals are received
	pTcb->sigsRecv |= sigs ;				// Add new signals to already present signals
	sigsWakeUp = pTcb->sigsRecv & pTcb->sigsWait ;
	pTcb->sigsRecv &= (uint16_t) ~pulseMask ;			// Clear pulsed signals

	if (pTcb->state == aaSignalWaitingState)
	{
		if ((pTcb->flags & AA_FLAG_SIGNALAND_) != 0)
		{
			// AND : Check if all signals are received
			if (sigsWakeUp != pTcb->sigsWait)
			{
				sigsWakeUp = 0 ;
			}
		}

		// Awake the task only if the waited signals are present
		if (sigsWakeUp != 0)
		{
			// Set signals to return
			pTcb->objPtr.sigsWakeUp = sigsWakeUp ;
			pTcb->sigsRecv &= (uint16_t) ~sigsWakeUp ;			// Clear used signals

			// Wake up the task: remove it from delayed tasks list
			aaRemoveFromDelayedList_ (& pTcb->listNode) ;

			// If suspend is requested for pTcb, add it to suspended list, else add it to ready list
			bRequestSchedule = aaTcbPutInList_ (pTcb) ;
		}
	}
	aaCriticalExit () ;

	if ((bRequestSchedule != 0)  &&  (aaIsInIsr () == 0))
	{
		aaSchedule_ () ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Clear some signals in the task signal received word

aaError_t	aaSignalClear		(aaTaskId_t taskId, aaSignal_t sigs)
{
	aaTcb_t			* pTcb ;
	aaError_t		resValue ;

	// Get target TCB
	resValue = aaTaskGetTcb_ (taskId, & pTcb) ;
	AA_ERRORCHECK (resValue == AA_ENONE, AA_EARG, AA_ERROR_SIG_3) ;

	aaCriticalEnter () ;
	pTcb->sigsRecv &= (aaSignal_t) ~sigs ;
	aaCriticalExit () ;

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
