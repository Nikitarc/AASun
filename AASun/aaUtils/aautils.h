/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aautils.c	Some utility functions

	When		Who	What
	21/04/18	ac	Creation
	20/09/21	ac	Add argument to displaytaskInfo()

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

#ifndef AAUTILS_H_
#define AAUTILS_H_

#include <stdint.h>

//----------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif


void		displaytaskInfo		(aaTaskInfo_t * pTaskInfo) ;
void    	aaDump 				(uint8_t * pBuffer, uint32_t nb) ;
void     	aaDumpEx			(uint8_t * pBuffer, uint32_t nb, void * pAddrs) ;

void		aaRandom1Seed		(uint32_t seed) ;
uint32_t	aaRandom1			(void) ;

void		aaRandom2Seed		(uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5) ;
uint32_t	aaRandom2			(void) ;

void		aaStrToUpper		(char * pStr) ;

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------
#endif // AAUTILS_H_
