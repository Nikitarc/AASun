/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aakernel.h	Kernel definitions. Private to kernel.

	When		Who	What
	25/11/13	ac	Creation

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

#if ! defined AA_KERNEL_H_
#define AA_KERNEL_H_
//--------------------------------------------------------------------------------

#include	"aalist.h"
#include	"aaerror.h"
#include	"bspcfg.h"

#if (AA_WITH_NEWLIB_REENT == 1)
	#include <reent.h>
	#include <string.h>		// because _REENT_INIT_PTR use memset
	#include "newlib_reclaim.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Task Control Block

typedef struct aaMutex_s 		aaMutex_t ;			// Forward declaration
typedef struct aaSem_s   		aaSem_t ;			// Forward declaration
typedef struct aaQueue_s 		aaQueue_t ;			// Forward declaration
typedef struct aaDriverDesc_s	aaDriverDesc_t ;	// I/O driver driver descriptor

struct aaTcb_s
{
	volatile bspStackType_t	* pTopOfStack;		// Points to the top of stack (location of the last item placed on the task stack)
												// MUST be the first item in aaTcb_t (easier asm access)

	char					name [AA_TASK_NAME_SIZE] ;	// Task name

	aaListNode_t			listNode ;			// A list node to put this TCB in a list (not synchro list)
	aaListNode_t			waitNode ;			// A list node to put this TCB in a wait list
	aaListHead_t			mutexList ;			// A list of owned mutexes
	union 
	{
		aaMutex_t			* pMutex ;			// The mutex in the waiting list of which the TCB is
		aaSem_t				* pSem ;			// The semaphore in the waiting list of which the TCB is
		aaQueue_t			* pQueue ;			// The queue in the waiting list of which the TCB is
		aaDriverDesc_t		* pIoList ;			// The driver descriptor in the waiting list of which the TCB is
		aaSignal_t			sigsWakeUp ;		// the signal that has awakened the task. Set by aaSignalSend()/Pulse, read by aaSignalWait()

	} objPtr ;
	uint16_t				taskIndex ;			// Index of the task in aaTcbArray)
	uint8_t					priority ;			// This task current priority (!= basePriority while priority inheritance)
	uint8_t					basePriority ;		// This task priority
	aaTaskState_t			state ;				// This task state

	bspStackType_t			* pStack ;			// Initial stack pointer (low address)
	uint16_t				stackSize ;			// Stack size, count of aaBspStackType_t

	uint16_t				flags ;				// Task user and internal flags
	aaSignal_t				sigsWait ;			// The signals the task is waiting for
	aaSignal_t				sigsRecv ;			// Received signals
	uint32_t				cpuUsage ;			// CPU Usage of this task

#if (AA_WITH_NEWLIB_REENT == 1)
	struct _reent			newlibReent ;		// To put in _impure_ptr
#endif
} ;

// Internal flags for aaTcb_t.flags
#define	AA_FLAG_RECLAIM_REENT_	0x0080			// Newlib's buffers must be freed (aaTaskDelete())
#define	AA_FLAG_DELAYTMO_		0x0100			// Awaken by expired timeout
#define	AA_FLAG_FLUSH_			0x0200			// Not acquired, awaken by flush (sem, queue, ...)
#define	AA_FLAG_SIGNALAND_		0x0400			// Task signal wait for all signals (AND)
#define	AA_FLAG_QUEUEPUT_		0x0800			// Task waiting for aaQueuePut()
#define	AA_FLAG_KERNELSTACK_	0x1000			// Task stack allocated by the kernel (not user flag)
#define	AA_FLAG_STACKTHR_		0x2000			// The stack has reached the threshold
#define	AA_FLAG_STACKOVFL_		0x4000			// The stack has overflowed
#define	AA_FLAG_SUSPEND_		0x8000			// Suspend is requested for this task

#define	AA_FLAG_USERMASK_		0x000F			// Mask to isolate user flags (defined in aabase.h)

//--------------------------------------------------------------------------------
//	Mutex Control Block and Semaphore Control Block

struct aaMutex_s
{
	aaListNode_t	mutexNode ;		// A list node to put this mutex in TCB owned muted list. Mandatory first item in struct
	aaListHead_t	waitingList ;	// Priority sorted list of TCB who want this mutex: head = higher priority TCB.
									// In this list node value is the priority of the waiting thread
	aaTcb_t			* pOwner ;		// Pointer to TCB who currently own the mutex
	aaMutexId_t		mutexIndex ;	// Index of this mutex object in aaMutexArray, used only by kernel trace
	int16_t			count ;			// TODO : Why count is signed ?

} ;

