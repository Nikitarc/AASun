/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aaqueue.c	Message passing IPC

	When		Who	What
	02/07/17	ac	Creation
	28/09/19	ac	Add AA_QUEUE_POINTER
	01/06/20	ac	Allow AA_QUEUE_MAX, AA_BUFPOOL_MAX to be 0

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

#if ! defined AA_QUEUE_H_
#define AA_QUEUE_H_
//--------------------------------------------------------------------------------

typedef	uint16_t	aaQueueId_t ;			// Queue IPC,   id from aaQueueCreate()
typedef	uint16_t	aaBuffPoolId_t ;		// Buffer pool, id from aaBufferPoolCreate()

#ifdef __cplusplus
extern "C" {
#endif

#define		AA_INVALID_QUEUE		((aaQueueId_t) 0xFFFFu)
#define		AA_INVALID_BUFFERPOOL	((aaBuffPoolId_t) 0xFFFFu)

void		aaQueueInit_			(void) ;

aaError_t	aaQueueIsId				(aaQueueId_t queueId) ;
aaError_t	aaQueueCreate			(aaQueueId_t * pQueueId, uint32_t msgSize, uint32_t msgCount, uint8_t * pBuffer, uint32_t flags) ;
aaError_t	aaQueueDelete			(aaQueueId_t queueId) ;
aaError_t	aaQueueGive				(aaQueueId_t queueId, void * pData, uint32_t size, uint32_t timeout) ;
aaError_t	aaQueueTake				(aaQueueId_t queueId, void * pData, uint32_t size, uint32_t timeout) ;
aaError_t	aaQueuePeek				(aaQueueId_t queueId, void ** ppData, uint32_t timeout) ;
aaError_t	aaQueuePurge			(aaQueueId_t queueId) ;
aaError_t	aaQueueGetCount			(aaQueueId_t queueId, uint32_t * pCount) ;

// For aaQueueCreate() flags
#define	AA_QUEUE_FIFO		0x0000		// Task waiting lists managed as FIFO (default)
#define	AA_QUEUE_PRIORITY	0x0001		// Task waiting lists ordered by priority
#define	AA_QUEUE_POINTER	0x0002		// The message is the data pointer


void		aaBufferPoolInit_		(void) ;

aaError_t	aaBufferPoolIsId		(aaBuffPoolId_t bufPoolId) ;
aaError_t	aaBufferPoolCreate		(aaBuffPoolId_t * pBufPoolId, uint32_t bufCount, uint32_t bufSize, void * pBuffer) ;
aaError_t	aaBufferPoolDelete		(aaBuffPoolId_t bufPoolId, uint32_t bForce) ;
aaError_t	aaBufferPoolTake		(aaBuffPoolId_t bufPoolId, void ** ppBuffer) ;
aaError_t	aaBufferPoolGive		(aaBuffPoolId_t bufPoolId, void * pBuffer) ;
aaError_t	aaBufferPoolGetCount	(aaBuffPoolId_t bufPoolId, uint32_t * pCount) ;
aaError_t	aaBufferPoolReset		(aaBuffPoolId_t bufPoolId) ;

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_QUEUE_H_
