/*
----------------------------------------------------------------------

	Alain Chebrou

	rinbuffer.h		Simple ring buffer, thread safe without critical section
					Only 1 writer and 1 reader

	When		Who	What
	09/04/24	ac	Creation

----------------------------------------------------------------------
*/
#if ! defined RINGBUFFER_H_
#define RINGBUFFER_H_

#include	<stdint.h>
#include	<stdbool.h>

typedef struct
{
	uint32_t	size ;		// Mandatory power of 2
	uint32_t	mask ;
	uint32_t	iRead ;
	uint32_t	iWrite ;
	char		* pBuffer ;

} ringBuf_t ;

//----------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void		rbInit				(ringBuf_t * pRing, uint32_t size, void * pBuffer) ;
bool		rbPut				(ringBuf_t * pRing, char data) ;
bool		rbGet				(ringBuf_t * pRing, char * pData) ;
bool		rbPeek				(ringBuf_t * pRing, char * pData) ;
uint32_t	rbReadChunkSize		(ringBuf_t * pRing) ;
uint32_t	rbWriteChunkSize	(ringBuf_t * pRing) ;

// Set the buffer to empty
static	inline void	rbReset (ringBuf_t * pRing)
{
	pRing->iRead  = 0 ;
	pRing->iWrite = 0 ;
}

// Get count of items to read
static	inline uint32_t	rbGetReadCount (ringBuf_t * pRing)
{
	return (pRing->iWrite - pRing->iRead) & pRing->mask ;
}

// Get count of free space to write
static	inline uint32_t	rbGetWriteCount (ringBuf_t * pRing)
{
	return pRing->mask - rbGetReadCount (pRing) ;
}

// Get the address in the buffer of the next item to read
static	inline char	* rbGetReadPtr (ringBuf_t * pRing)
{
	return & pRing->pBuffer [pRing->iRead] ;
}

// Get the address in the buffer of the next item to write
static	inline char	* rbGetWritePtr (ringBuf_t * pRing)
{
	return & pRing->pBuffer [pRing->iWrite] ;
}

// Increement read index
static	inline uint32_t	rbAddRead (ringBuf_t * pRing, uint32_t count)
{
	return pRing->iRead = (pRing->iRead + count) & pRing->mask ;
}

// Increment write index
static	inline uint32_t	rbAddWrite (ringBuf_t * pRing, uint32_t count)
{
	return pRing->iWrite = (pRing->iWrite + count) & pRing->mask ;
}

#ifdef __cplusplus
}
#endif
//----------------------------------------------------------------------
#endif	// RINGBUFFER_H_
