/*
--------------------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aamallocbloc.h		Very simple memory allocator
						It does not permit memory to be freed once it has been allocated

	When		Who	What
	13/02/18	ac	Creation

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

#if ! defined AA_MALLOCBLOC_H_
#define AA_MALLOCBLOC_H_
//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

#include	<stddef.h>

typedef	void	* aaMallocBlocId_t ;	// Memory pool identifier

// Initialize a new memory pool, size is in byte
aaError_t	aaInitMallocBloc	(uint8_t * pBloc, uint32_t size, aaMallocBlocId_t * pId, uint32_t bVerbose) ;

//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then aaMallocBloc returns NULL
aaError_t	aaMallocBloc		(aaMallocBlocId_t blockId, uint32_t size, void ** ppBloc) ;

//	Get amount of free space
aaError_t	aaMallocBlocSize	(aaMallocBlocId_t blockId, uint32_t * pSize) ;


#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_MALLOCBLOC_H_
