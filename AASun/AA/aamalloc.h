/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aamalloc.h	System dynamic memory management

	When		Who	What
	01/05/17	ac	Creation

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

#if ! defined AA_MALLOC_H_
#define AA_MALLOC_H_
//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

#if (AA_WITH_MALLOC == 1)

	//	Initialize system default memory partition
	//	It should only be called once
	aaError_t	aaMallocInit_	(uint8_t * pStart, uint8_t * pEnd, uint32_t bVerbose) ;

	//	Access to system default memory partition
	void *		aaMalloc		(uint32_t size) ;

#endif

#if (AA_WITH_MALLOC_TLSF == 1)

void *		aaCalloc		(uint32_t nmemb, uint32_t size) ;
void *		aaRealloc		(void * pMem, uint32_t size) ;
void		aaFree			(void * pMem) ;
aaError_t	aaTryFree		(void * pMem) ;
uint32_t	aaMemPoolCheck	(uint32_t bVerbose) ;
void		aaMallocStat 	(uint32_t * pSize, uint32_t * pUsed, uint32_t * pCount) ;

#endif		// AA_WITH_MALLOC_TLSF


#if (AA_WITH_MALLOC_BLOC == 1)

void		aaFree			(void * pMem) ;
aaError_t	aaTryFree		(void * pMem) ;

uint32_t	aaMallocSize (void) ;

#endif		// AA_WITH_MALLOC_BLOC


#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_MALLOC_H_
