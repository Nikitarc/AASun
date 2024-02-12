/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	tlsf.c		TLSF algorithm for Dynamic Storage Allocation
				For memory pool less than 256 K bytes
				Deterministic: no loop, the worst case is bounded

	When		Who	What
	05/03/17	ac	Creation
	17/04/20	ac	Get rid of gcc warning about conversion
	07/03/22	ac	When reallocate fails, doesn't free the block anymore
	09/06/22	ac	Fail to align to 0x04 or 0x0C in tlsfInit()

	Reference:	http://www.gii.upv.es/tlsf/
				[PDF] Description of the TLSF Memory Allocator Version 2.0 

TO DO:

OK	Small blocks management (<128 bytes)
OK	tslfRealloc
OK	tslfCalloc
OK	tslfCheck (printf)
	Dynamically configure memory pool 16/32../256 KB
OK	Config lock
	Optional debug (optimize size and speed)
OK	Do not use the 1st guard block
	Replace pointer arithmetic

	WARNING: GCC -Wconversion  : complain about conversion to 'short unsigned int:15' from 'int'
	=> don't use GCC -Wconversion option

	There is warning: cast increases required alignment of target type [-Wcast-align]
	So :        (sFreeHeader_t *)           ((char *) pBlock - SLICETOSIZE (pBlock->prevOffset)) ;
	is written: (sFreeHeader_t *) (void *)  ((char *) pBlock - SLICETOSIZE (pBlock->prevOffset)) ;
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
/*
	The block overhead is only 4 bytes.
	This overhead allows the link to the next block and the previous block,
	which makes it possible to check the consistency of all the blocks in the memory pool.

	The smallest allocable block is 16 bytes
	The size of the blocks is a multiple of 8 bytes
	All blocks are 8 bytes aligned 
*/

#include	"aa.h"			// For INLINE, aaError_t
#include	"aaerror.h"
#include	"aatlsf.h"
#include	"aaprintf.h"

// Include informations about amount of used memory
#define	AA_WITH_TLSF_STAT	1

#if (AA_WITH_TLSF == 1)

//--------------------------------------------------------------------------------
// Define the memory pool topology
// FLI_MAX_INDEX define the size of the largest managed block, therefore the memory pool size
// For this implementation of TLSF must have : FLI_MAX_INDEX < 18

											// Cost without AA_WITH_TLSF_STAT
#define	FLI_MAX_INDEX	17					// 17 => 262143 max (cost 444 Bytes, 0.16%)
											// 16 => 131071 max (cost 408 Bytes, 0.31%) 
											// 15 =>  65535 max (cost 372 Bytes, 0.56%) 
											// 14 =>  32767 max (cost 336 Bytes, 1.0%) 
											// 13 =>  16383 max (cost 300 Bytes, 1.8%) 
											// 12 =>   8191 max (cost 264 Bytes, 3.2%) 
#define	SLI_LOG2		4					// SLI_MAX = 2 ^ SLI_LOG2
#define	SLI_MAX			(1 << SLI_LOG2)		// Count of range in each class
#define	SLICE_LOG2		3					// SLICE is 2^SLICE_LOG2
#define	SLICE			(1 << SLICE_LOG2)	// Blocks are allocated with an integer count of SLICE bytes

//--------------------------------------------------------------------------------

#define	FLI_OFFSET		6
#define	FLI_COUNT		(FLI_MAX_INDEX - FLI_OFFSET + 1)
#define	SLI_COUNT		SLI_MAX

#define	MIN_SIZE		16								// Byte size of smallest block
#define	MAX_SIZE		((1 << (FLI_MAX_INDEX+1)) -1)	// Byte size of biggest block
#define	SMALL_BLOCK		128								// Small block size is < SMALL_BLOCK

// Flags of sFreeHeader_t
#define	BIT_FREE		1		// The block with this bit is free
#define	BIT_BUSY		0		// The block with this bit is busy
#define	BIT_LAST		1		// The block is the last in the pool
#define	BIT_NOTLAST		0		// The block is not the last in the pool

//--------------------------------------------------------------------------------
// Header of free blocks

typedef struct sFreeHeader_s
{
	// The first word of every block (block overhead)
	uint32_t				prevOffset : 15 ;	// Slice offset of physical previous block
	uint32_t				freeFlag   :  1 ;	// 1 if this block is free
	uint32_t				sizeSlice  : 15 ;	// The size of the block (in slice count)
	uint32_t				lastFlag   :  1 ;	// The block is the last ;

	// Only used if the block is free, and in the free list
	struct sFreeHeader_s	* pNext ;
	struct sFreeHeader_s	* pPrev ;

} sFreeHeader_t ;