// Flags definition for aaSem_t.flags
#define	AA_SEMTYPE_NONE_	0x01	// For free SCB
#define	AA_SEMTYPE_SEM_		0x02	// For an allocated counting semaphore

struct aaSem_s
{
	aaListHead_t	waitingList ;	// Priority sorted list of TCB who want this sem: head = higher priority TCB.
	aaTcb_t			* pOwner ;		// Pointer to TCB who currently own the sem
	int32_t			count ;			// The semaphore counter
	aaSemId_t		semIndex ;		// Index of this sem object in aaSemArray
	uint8_t			flags ;			// The semaphore flags
	uint8_t			PADDING1 ;

} ;

//--------------------------------------------------------------------------------
// Queue control block

struct aaQueue_s
{
	aaListHead_t	putWaitingList ;	// Put task waiting list. Mandatory first item in the structure (used by free queues list)
	aaListHead_t	getWaitingList ;	// Get task waiting list
	uint8_t			* pBuffer ;			// Message buffer
	uint8_t			* pLast ;			// Pointer past buffer
	uint8_t			* pWrite ;			// Where to write next message
	uint8_t			* pRead ;			// Where  to read next message
	uint16_t		msgSize ;			// Max size of message (byte count)
	uint16_t		msgCount ;			// Max count of messages in buffer, or queue ID when the queue is in free list
	uint16_t		msgUsed ;			// Count of messages in buffer
	uint16_t		flags ;

} ;

//	Queue flags values, kernel internal use
enum
{
	AA_QUEUE_DELETED_		= 1u,	// Queue in free state (and in the free queue list)
	AA_QUEUE_DYNAMICBUFFER_	= 2u,	// Buffer dynamically managed by the queue
	AA_QUEUE_PRIORITY_		= 4u,	// Waiting lists ordered by priority (default FIFO)
	AA_QUEUE_POINTER_		= 8u	// The message is the data pointer
} ;

//--------------------------------------------------------------------------------
//	Handle tags, allows to check objects ID validity
//	One tag value for every kernel object type

#define		AA_HTAG_TMASK		0xF000	// Mask to extract the Tag
#define		AA_HTAG_IMASK		0x0FFF	// Mask to extract the Index
#define		AA_HTAG_TASK		0x1000
#define		AA_HTAG_MUTEX		0x2000
#define		AA_HTAG_SEMA		0x3000
#define		AA_HTAG_QUEUE		0x4000
#define		AA_HTAG_TIMER		0x5000
#define		AA_HTAG_BPOOL		0x6000

//--------------------------------------------------------------------------------

AA_EXTERN	uint8_t			aaIsRunning ;						// 0=stopped, 1=Running
AA_EXTERN	uint8_t			aaInIsrCounter ;					// ISR nesting count, 0= not in ISR
AA_EXTERN	uint8_t			aaInCriticalCounter	;				// Critical section nesting count

AA_EXTERN	aaTcb_t			aaTcbArray [AA_TASK_MAX] ;			// Predefined array of task TCB
AA_EXTERN	aaTcb_t			* pAaCurrentTcb ;					// Pointer to currently running TCB
AA_EXTERN	aaTcb_t			* pAaNextTcb ;						// Next TCB for aaSchedule_()

AA_EXTERN	aaStackNode_t	aaTaskFreeList ;					// List of free TCB
AA_EXTERN	aaListHead_t	aaTaskReadyList [AA_PRIO_COUNT] ;	// One list per priority level
AA_EXTERN	aaListHead_t	aaTaskDelayedList ;					// Tasks waiting for timer timeout
AA_EXTERN	aaListHead_t	aaTaskSuspendedList ;				// Tasks suspended
AA_EXTERN	aaListHead_t	aaTaskDeleteList ;					// Deleted tasks not already freed by idle task

#if (AA_MUTEX_MAX > 0)
AA_EXTERN	aaMutex_t		aaMutexArray [AA_MUTEX_MAX] BSP_ATTR_NOINIT ;	// Predefined array of Mutexes
AA_EXTERN	aaStackNode_t	aaMutexFreeList BSP_ATTR_NOINIT ;	// List of free Mutexes
#endif

#if (AA_SEM_MAX > 0)
AA_EXTERN	aaSem_t			aaSemArray [AA_SEM_MAX] BSP_ATTR_NOINIT ;	// Predefined array of Sem
AA_EXTERN	aaStackNode_t	aaSemFreeList BSP_ATTR_NOINIT ;		// List of free Sem
#endif

