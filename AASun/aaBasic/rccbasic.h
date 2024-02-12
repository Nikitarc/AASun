/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	rccbasic.h	Some routine to manage basic RCC features

	When		Who	What
	01/02/22	ac	Creation: split from gpiobasic.h

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

#if ! defined RCCBASIC_H_
#define	RCCBASIC_H_
//----------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

uint32_t	rccGetSystemClockFreq		(void) ;
uint32_t	rccGetHCLKClockFreq			(void) ;
uint32_t	rccGetPCLK1ClockFreq		(void) ;
uint32_t	rccGetPCLK2ClockFreq		(void) ;

void		rccEnableUartClock			(void * deviceAddress) ;
void		rccDisableUartClock			(void * deviceAddress) ;
void		rccResetUart				(void * deviceAddress) ;

void		rccEnableTimClock			(void * deviceAddress) ;
void		rccDisableTimClock			(void * deviceAddress) ;
void		rccResetTim 				(void * deviceAddress) ;

void		rccEnableClock				(void * deviceAddress, uint32_t enableBitMask) ;
void		rccDisableClock				(void * deviceAddress, uint32_t enableBitMask) ;
void		rccResetPeriph 				(void * deviceAddress, uint32_t enableBitMask) ;
uint32_t	rccGetClockFrequency		(void * deviceAddress) ;
uint32_t	rccGetTimerClockFrequency	(void * deviceAddress) ;

#ifdef __cplusplus
}
#endif
//----------------------------------------------------------------------
#endif		// RCCBASIC_H_