// TLSF pool descriptor

typedef struct sEnv_s
{
	sFreeHeader_t	* pMemLast ;		// Address of last block of the pool
	uintptr_t		poolBase ;
	uint32_t		allocCount ;		// Count of tlsfMalloc - count of tlsfFree (debug only)
	uint32_t		flBitmap ;
	uint32_t		slBitmap [FLI_COUNT] ;

	uint16_t		table [FLI_COUNT][SLI_COUNT] ;
	#if (AA_WITH_TLSF_STAT == 1)
		uint32_t	size ;				// Initial free size (bytes)
		uint32_t	used ;				// Amount used (bytes)
	#endif
} sEnv_t ;

//--------------------------------------------------------------------------------

#define	ROUNDUP(x, y)			(((x) + ((y)-1u)) & ~((y)-1u))
#define	ROUNDDOWN(x, y)			((x) & ~((y)-1u))
#define TOSLICEOFFSET(pBlock)	(uint16_t) (((uintptr_t) pBlock - pEnv->poolBase) >> SLICE_LOG2)
#define	FROMSLICEOFFSET(offset)	(sFreeHeader_t *) (pEnv->poolBase + (uintptr_t) ((offset) << SLICE_LOG2))
#define SIZETOSLICE(size)		((size) >>  SLICE_LOG2)
#define SLICETOSIZE(size)		((uint32_t)((size) <<  SLICE_LOG2))

//--------------------------------------------------------------------------------

//uint32_t	msBit	(uint32_t word) ;
//uint32_t	lsBit	(uint32_t word) ;

#if defined (_MSC_VER)
	// For Windows port (unit tests)
	#include <intrin.h>

	#pragma intrinsic(_BitScanReverse)
	#pragma intrinsic(_BitScanForward)

	// fls : Return MSB bit index, 0x81 -> index 7
	static INLINE uint32_t msBit (uint32_t word)
	{
		uint32_t index;
		return _BitScanReverse (& index, word) ? index : -1 ;
	}

	// ffs : Return LSB bit index, 0x81 -> index 0
	static INLINE uint32_t lsBit (uint32_t word)
	{
		uint32_t index;
		return _BitScanForward (& index, word) ? index : -1 ;
	}

#endif	// _MSC_VER


// gcc 3.4 and above have builtin support, specialized for architecture.
// Some compilers masquerade as gcc; patchlevel test filters them out.
#if defined (__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)) && defined (__GNUC_PATCHLEVEL__)

	// fls : Return MSB bit index, 0x81 -> index 7
	__STATIC_INLINE uint32_t msBit (uint32_t word)
	{
		return (31u - (uint32_t) __CLZ (word)) ;
	}

	// ffs : Return LSB bit index, 0x81 -> index 0
	__STATIC_INLINE uint32_t lsBit (uint32_t word)
	{
		return (uint32_t) __builtin_ffs ((int) word) - 1u;
	}

#endif	// GNU

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

__STATIC_INLINE void getMapping (uint32_t size, uint32_t * pFli, uint32_t * pSli)
{
	if (size < SMALL_BLOCK)
	{
		* pFli = 0 ;
		* pSli = size / (SMALL_BLOCK / SLI_MAX) ; 
	}
	else
	{
		* pFli = msBit (size) ;
		* pSli = (size >> (* pFli - SLI_LOG2)) - SLI_MAX ;
		* pFli -= FLI_OFFSET ;
	}
}

//--------------------------------------------------------------------------------

__STATIC_INLINE sFreeHeader_t * getNextBlock (sFreeHeader_t * pBlock)
{
	if (pBlock->lastFlag == BIT_LAST)
	{
		return NULL ;	// This is the last block
	}
	return (sFreeHeader_t *) (void*) ((char *) pBlock + SLICETOSIZE (pBlock->sizeSlice)) ;
}

//--------------------------------------------------------------------------------

__STATIC_INLINE sFreeHeader_t * getPrevBlock (sFreeHeader_t * pBlock)
{
	if (pBlock->prevOffset == 0)
	{
		return NULL ;	// This is the first block
	}
	return (sFreeHeader_t *) (void *) ((char *) pBlock - SLICETOSIZE (pBlock->prevOffset)) ;
}

//--------------------------------------------------------------------------------
//	Insert a free block in its size class free list

