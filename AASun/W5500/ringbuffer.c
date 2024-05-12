/*
----------------------------------------------------------------------

	Alain Chebrou

	rinbuffer.h		Simple ring buffer, thread safe without critical section
					Only 1 writer and 1 reader

	When		Who	What
	09/04/24	ac	Creation


	The ring buffer doesn't maintain a count of items
	because we don't want to use exclusive access to write to a variable common to write and read

	To differentiate the full state from the empty state, the full state corresponds to size - 1 items.
	Otherwise in both states iRead would equal iWrite.

----------------------------------------------------------------------
*/

#include <ringbuffer.h>
#include	"aa.h"
#include	<stdbool.h>


//----------------------------------------------------------------------

void	rbInit (ringBuf_t * pRing, uint32_t size, void * pBuffer)
{
	if (size && !(size & (size - 1u)))
	{
		pRing->size    = size ;
		pRing->mask    = size - 1 ;
		pRing->iRead   = 0 ;
		pRing->iWrite  = 0 ;
		pRing->pBuffer = pBuffer ;
	}
	else
	{
		AA_ASSERT (0) ;
	}
}

//----------------------------------------------------------------------
// Add one item to the buffer

bool	rbPut (ringBuf_t * pRing, char data)
{
	uint32_t	 count = (pRing->iWrite - pRing->iRead) & pRing->mask ;

	if (count != pRing->mask)
	{
		pRing->pBuffer [pRing->iWrite] = data ;
		pRing->iWrite = (pRing->iWrite + 1u) & pRing->mask ;
		return true ;
	}
	return false ;
}

//----------------------------------------------------------------------
// Get one item from the buffer

bool	rbGet (ringBuf_t * pRing, char * pData)
{
	uint32_t	 count = (pRing->iWrite - pRing->iRead) & pRing->mask ;

	if (count > 0)
	{
		* pData = pRing->pBuffer [pRing->iRead] ;
		pRing->iRead = (pRing->iRead + 1u) & pRing->mask ;
		return true ;
	}
	return false ;
}

//----------------------------------------------------------------------
// Get one item from the buffer, without removing the item from the buffer

bool	rbPeek (ringBuf_t * pRing, char * pData)
{
	uint32_t	 count = (pRing->iWrite - pRing->iRead) & pRing->mask ;

	if (count > 0)
	{
		* pData = pRing->pBuffer [pRing->iRead] ;
		return true ;
	}
	return false ;
}

//----------------------------------------------------------------------
// Return the count of items to read without wrap at the end of the buffer
// To use for DMA or socket send, in coordination with rbGetReadPtr() and rbAddRead()

uint32_t	rbReadChunkSize (ringBuf_t * pRing)
{
	uint32_t	 count = rbGetReadCount (pRing) ;

	if ((pRing->iRead + count) <= pRing->size)
	{
		return count ;
	}
	return (pRing->size - pRing->iRead) ;
}

//----------------------------------------------------------------------
// Return the count of items to write without wrap at the end of the buffer
// To use for DMA or socket recv, in coordination with rbGetWritePtr() and rbAddWrite()

uint32_t	rbWriteChunkSize (ringBuf_t * pRing)
{
	uint32_t	 count = rbGetWriteCount (pRing) ;

	if ((pRing->iWrite + count) <= pRing->size)
	{
		return count ;
	}
	return (pRing->size - pRing->iWrite) ;
}

//----------------------------------------------------------------------