AA_EXTERN	uint32_t		aaTickCount ;						// Kernel tick interruption counter
AA_EXTERN	uint32_t		aaCpuUsage ;						// Duration of CPU usage computing for all tasks

#if (AA_WITH_CRITICALSTAT == 1)
	AA_EXTERN	uint32_t	aaCriticalUsage ;					// Max duration of critical section
	AA_EXTERN	uint32_t	aaTsCriticalEnter ;					// Time stamp of aaCriticalEnter
#endif

#if defined	AA_WITH_TASKSTAT
	AA_EXTERN	uint32_t	aaTsTaskSwitch ;					// Time stamp of last task switch
#endif

//--------------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	I/O driver helpers

//	Driver descriptor. Only the kernel know theses variables
struct aaDriverDesc_s
{
	aaListHead_t waitingList ;

} ;

//	Check if the driver descriptor waiting list contains at least one task

__STATIC_INLINE uint32_t	aaIsIoWaitingTask	(aaDriverDesc_t * pDriverDesc)
{
	return (aaListIsEmpty (& pDriverDesc->waitingList) == 1) ? 0u : 1u ;
}

//-----------------------------------------------------------------------------
//	Initialize a driver descriptor structure

__STATIC_INLINE void		aaIoDriverDescInit	(aaDriverDesc_t * pDriverDesc)
{
	aaListInit (& pDriverDesc->waitingList) ;
}

aaError_t	aaIoWait				(aaDriverDesc_t * pDriverDesc, uint32_t bOrdered, uint32_t timeOut) ;
aaTcb_t *	aaIoResume				(aaDriverDesc_t * pDriverDesc) ;
aaTcb_t *	aaIoResumeWaitingTask	(aaDriverDesc_t * pDriverDesc) ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Intra kernel API

// In aakernel.c
aaError_t	aaInit_					(void) ;
void		aaAddReady_				(aaTcb_t * pTcb) ;
void		aaRemoveReady_			(aaTcb_t * pTcb) ;
void		aaListAddOrdered_		(aaListHead_t * pList, aaListNode_t * pNodeAdd) ;
aaError_t	aaRemoveTaskFromLists_ 	(aaTcb_t * pTcb) ;
void		aaSchedule_				(void) ;
void		aaTick_					(void) ;
void		aaRemoveFromDelayedList_(aaListNode_t * pNode) ;
void		aaAddToDelayedList_		(aaTcb_t * pTcb, uint32_t delay) ;
void		aaTaskSwitch_			(void) ;
uint8_t		aaTcbPutInList_			(aaTcb_t * pTcb) ;
void		aaIdleTask_				(uintptr_t arg) ;	// Idle task routine
void		aaStart_				(void) ;
void		aaTaskEnd_				(void) ;				// At end tasks call this function
uint32_t	aaTickToWait_			(void) ;
void		aaUpdateTick_			(uint32_t nTick) ;

__STATIC_INLINE uint32_t	aaIsInIsr	(void)
{
	return (aaInIsrCounter == 0u) ? 0u : 1u ;
}

//	To be called at start of every interrupt handler
//	This function is inlined because it is very short (when no trace and no check)
__STATIC_INLINE void	aaIntEnter		(void)
{
	aaIrqStatus_t	intState ;

	intState = bspSaveAndDisableIrq () ;

	aaTraceInterruptEnter () ;

	aaInIsrCounter ++ ;

	// Check aaInIsrCounter overflow => exception
	AA_ERRORASSERT (aaInIsrCounter != 0, AA_ERROR_KERNEL_2) ;

	bspRestoreIrq (intState) ;
}

void		aaIntExit				(void) ;

//--------------------------------------------------------------------------------

// In aatask.c
aaError_t	aaTaskFreeTaskStack_	(aaTcb_t * pTcb) ;
void		aaTaskFreeZombies_		(void) ;
aaError_t	aaTaskGetTcb_			(aaTaskId_t taskId, aaTcb_t ** ppTcb) ;

//--------------------------------------------------------------------------------

// In aamutex.c
void		aaInitMutex_			(void) ;
uint32_t	aaMutexNewPrio_			(aaTcb_t * pTask) ;
void		aaMutexPropagate_		(aaTcb_t * pTask) ;

// In aasema.c
void		aaInitSemaphore_ 		(void) ;

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_KERNEL_H_