static void insertBlock (sEnv_t * pEnv, sFreeHeader_t * pBlock)
{
	uint32_t		fli, sli ;
	sFreeHeader_t	* pHeader ;

	// Get class indexes
	getMapping (SLICETOSIZE (pBlock->sizeSlice), & fli, & sli) ;

	// Get class pointer, and build links
	pBlock->pNext = NULL ;
	pBlock->pPrev = NULL ;
	if (pEnv->table [fli][sli] != 0)
	{
		pHeader = FROMSLICEOFFSET (pEnv->table [fli][sli]) ;
		pHeader->pPrev = pBlock ;
		pBlock->pNext = pHeader ;
	}

	// Update table
	pEnv->table[fli][sli] = TOSLICEOFFSET(pBlock) ;

	/* Indicate that the class have free blocks. */
	pBlock->freeFlag = BIT_FREE ;
	pEnv->flBitmap      |= (1U << fli) ;
	pEnv->slBitmap[fli] |= (1U << sli) ;
}

//--------------------------------------------------------------------------------
// Remove block from class list

static void removeBlock (sEnv_t * pEnv, sFreeHeader_t * pBlock, uint32_t fli, uint32_t sli)
{

	if (pBlock->pNext)
	{
		pBlock->pNext->pPrev = pBlock->pPrev ;
	}
	if (pBlock->pPrev)
	{
		pBlock->pPrev->pNext = pBlock->pNext ;
	}
	if (FROMSLICEOFFSET (pEnv->table[fli][sli]) == pBlock)
	{
		// pBlock is first in list
		if (pBlock->pNext)
		{
			pEnv->table[fli][sli] = TOSLICEOFFSET (pBlock->pNext) ;
		}
		else
		{
			pEnv->table[fli][sli] = 0 ;	// pBlock was alone in the list
		}
	}
	AA_KERNEL_ASSERT (pBlock->freeFlag == BIT_FREE) ;	// Development check
	AA_KERNEL_ASSERT (pBlock->sizeSlice != 0) ;			// Development check

	// Update bitmaps
	if (pBlock->pNext == 0)
	{
		pEnv->slBitmap[fli] &= ~(1U << sli) ;
		if (pEnv->slBitmap[fli] == 0)
		{
			pEnv->flBitmap &= ~(1U << fli) ;
		}
	}
}

//--------------------------------------------------------------------------------
// Split the oversize of block and insert it in class size list
// overSize is in byte, multiple of SLICE

static void	removeRemainder (sEnv_t * pEnv, sFreeHeader_t * pBlock, uint32_t overSize)
{
	sFreeHeader_t	* pRemainder ;
	uint32_t		blockSize = SLICETOSIZE (pBlock->sizeSlice) - overSize ;

	// Check if sizes are multiple of SLICE size
	AA_KERNEL_ASSERT ((blockSize & (SLICE - 1)) == 0) ;		// Development check
	AA_KERNEL_ASSERT ((overSize & (SLICE - 1)) == 0) ;		// Development check

	overSize = SIZETOSLICE (overSize) ;		// Now overSize is in SLICE unit

	// Build remainder block ;
	pRemainder = (sFreeHeader_t *) (void*) ((char *) pBlock + blockSize) ;
	pRemainder->sizeSlice  = 0x7FFF & overSize ;
	pRemainder->prevOffset = 0x7FFF & (pBlock->sizeSlice - overSize) ;
	pRemainder->freeFlag   = BIT_FREE ;
	pRemainder->lastFlag   = pBlock->lastFlag ;

	// Finishing pRemainder processing
	if (pRemainder->lastFlag == BIT_LAST)
	{
		pEnv->pMemLast = pRemainder ;
	}
	else
	{
		sFreeHeader_t	* pNext ;
		uint32_t		sli, fli ;

		// Update prev link of next block of pRemainder
		pNext = getNextBlock (pBlock) ;
		AA_KERNEL_ASSERT (pNext != NULL) ;		// Development check, pRemainder pNext can't be null
		pNext->prevOffset = 0x7FFF & overSize ;

		// pNext can be free, so may need to coalesce with remainder
		if (pNext->freeFlag == BIT_FREE)
		{
			getMapping (SLICETOSIZE (pNext->sizeSlice), & fli, & sli) ;
			removeBlock (pEnv, pNext, fli, sli) ;
			pRemainder->lastFlag = pNext->lastFlag ;
			pRemainder->sizeSlice = 0x7FFFu & ((uint32_t) pRemainder->sizeSlice + pNext->sizeSlice) ;
			if (pRemainder->lastFlag == BIT_NOTLAST)
			{
				pNext = getNextBlock (pNext) ;
				pNext->prevOffset = pRemainder->sizeSlice ;
			}
		}
	}
	insertBlock (pEnv, pRemainder) ;	// Remainder block Inserted in free list

	// If this is the new last block of the physical list, memorize
	if (pRemainder->lastFlag == BIT_LAST)
	{
		pEnv->pMemLast = pRemainder ;
	}

	// Update pBlock
	pBlock->lastFlag  = BIT_NOTLAST ;
	pBlock->sizeSlice = 0x7FFF & SIZETOSLICE (blockSize) ; // From now we don't need pBlock
}

