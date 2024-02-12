/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aamallocbloc.c		Very simple memory allocator
						It does not permit memory to be freed once it has been allocated

	When		Who	What
	13/02/18	ac	Creation

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

#include	"aa.h"			// For INLINE, aaError_t
#include	"aaerror.h"
#include	"aamemblock.h"

#if (AA_WITH_MEMBLOCK == 1)

//---------------------------------------------------------------------------------
//	Very simple memory allocator: It does not permit memory to be freed once it has been allocated.
//	Very fast, no memory fragmentation.
//
//	The allocator is initialized with a block of memory, which is then divided on demand.
//	There is no overhead for allocated blocks.
//	Allocated blocs are always aligned on 8 bytes boundary, with a size multiple of 8.
//	Memory allocator descriptor size is very low: 8 bytes on 32 bits systems.
//	The descriptor of the memory allocator is allocated at the begining of the memory bloc it manage.

typedef struct blocDesc_s
{
	uint8_t		* pBloc ;
	uint32_t	size ;

} blocDesc_t ;

//--------------------------------------------------------------------------------
// Initialize a new memory pool
// size is in byte

aaError_t	aaInitMallocBloc (uint8_t * pBloc, uint32_t size, aaMallocBlocId_t * pId, uint32_t bVerbose)
{
	blocDesc_t	* pDesc ;
	uint32_t	offset ;

	(void) bVerbose ;

	// If pBloc not aligned on 8 bytes, align
	offset = (uint32_t) ((uintptr_t) pBloc & 0x07u) ;
	if (offset != 0)
	{
		offset = 8u - offset ;
		AA_KERNEL_ASSERT (! (size < offset)) ;
		if (size < offset)
		{
			return AA_EARG ;
		}
		size -= offset ;
		pBloc = & pBloc[offset] ;
	}

	// Check if the size is sufficient
	if (size < sizeof (blocDesc_t))
	{
		// The pool is too small
		AA_ERRORNOTIFY (AA_ERROR_MALLOCB_1 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}

	// Set  descriptor
	pDesc = (blocDesc_t *) pBloc ;
	pDesc->pBloc = & pBloc [sizeof (blocDesc_t)] ;
	pDesc->size  = size - sizeof (blocDesc_t) ;

	* pId = pDesc ;
	return AA_ENONE ;
}


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then tlsfMalloc returns NULL

aaError_t	aaMallocBloc		(aaMallocBlocId_t blockId, uint32_t size, void ** ppBloc)
{
	blocDesc_t	* pDesc = (blocDesc_t *) blockId ;

	if (blockId == NULL ||  ppBloc == NULL)
	{
		AA_ERRORNOTIFY (AA_ERROR_MALLOCB_2 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}
	* ppBloc = NULL ;

	//	Align size to 8
	size = (size + 7u) & ~0x07u ;

	if (pDesc->size < size)
	{
		// The pool is too small
		AA_ERRORNOTIFY (AA_ERROR_MALLOCB_3 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EFAIL ;
	}

	if (size != 0)
	{
		* ppBloc = pDesc->pBloc ;
		pDesc->pBloc = & pDesc->pBloc [size] ;
		pDesc->size -= size ;
	}
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
//	Return free size in the memory partition

aaError_t	aaMallocBlocSize	(aaMallocBlocId_t blockId, uint32_t * pSize)
{
	blocDesc_t	* pDesc = (blocDesc_t *) blockId ;

	if (blockId == NULL  ||  pSize == NULL)
	{
		AA_ERRORNOTIFY (AA_ERROR_MALLOCB_4 | AA_ERROR_FORCERETURN_FLAG) ;
		return AA_EARG ;
	}
	* pSize = pDesc->size ;
	return AA_ENONE ;
}

//--------------------------------------------------------------------------------
#endif		// AA_WITH_MEMBLOCK
