/*
--------------------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	tlsf.h		TLSF algorithm for Dynamic Storage Allocation
				For memory pool less than 256 K bytes, to lower block overhead

	When		Who	What
	05/03/17	ac	Creation

--------------------------------------------------------------------------------
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

#if ! defined AA_TLSF_H_
#define AA_TLSF_H_
//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

#include	<stddef.h>

typedef void *	hTlsf_t ;	// Memory pool handle for TSFL functions

// Initialize a new memory pool, size is in byte
// pMem should be aligned on 4 bytes
// Return NULL on error
hTlsf_t		tlsfInit	(void * pMem, uint32_t size, uint32_t bVerbose) ;

//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then tlsfMalloc returns NULL
void *		tlsfMalloc	(hTlsf_t hTlsf, uint32_t size) ;

//	Allocates memory for an array of nmemb elements of size bytes each
//	and returns a pointer to the allocated memory.
//	The memory is set to zero. If nmemb or size is 0, then tlsfCalloc returns NULL
void *		tlsfCalloc	(hTlsf_t hTlsf, uint32_t nmemb, uint32_t size) ;

//	Frees the memory space pointed to by ptr, which must have been returned by
//	a previous call to tlsfMalloc(), tlsfCalloc() or tlsfRealloc().
//	If ptr is NULL, no operation is performed.
void 		tlsfFree	(hTlsf_t hTlsf, void * ptr) ;

//	Attempts to resize the memory block pointed to by ptr that was previously allocated
//	with a call to tlsfMalloc or tlsfCalloc.
//	Ptr can be NULL, if size is 0 then return NULL.
void * 		tlsfRealloc	(hTlsf_t hTlsf, void * ptr, uint32_t size) ;

// Check memory pool consistency
aaError_t 	tlsfCheck (hTlsf_t hTlsf, uint32_t bVerbose) ;

// Get statistics
void	tlsfGetStat (hTlsf_t hTlsf, uint32_t * pSize, uint32_t * pUsed, uint32_t * pCount) ;


#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_TLSF_H_