//--------------------------------------------------------------------------------
// Initialize a new memory pool
// size is in byte
// pMem should be aligned on 4 bytes

hTlsf_t	tlsfInit	(void * pMem, uint32_t size, uint32_t bVerbose)
{
	sEnv_t			* pEnv ;
	sFreeHeader_t	* pHeader ;
	uint32_t		temp ;
	uint32_t		ii, jj ;

	// Memory pool should be aligned on 4 bytes
	AA_ERRORASSERT (((uintptr_t)pMem & 0x03) == 0,  AA_ERROR_TLSF_2) ;

	// Compute size of environment struct
	temp = sizeof (sEnv_t) ;

	// Align start address of blocks to 0x4 or 0xC (so user block address are aligned on SLICE bytes)
	ii = (uint32_t) (((uintptr_t) pMem + temp) & 0x0F) ;
	if (ii == 0x00  ||  ii == 0x08)
	{
		temp += 4 ;
	}

	// Check pool size
	if (size < (temp + MIN_SIZE))
	{
		// Not enough space for pool
		return NULL ;
	}

	// Initialize environment
	pEnv = (sEnv_t *) pMem ;
	for (ii = 0 ; ii < FLI_COUNT ; ii++)
	{
		for (jj = 0 ; jj < SLI_COUNT ; jj++)
		{
			pEnv->table [ii][jj] = 0 ;
		}
	}

	pEnv->flBitmap = 0 ;
	for (jj = 0 ; jj < FLI_COUNT ; jj++)
	{
		pEnv->slBitmap [jj] = 0 ;
	}

	// Compute free block size. Should be integer count of SLICE
	size = ROUNDDOWN (size - temp, SLICE) ;
	if (size > MAX_SIZE)
	{
		// Too big block
		size = ROUNDDOWN (MAX_SIZE, SLICE)   ;
	}

	// Build the free block
	pHeader = (sFreeHeader_t *) (void*) (& ((uint8_t *) pMem) [temp]) ;
	AA_ASSERT (((((uintptr_t) pHeader) + 4) & 0x03u) == 0u) ;	// Check correct 8 byte alignment

	pHeader->freeFlag   = BIT_FREE ;
	pHeader->lastFlag   = BIT_LAST ;
	pHeader->prevOffset = 0 ;		// No prev block
	pHeader->sizeSlice  = 0x7FFFu & SIZETOSLICE (size) ;
	pEnv->pMemLast		= pHeader ;
	pEnv->poolBase		= (uintptr_t) pHeader - SLICE ;	// -slice to avoid a value of 0 in pEnv->table for the first block
	pEnv->allocCount	= 0 ;
	#if (AA_WITH_TLSF_STAT == 1)
	{
		pEnv->size = size ;
		pEnv->used = 0 ;
	}
	#endif

	// Insert free block in class list
	insertBlock (pEnv, pHeader);
	if (bVerbose != 0)
	{
		aaPrintf ("tlsfInit Block:%08X, size:%u/%u, end %08X\n", pHeader, size, pHeader->sizeSlice, (char *) pHeader + size) ;
	}

	return (hTlsf_t) pEnv ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Allocates size bytes and returns a pointer to the allocated memory.
//	The memory is not initialized.
//	If size is 0, then tlsfMalloc returns NULL

void *	tlsfMalloc (hTlsf_t hTlsf, uint32_t size)
{
	sEnv_t			* pEnv = (sEnv_t *) hTlsf ;
	uint32_t		fli, sli ;
	sFreeHeader_t	* pBlock ;
	uint32_t		target, temp ;

	if (size == 0)
	{
		return NULL ;
	}
	pBlock = NULL ;

	// Adjust size: add block overhead, round to upper SLICE, then to next size class.
	// Get FLI/SLI of the class
	if (size < MIN_SIZE - sizeof (uint32_t))
	{
		size = MIN_SIZE - sizeof (uint32_t) ;	// Min size minus overhead
	}
	size = ROUNDUP (size + sizeof (uint32_t), SLICE) ;
	target = size + (1UL << (msBit(size) - SLI_LOG2)) - 1u ;
	getMapping (target, & fli, & sli) ;

	// Find a free block
	temp = pEnv->slBitmap [fli] & (~0u << sli) ;
	if (temp != 0)
	{
		sli = lsBit (temp) ;
	}
	else
	{
		temp = pEnv->flBitmap & (~0u << (fli+1u)) ;
		if (temp == 0)
		{
			return NULL ;	// No suitable free block
		}
		fli = lsBit (temp) ;
		sli = lsBit (pEnv->slBitmap [fli]) ;
	}
	pBlock = FROMSLICEOFFSET (pEnv->table [fli][sli]) ;

	removeBlock (pEnv, pBlock, fli, sli) ;
	pBlock->freeFlag = BIT_BUSY ;

	// If the block is too large: split it
	temp = SLICETOSIZE (pBlock->sizeSlice) - size ;
	if (temp >= MIN_SIZE)
	{
		// Need to split
		removeRemainder (pEnv, pBlock, temp) ;
	}

	#if (AA_WITH_TLSF_STAT == 1)
	{
		pEnv->used += SLICETOSIZE (pBlock->sizeSlice) ;
	}
	#endif
	pEnv->allocCount ++ ;
	return (void *) ((uintptr_t) pBlock + sizeof (uint32_t)) ;
}

#if (AA_WITH_TLSF_STAT == 1)
#endif
//--------------------------------------------------------------------------------
//	Allocates memory for an array of nmemb elements of size bytes each
//	and returns a pointer to the allocated memory.
//	The memory is set to zero. If nmemb or size is 0, then tlsfCalloc returns NULL

void *	tlsfCalloc (hTlsf_t hTlsf, uint32_t nmemb, uint32_t size)
{
	uint8_t			* pBlock ;
	uint8_t			* pMem ;

	pBlock = NULL ;
	size = size * nmemb ;
	if (size != 0)
	{
		pBlock = (uint8_t *) tlsfMalloc (hTlsf, size) ;
		if (pBlock != NULL)
		{
			// Fast 0 initialize
			pMem = pBlock ;
			for ( ; size >= 4u ; size -= 4u)
			{
				* (uint32_t *) (void*) pMem = 0x0u ;
				pMem += 4u ;
			}
			for ( ; size > 0 ; size--)
			{
				* pMem++ = 0x0u ;
			}
		}
	}
	return pBlock ;
}

//--------------------------------------------------------------------------------
//	Frees the memory space pointed to by ptr, which must have been returned by
//	a previous call to tlsfMalloc(), tlsfCalloc() or tlsfRealloc().
//	If ptr is NULL, no operation is performed.

void 	tlsfFree (hTlsf_t hTlsf, void * ptr)
{
	sEnv_t			* pEnv = (sEnv_t *) hTlsf ;
	sFreeHeader_t	* pBlock, * pHeader ;
	uint32_t		fli, sli ;

	if (ptr == NULL)
	{
		return ;
	}
	pBlock = (sFreeHeader_t *) (void*) ((char *) ptr - sizeof (uint32_t)) ;
	#if (AA_WITH_TLSF_STAT == 1)
	{
		pEnv->used -= SLICETOSIZE (pBlock->sizeSlice) ;
	}
	#endif

	//	Check if this block is in the memory partition chain.
	//	We can get the supposed next block from the header of the block.
	//	If the size of the block is equal to the prevOffset of the supposed next block,
	//	chances are that the supposed next block is the real next block.
	//	Same thing with previous block.
	//	If the 4 links are consistent, the bloc is probably in the memory partition chain
	pHeader = getPrevBlock (pBlock) ;
	if (pHeader != NULL)
	{
		if (pBlock->prevOffset != pHeader->sizeSlice)
		{
			AA_ERRORNOTIFY (AA_ERROR_TLSF_3 | AA_ERROR_FORCERETURN_FLAG) ;
			return ;	// Links are broken, don't try to free the block
		}
	}

	pHeader = getNextBlock (pBlock) ;
	if (pHeader != NULL)
	{
		if (pBlock->sizeSlice != pHeader->prevOffset)
		{
			AA_ERRORNOTIFY (AA_ERROR_TLSF_4 | AA_ERROR_FORCERETURN_FLAG) ;
			return ;	// Links are broken, don't try to free the block
		}
	}

	// Check if next block exist and is free
	if (pHeader != NULL  && pHeader->freeFlag == BIT_FREE)
	{
		// Coalesce blocks
		getMapping (SLICETOSIZE (pHeader->sizeSlice), & fli, & sli) ;
		removeBlock (pEnv, pHeader, fli, sli) ;
		pBlock->lastFlag = pHeader->lastFlag ;
		pBlock->sizeSlice = 0x7FFFu & ((uint32_t) pBlock->sizeSlice + pHeader->sizeSlice) ;
		pHeader = getNextBlock (pHeader) ;
		if (pHeader != NULL)
		{
			pHeader->prevOffset = pBlock->sizeSlice ;
		}
	}

	// Check if previous block exist and is free
	pHeader = getPrevBlock (pBlock) ;
	if (pHeader != NULL  &&  pHeader->freeFlag == BIT_FREE)
	{
		// Coalesce with prev block
		getMapping (SLICETOSIZE (pHeader->sizeSlice), & fli, & sli) ;
		removeBlock (pEnv, pHeader, fli, sli) ;
		pHeader->lastFlag = pBlock->lastFlag ;
		pHeader->sizeSlice = 0x7FFFu & ((uint32_t)pHeader->sizeSlice + pBlock->sizeSlice) ;
		pBlock = getNextBlock (pBlock) ;
		if (pBlock != NULL)
		{
			pBlock->prevOffset = pHeader->sizeSlice ;
		}
		pBlock = pHeader ;
	}

	// If this is the new last block of the physical list, memorize
	if (pBlock->lastFlag == BIT_LAST)
	{
		pEnv->pMemLast = pBlock ;
	}

	// Insert free block in class list
	insertBlock (pEnv, pBlock);
	pEnv->allocCount -- ;
}

//--------------------------------------------------------------------------------
// Copy data from one block to another. Data may overlap (pDst < pSrc).
// Blocks size is always multiple of 4, so copy uint32_t.

#if (AA_WITH_REALLOC == 1)

__STATIC_INLINE void copyData (uint32_t * pSrc, uint32_t * pDst, uint32_t count)
{
	while (count--)
	{
		* pDst++ = * pSrc++ ;
	}
}

//	Attempts to resize the memory block pointed to by ptr that was previously allocated
//	with a call to tlsfMalloc or tlsfCalloc or tlsfRealloc.
//	If ptr is NULL then this is equivalent to malloc
//	If size is 0 and ptr is not NULL then free the block and return NULL.
//	If there is not enough memory, the old memory block is left untouched and NULL pointer is returned.

void * 	tlsfRealloc (hTlsf_t hTlsf, void * ptr, uint32_t size)
{
	sEnv_t			* pEnv = (sEnv_t *) hTlsf ;
	sFreeHeader_t	* pBlock, * pNext, * pPrev ;
	uint32_t		sizeBlock, sizePrev, sizeNext ;
	uint32_t		fli, sli ;

	if (size == 0)
	{
		// Free the block
		tlsfFree (hTlsf, ptr) ;
		return NULL ;
	}

	if (ptr == NULL)
	{
		// Alloc a new block ;
		return tlsfMalloc (hTlsf, size) ;
	}

	size = ROUNDUP (size + sizeof (uint32_t), SLICE) ;
	pBlock = (sFreeHeader_t *) (void*) ((char *) ptr - sizeof (uint32_t)) ;
	sizeBlock = SLICETOSIZE (pBlock->sizeSlice) ;

	if (size == sizeBlock)
	{
		// Same size, nothing to do
		return ptr ;
	}

	#if (AA_WITH_TLSF_STAT == 1)
	{
		pEnv->used -= sizeBlock ;
	}
	#endif

	if (size < sizeBlock)
	{
		// Shrink the block
		if ((sizeBlock - size) >= MIN_SIZE)
		{
			removeRemainder (pEnv, pBlock, sizeBlock - size) ;
		}
		#if (AA_WITH_TLSF_STAT == 1)
		{
			pEnv->used += SLICETOSIZE (pBlock->sizeSlice) ;
		}
		#endif
		return (void *) ((uintptr_t) pBlock + sizeof (uint32_t)) ;
	}

	// The block is growing. Try to coalesce with next block
	pNext = getNextBlock (pBlock) ;
	sizeNext = 0 ;
	if (pNext != NULL  &&  (pNext->freeFlag == BIT_FREE))
	{
		sizeNext = SLICETOSIZE (pNext->sizeSlice) ;

		if ((sizeBlock + sizeNext) >= size)
		{
			// New size is OK, coalesce with next block
			getMapping (sizeNext, & fli, & sli) ;
			removeBlock (pEnv, pNext, fli, sli) ;
			pBlock->lastFlag = pNext->lastFlag ;
			pBlock->sizeSlice = 0x7FFFu & ((uint32_t) pBlock->sizeSlice + pNext->sizeSlice) ;
			sizeBlock = SLICETOSIZE (pBlock->sizeSlice) ;
			pNext = getNextBlock (pNext) ;
			if (pNext != NULL)
			{
				pNext->prevOffset = pBlock->sizeSlice ;
			}
			else
			{
				// This is the new last block of the physical list, memorize
				pEnv->pMemLast = pBlock ;
			}

			// Shrink the block if necessary
			if ((sizeBlock - size) >= MIN_SIZE)
			{
				removeRemainder (pEnv, pBlock, sizeBlock - size) ;
			}
			#if (AA_WITH_TLSF_STAT == 1)
			{
				pEnv->used += SLICETOSIZE (pBlock->sizeSlice) ;
			}
			#endif
			return (void *) ((uintptr_t) pBlock + sizeof (uint32_t)) ;
		}
	}

	// Not enough, try prev block
	pPrev = getPrevBlock (pBlock) ;
	sizePrev = 0 ;
	if (pPrev != NULL  && (pPrev->freeFlag == BIT_FREE))
	{
		sizePrev = SLICETOSIZE (pPrev->sizeSlice) ;
	}
	if (size > (sizeBlock + sizePrev)   &&  size > (sizeBlock + sizePrev + sizeNext))
	{
		// Need to allocate a new block
		#if (AA_WITH_TLSF_STAT == 1)
		{
			// Restore previously subtracted size, will be subtracted by tlsfFree()
			pEnv->used += SLICETOSIZE (pBlock->sizeSlice) ;
		}
		#endif
		pNext = (sFreeHeader_t *) tlsfMalloc (hTlsf, size) ;
		if (pNext == NULL)
		{
			return NULL ;	// malloc error
		}
		// Copy data
		copyData ((uint32_t *) ptr, (uint32_t *) pNext+1, (sizeBlock >> 2) - 1) ;

		tlsfFree (pEnv, ptr) ;
		return (void *) pNext ;
	}

	// (sizeBlock + sizePrev) or (sizeBlock + sizePrev + sizeNext) is enough size
	// We need prev block, so coalesce with prev block
	getMapping (sizePrev, & fli, & sli) ;
	removeBlock (pEnv, pPrev, fli, sli) ;
 	pPrev->freeFlag = BIT_BUSY ;
	pPrev->lastFlag = pBlock->lastFlag ;
	pPrev->sizeSlice = 0x7FFFu & ((uint32_t) pPrev->sizeSlice + pBlock->sizeSlice) ;
	if (pNext != NULL)
	{
		pNext->prevOffset = pPrev->sizeSlice ;
	}

	// Copy data
	copyData ((uint32_t *) ptr, (uint32_t *) pPrev + 1, (sizeBlock >> 2) - 1) ;

	pBlock = pPrev ;
	sizeBlock = SLICETOSIZE (pBlock->sizeSlice) ;

	// Does we need next block ?
	if (size > sizeBlock)
	{
		// Coalesce with next block
		getMapping (sizeNext, & fli, & sli) ;
		removeBlock (pEnv, pNext, fli, sli) ;
		pBlock->lastFlag = pNext->lastFlag ;
		pBlock->sizeSlice = 0x7FFFu & ((uint32_t) pBlock->sizeSlice + pNext->sizeSlice) ;
		sizeBlock = SLICETOSIZE (pBlock->sizeSlice) ;
		pNext = getNextBlock (pNext) ;
		if (pNext != NULL)
		{
			pNext->prevOffset = pBlock->sizeSlice ;
		}
	}

	// Shrink the block if necessary
	if ((sizeBlock - size) >= MIN_SIZE)
	{
		removeRemainder (pEnv, pBlock, sizeBlock - size) ;
	}

	pBlock->freeFlag = BIT_BUSY ;

	// If this is the new last block of the physical list, memorize
	if (pBlock->lastFlag == BIT_LAST)
	{
		pEnv->pMemLast = pBlock ;
	}

	#if (AA_WITH_TLSF_STAT == 1)
	{
		pEnv->used -= SLICETOSIZE (pBlock->sizeSlice) ;
	}
	#endif
	return (void *) ((uintptr_t) pBlock + sizeof (uint32_t)) ;
}
#endif //  AA_WITH_REALLOC

//--------------------------------------------------------------------------------
//

void	tlsfGetStat (hTlsf_t hTlsf, uint32_t * pSize, uint32_t * pUsed, uint32_t * pCount)
{
	sEnv_t			* pEnv = (sEnv_t *) hTlsf ;

	* pCount = pEnv->allocCount ;
	#if (AA_WITH_TLSF_STAT == 1)
		* pSize = pEnv->size ;
		* pUsed = pEnv->used ;
	#else
		* pSize = 0 ;
		* pUsed = 0 ;
	#endif
}

//--------------------------------------------------------------------------------
// Check memory pool consistency

#include	"aalogmes.h"

#define	TLSF_PRINT(x)		\
	if (bVerbose != 0)		\
	{						\
		aaPrintf x ;		\
	}

aaError_t 	tlsfCheck (hTlsf_t hTlsf, uint32_t bVerbose)
{
	sEnv_t			* pEnv = (sEnv_t *) hTlsf ;
	sFreeHeader_t	* pBlock, * pNext ;
	uint32_t		ii, jj ;

	// Run through blocks physical link, from the block with higher address to the block with lower address
	pBlock = pEnv->pMemLast ;

	TLSF_PRINT (("\ntlsfCheck: Alloc count: %u\n", pEnv->allocCount)) ;

	while (1)
	{
		TLSF_PRINT (("  %08X %5u f:%u l:%u prev:%5u (%6u)\n", (uintptr_t) pBlock, pBlock->sizeSlice, pBlock->freeFlag, pBlock->lastFlag, pBlock->prevOffset, pBlock->sizeSlice * SLICE)) ;

		// Is this block the last one ?
		if (pBlock->prevOffset == 0)
		{
			break ; // End of list
		}

		// Go to prev block: check if next of pBloc->prev is pBloc
		pNext = (sFreeHeader_t *) (void*) ((char *)pBlock - SLICETOSIZE (pBlock->prevOffset)) ;
		if (pBlock != (sFreeHeader_t *) (void*) ((char *)pNext + SLICETOSIZE (pNext->sizeSlice)))
		{
			// Error
			TLSF_PRINT (("error: bad prev block: %08X %5u f:%u l:%u\n", (uintptr_t) pNext, pNext->sizeSlice, pNext->freeFlag, pNext->lastFlag)) ;
			return AA_EFAIL ;
		}

		// Should not have to adjacent free blocks
		if (pBlock->freeFlag == BIT_FREE   &&  pNext->freeFlag == BIT_FREE)
		{
			// Error
			TLSF_PRINT ((" tlsfCheck free seq block:%08X prev:%08X\n", (uintptr_t) pBlock, (uintptr_t) pNext)) ;
			return AA_EFAIL ;
		}

		pBlock = pNext ;
	}

	// Check every class double link list
	for (ii = 0 ; ii < FLI_COUNT ; ii++)
	{
		for (jj = 0 ; jj < SLI_COUNT ; jj++)
		{
			if (pEnv->table [ii][jj] != 0)
			{
				pBlock = FROMSLICEOFFSET (pEnv->table [ii][jj]) ;
				TLSF_PRINT (("tlsfCheck [%2u][%2u] %08X %5u next %08X ", ii, jj, (uintptr_t) pBlock, pBlock->sizeSlice, (uintptr_t) pBlock->pNext)) ;
				if (pBlock->pPrev != NULL  ||  pBlock->freeFlag != BIT_FREE)
				{
					TLSF_PRINT (("error: pPrev != NULL (%08X) or not free\n", (uintptr_t) pBlock->pPrev)) ;
					return AA_EFAIL ;
				}
				while (1)
				{
					if (pBlock->pNext == NULL)
					{
						break;	// End of list
					}
					if (pBlock != pBlock->pNext->pPrev)
					{
						TLSF_PRINT (("error: pNext (%08X)\n", (uintptr_t) pBlock->pNext)) ;
						return AA_EFAIL ;
					}
					pBlock = pBlock->pNext ;
					TLSF_PRINT (("\ntlsfCheck [%2u][%2u] %08X %5u next %08X ", ii, jj, (uintptr_t) pBlock, pBlock->sizeSlice, (uintptr_t) pBlock->pNext)) ;
				}
				TLSF_PRINT (("\n")) ;
			}
		}
	}

	TLSF_PRINT (("tlsfCheck: OK\n")) ;
	return AA_ENONE ;	// All is OK
}

//--------------------------------------------------------------------------------

#endif		// AA_WITH_TLSF

