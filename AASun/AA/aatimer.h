/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aatimer.c	Watchdog timer facility

	When		Who	What
	25/05/17	ac	Creation

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

#if ! defined AA_TIMER_H_
#define AA_TIMER_H_
//--------------------------------------------------------------------------------

typedef	uint16_t	aaTimerId_t ;		// Timer		id from aaTimerCreate()

// Timer callback prototype
typedef uint32_t (* aaTimerCallback) (uintptr_t arg) ;


#ifdef __cplusplus
extern "C" {
#endif

aaError_t	aaTimerCreate		(aaTimerId_t * pTimerId) ;
aaError_t	aaTimerDelete		(aaTimerId_t timerId) ;
aaError_t	aaTimerSet			(aaTimerId_t timerId, aaTimerCallback callback, uintptr_t arg, uint32_t timeout) ;
aaError_t	aaTimerStart		(aaTimerId_t timerId) ;
aaError_t	aaTimerStop			(aaTimerId_t timerId) ;

// RTK internal use only
void		aaTimerInit_		(void) ;
void		aaTimerTick_		(void) ;
void		aaTimerUpdateTick_	(uint32_t nTick) ;
uint32_t	aaTimerTickToWait_	(void) ;

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_TIMER_H_
