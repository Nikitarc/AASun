/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aamalloc.c	System dynamic memory management
				TLSF or BLOCK algorithm

	When		Who	What
	01/05/17	ac	Creation
	09/06/22	ac	Add: AA_ASSERT (((uintptr_t) pMem & 0x03u) == 0u) ;	// Verify 8 byte alignment

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
#include	"aakernel.h"	// for aaIsRunning

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#if (AA_WITH_MALLOC_TLSF == 1)

	#include	"aatlsf.h"

#endif	// AA_WITH_MALLOC_TLSF

//--------------------------------------------------------------------------------

#if (AA_WITH_MALLOC_BLOC == 1)

	#include	"aamemblock.h"

#endif		// AA_WITH_MALLOC_BLOC

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
#if (AA_WITH_MALLOC_TLSF == 1)


STATIC	aaMutexId_t mallocMutexId = AA_INVALID_MUTEX ;
STATIC	hTlsf_t		hPoolMalloc = NULL ;

//--------------------------------------------------------------------------------
//	Initialize system default memory partition
//	It should be called only once

aaError_t	aaMallocInit_	(uint8_t * pStart, uint8_t * pEnd, uint32_t bVerbose)
{
	aaError_t	res ;

	//	It should be called only once
	if (mallocMutexId != AA_INVALID_MUTEX)
	{
		return AA_ESTATE ;
	}

	// Create mutex
	res = aaMutexCreate (& mallocMutexId) ;
	if (res != AA_ENONE)
	{
		return res ;
	}

	// Create memory partition
	hPoolMalloc = tlsfInit (pStart, (uint32_t) (pEnd - pStart), bVerbose) ;
	if (hPoolMalloc == NULL)
	{
		return AA_EFAIL ;
	}

	return AA_ENONE ;
}

//--------------------------------------------------------------------------------

void		aaMallocStat 	(uint32_t * pSize, uint32_t * pUsed, uint32_t * pCount)
{
	tlsfGetStat (hPoolMalloc, pSize, pUsed, pCount) ;
}

//--------------------------------------------------------------------------------
//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then returns NULL

void *		aaMalloc		(uint32_t size)
{
	void	* pMem ;

	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	pMem = tlsfMalloc (hPoolMalloc, size) ;
	aaMutexGive (mallocMutexId) ;
	AA_ASSERT (((uintptr_t) pMem & 0x03u) == 0u) ;	// Verify 8 byte alignment
	return pMem ;
}

//--------------------------------------------------------------------------------
//	Allocates memory for an array of nmemb elements of size bytes each
//	and returns a pointer to the allocated memory.
//	The memory is set to zero. If nmemb or size is 0, then returns NULL

void *	aaCalloc (uint32_t nmemb, uint32_t size)
{
	void	* pMem ;

	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	pMem = tlsfCalloc (hPoolMalloc, nmemb, size) ;
	aaMutexGive (mallocMutexId) ;
	AA_ASSERT (((uintptr_t) pMem & 0x03u) == 0u) ;	// Verify 8 byte alignment
	return pMem ;
}

//--------------------------------------------------------------------------------
//	Attempts to resize the memory block pointed to by ptr that was previously allocated
//	with a call to aaMlloc or aaCalloc.
//	Ptr can be NULL, if size is 0 then return NULL.

#if (AA_WITH_REALLOC == 1)

void *		aaRealloc		(void * pMem, uint32_t size)
{
	void	* pMemResult ;

	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	pMemResult = tlsfRealloc (hPoolMalloc, pMem, size) ;
	aaMutexGive (mallocMutexId) ;
	AA_ASSERT (((uintptr_t) pMem & 0x03u) == 0u) ;	// Verify 8 byte alignment
	return pMemResult ;
}
#endif	// AA_WITH_REALLOC

//--------------------------------------------------------------------------------
//	Frees the memory space pointed to by ptr, which must have been returned by
//	a previous call to aaMalloc(), aaCalloc() or aaRealloc().
//	If ptr is NULL, no operation is performed.

void		aaFree			(void * pMem)
{
	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	tlsfFree (hPoolMalloc, pMem) ;
	aaMutexGive (mallocMutexId) ;
}

//--------------------------------------------------------------------------------
//	Non blocking aaFree()
//	Frees the memory space pointed to by ptr, which must have been returned by
//	a previous call to aaMalloc(), aaCalloc() or aaRealloc().
//	If ptr is NULL, no operation is performed.

aaError_t		aaTryFree		(void * pMem)
{
	aaError_t	res ;

	res = aaMutexTake (mallocMutexId, 0) ;
	if (res != AA_ENONE)
	{
		return res ;
	}
	tlsfFree (hPoolMalloc, pMem) ;
	aaMutexGive (mallocMutexId) ;
	return AA_ENONE ;
}
//--------------------------------------------------------------------------------
//	This function can be called before the kernel is started, but mallocMutexId must be created.
//	This function can be called by idle task.
//	The idle task can't block, and is identified by index 0.
//	So the idle task can't use aaMutexTake(), and use aaMutexTryTake().
//	To use aaTaskSelfId() the kernel should be started
//	If the mutex can't be taken the function return AA_ESTATE an no memory pool check is done

uint32_t	aaMemPoolCheck	(uint32_t bVerbose)
{
	uint32_t	res ;

	if (aaIsInIsr () != 0)
	{
		// Not allowed from ISR
		AA_ASSERT (0) ;
		return AA_ESTATE ;
	}

	if (aaMutexIsId (mallocMutexId) != AA_ENONE)
	{
		// The mutex is not created
		return AA_ESTATE ;
	}

	if ((aaIsRunning == 1)  &&  ((aaTaskSelfId () & AA_HTAG_IMASK) == 0))
	{
		// This is Idle task who can't block
		res = aaMutexTryTake (mallocMutexId) ;
		if (res != AA_ENONE)
		{
			return AA_ESTATE ;
		}
	}
	else
	{
		aaMutexTake (mallocMutexId, AA_INFINITE) ;
	}

	res = tlsfCheck (hPoolMalloc, bVerbose) ;
	aaMutexGive (mallocMutexId) ;
	return res ;
}

//--------------------------------------------------------------------------------
#endif		// AA_WITH_MALLOC_TLSF

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#if (AA_WITH_MALLOC_BLOC == 1)
//--------------------------------------------------------------------------------

STATIC	aaMutexId_t 		mallocMutexId = AA_INVALID_MUTEX ;
STATIC	aaMallocBlocId_t	hPoolMalloc = NULL ;

//--------------------------------------------------------------------------------
//	Initialize system default memory partition
//	It should be called only once

aaError_t	aaMallocInit_	(uint8_t * pStart, uint8_t * pEnd, uint32_t bVerbose)
{
	aaError_t	res ;

	//	It should be called only once
	if (mallocMutexId != AA_INVALID_MUTEX)
	{
		return AA_ESTATE ;
	}

	// Create mutex
	res = aaMutexCreate (& mallocMutexId) ;
	if (res != AA_ENONE)
	{
		return res ;
	}

	// Create memory partition
	res = aaInitMallocBloc (pStart, (uint32_t) (pEnd - pStart), & hPoolMalloc, uint32_t bVerbose) ;
	return res ;
}

//--------------------------------------------------------------------------------
//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then returns NULL

void *		aaMalloc		(uint32_t size)
{
	void		* pMem = NULL ;

	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	(void) aaMallocBloc (hPoolMalloc, size, & pMem) ;
	aaMutexGive (mallocMutexId) ;
	return pMem ;
}

//--------------------------------------------------------------------------------
// This function does nothing, but satisfy the linker
// The memory is not released (memory leak)

void		aaFree			(void * pMem)
{
	#if (AA_WITH_MALLOC_BLOC_ERRONFREE == 1)
	{
		AA_ERRORNOTIFY (AA_ERROR_MALLOC_1) ;
	}
	#endif
	(void) pMem ;
}

//--------------------------------------------------------------------------------
// This function does nothing, but satisfy the linker
// The memory is not released (memory leak)
// The return value is always AA_ENONE

aaError_t		aaTryFree		(void * pMem)
{
	#if (AA_WITH_MALLOC_BLOC_ERRONFREE == 1)
	{
		AA_ERRORNOTIFY (AA_ERROR_MALLOC_2) ;
	}
	#endif
	(void) pMem ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Get amount of free space

uint32_t	aaMallocSize ()
{
	uint32_t	size = 0 ;

	aaMutexTake (mallocMutexId, AA_INFINITE) ;
	(void) aaMallocBlocSize (hPoolMalloc, & size) ;
	aaMutexGive (mallocMutexId) ;

	return size;
}

//--------------------------------------------------------------------------------
#endif		// AA_WITH_MALLOC_BLOC

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
