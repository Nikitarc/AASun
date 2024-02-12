/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aabase.h	Kernel API for applications.

	When		Who	What
	25/11/13	ac	Creation
	10/09/19	ac	Add aaTaskCreate() AA_FLAG_SUSPENDED flag management
	21/04/20	ac	Repace AA_EXTERN by extern in forward declaration of aaTaskStateName[]

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

#if ! defined AA_BASE_H_
#define AA_BASE_H_
//--------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------
//	The task states
//	Theses informations are defined here, because applications can use aaTaskInfo_t, that give task state

typedef enum 
{
	aaNoneState				= 0u,		// TCB not in use, or in aaTaskDeleteList
	aaReadyState			= 1u,		// Ready to run, in ready tasks list
	aaDelayedState			= 2u,		// Time delayed, in delayed tasks list
	aaSuspendedState		= 3u,		// Task suspended, in suspended tasks list
	aaMutexWaitingState		= 4u,		// Waiting mutex, in mutex waiting task list and in aaTaskDelayedList
	aaSemWaitingState		= 5u,		// Waiting sem, in sem waiting task list and in aaTaskDelayedList
	aaSignalWaitingState	= 6u,		// Task waiting signal, in aaTaskDelayedList
	aaQueueWaitingState		= 7u,		// Task waiting message queue, in queue task waiting list and in aaTaskDelayedList
	aaIoWaitingState		= 8u,		// Task waiting for I/O

	aaMaxState				= 9u		// This is not a state but the size of aaTaskStateName

} aaTaskState_t ;

// The strings that give the states names 
extern	const char * const aaTaskStateName [aaMaxState] ;

// User flags for aaTaskCreate
#define	AA_FLAG_STACKCHECK		0x0001		// Monitor stack of this task
#define	AA_FLAG_SUSPENDED		0x0002		// Task created in suspended state

//--------------------------------------------------------------------------------
//	Return AA version, with 2 half word numbers: Version - Release,  eg 0x00010005 is 1.5

uint32_t			aaVersion			(void) ;

//--------------------------------------------------------------------------------
// Task management

// Task function prototype
typedef void (* aaTaskFunction_t) (uintptr_t arg) ;

// Task id from aaTaskCreate()
typedef	uint16_t	aaTaskId_t ;

typedef struct aaTaskInfo_s
{
	aaTaskId_t		taskId ;				// Id of this task
	aaTaskState_t	state ;
	uint8_t			priority ;
	uint8_t			basePriority ;
	uint16_t		PADDING1 ;
	uint32_t		cpuUsage ;				// Count of CPU usage
	uint32_t		stackFree ;				// Count of unused words in the task stack

} aaTaskInfo_t ;

aaError_t					aaTaskIsId				(aaTaskId_t taskId) ;
aaError_t					aaTaskCreate			(uint32_t prio, const char * pName, aaTaskFunction_t pEntry, uintptr_t arg, bspStackType_t * pStack, uint32_t stackSize, uint32_t flags, aaTaskId_t * pTaskId) ;
aaError_t					aaTaskDelete			(aaTaskId_t taskId) ;
aaError_t					aaTaskGetBasePriority	(aaTaskId_t taskId, uint8_t * pBasePriority) ;
aaError_t					aaTaskGetRealPriority	(aaTaskId_t taskId, uint8_t * pRealPriority) ;
aaError_t					aaTaskSetPriority		(aaTaskId_t taskId, uint8_t newBasePriority) ;
aaError_t					aaTaskSuspend			(aaTaskId_t taskId) ;
aaError_t					aaTaskResume			(aaTaskId_t taskId) ;
void						aaTaskDelay				(uint32_t delay) ;
aaError_t					aaTaskWakeUp			(aaTaskId_t taskId) ;
aaTaskId_t					aaTaskSelfId			(void) ;
void						aaTaskYield				(void) ;
aaError_t					aaTaskGetName			(aaTaskId_t taskId, const char ** ppName) ;
aaError_t					aaTaskCheckStack		(aaTaskId_t taskId, uint32_t * pFreeSpace) ;
aaError_t					aaTaskInfo				(aaTaskInfo_t * pInfo, uint32_t size, uint32_t * pReturnSize, uint32_t * pCpuTotal, uint32_t * pCriticalUsage, uint32_t flags) ;
void						aaTaskStatClear			(void) ;

#define						AA_SELFTASKID			(0xFFFEu)		// Value to use as taskId for self reference
#define						AA_INFINITE				(0xFFFFFFFFu)	// Value to use for infinite timeout

//--------------------------------------------------------------------------------
//	Tick management

uint32_t					aaGetTickCount			(void) ;

//--------------------------------------------------------------------------------
// Task synchronization API

typedef	uint16_t			aaMutexId_t ;		// Mutex		id from aaMutexCreate()
typedef	uint16_t			aaSemId_t ;			// Semaphore	id from aaSemCreate()
typedef	uint16_t			aaSignal_t ;		// Task signal word

// Useful to initialize uncreated objects
#define	AA_INVALID_TASK		((aaTaskId_t)  0xFFFFu)
#define	AA_INVALID_MUTEX	((aaMutexId_t) 0xFFFFu)
#define	AA_INVALID_SEM		((aaSemId_t)   0xFFFFu)

aaError_t					aaMutexIsId			(aaMutexId_t mutexId) ;
aaError_t					aaMutexCreate		(aaMutexId_t * pMutexId) ;
aaError_t					aaMutexDelete		(aaMutexId_t mutexId) ;
aaError_t					aaMutexTake			(aaMutexId_t mutexId, uint32_t timeOut) ;
aaError_t					aaMutexGive			(aaMutexId_t mutexId) ;
__STATIC_INLINE	aaError_t	aaMutexTryTake		(aaMutexId_t mutexId)
{
	return aaMutexTake (mutexId, 0) ;
}

aaError_t					aaSemIsId			(aaSemId_t semId) ;
aaError_t					aaSemCreate			(int32_t count, aaSemId_t * pSemId) ;
aaError_t					aaSemDelete			(aaSemId_t semId) ;
aaError_t					aaSemTake			(aaSemId_t semId, uint32_t timeOut) ;
aaError_t					aaSemGive			(aaSemId_t semId) ;
aaError_t					aaSemFlush			(aaSemId_t semId) ;
aaError_t					aaSemReset			(aaSemId_t semId, int32_t count) ;
__STATIC_INLINE	aaError_t	aaSemTryTake 		(aaSemId_t semId)
{
	return aaSemTake (semId, 0) ;
}

// Task signals
aaError_t					aaSignalWait		(aaSignal_t sigsIn, aaSignal_t * pSigsOut, uint8_t mode, uint32_t timeOut) ;
aaError_t					aaSignalClear		(aaTaskId_t taskId, aaSignal_t sigs) ;

extern aaError_t			aaSignalSendPulse_	(aaTaskId_t taskId, aaSignal_t sigs, aaSignal_t pulseMask) ; // Kernel use only

__STATIC_INLINE aaError_t	aaSignalSend		(aaTaskId_t taskId, aaSignal_t sigs)
{
	return aaSignalSendPulse_ (taskId, sigs, 0) ;
}

__STATIC_INLINE aaError_t	aaSignalPulse		(aaTaskId_t taskId, aaSignal_t sigs)
{
	return aaSignalSendPulse_ (taskId, sigs, sigs) ;
}

// This task signal is reserved for device driver libraries
#define	AA_SIGNAL_IO		0x8000

// aaSignalWait() mode value
#define	AA_SIGNAL_OR		0			// Wait for one or more signal
#define	AA_SIGNAL_AND		1			// Wait for all signals

#define	AA_SIGNAL_ALL		0xFFFF		// All signals

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	The user application should define theses functions

//	The function of the first user task created by the kernel
void		userInitTask		(uintptr_t arg) ;

//	Callback from kernel when a task is deleted, if the stack is allocated by the application (not by the kernel).
//	pStack is the stack pointer passed to aaTaskCreate().
//	It must return AA_ENONE when the stack memory was released, else AA_EFAIL
aaError_t	aaUserReleaseStack	(uint8_t * pStack, uint32_t size) ;

//	This function allows the kernel to notify the user of some events
//	Event values:
#define	AA_NOTIFY_STACKOVFL		1		// Stack overflow, arg is the task id
#define	AA_NOTIFY_STACKTHR		2		// Stack threshold is crossed, arg is the task id

void		aaUserNotify		(uint32_t	event, uintptr_t arg) ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Kernel traces

// This defines the number associated with each event
enum
{
	AA_T_TASKREADY				= 1,	// Task added to ready list		task index
	AA_T_TASKCREATED			,		// New task created,			task index
	AA_T_TASKCREATEFAIL			,		// New task create failed,		Caller task index
	AA_T_TASKDELETED			,		// aaTaskDelete,				task index
	AA_T_TASKDELAYED			,		// aaTaskDelay,					task index delayed
	AA_T_TASKRESUMED			,		// aaTaskResume					task index
	AA_T_TASKSUSPENDED			,		// aaTaskDelete,				task index
	AA_T_TASKNEWPRIO			,		// aaTaskSetPriority,			task index, prio

	AA_T_MTXCREATED				,		// aaMutexCreate ok				mutex index
	AA_T_MTXCREATEFAIL			,		// aaMutexCreate fail			mutex index
	AA_T_MTXDELETED				,		// aaMutexDelete ok				mutex index
	AA_T_MTXDELETEFAIL			,		// aaMutexDelete fail			mutex index
	AA_T_MTXWAIT				,		// aaMutexTake delayed			task index, mutex index
	AA_T_MTXTAKE				,		// aaMutexTake ok				task index, mutex index
	AA_T_MTXTAKEFAIL			,		// aaMutexTake fail				task index, mutex index
	AA_T_MTXTAKEREC				,		// aaMutexTake ok recursive		task index, mutex index
	AA_T_MTXTAKERECFAIL			,		// aaMutexTake fail recursive	task index, mutex index
	AA_T_MTXPRIOINHERIT			,		// Mutex prio inheritance		task index, new effective prio
	AA_T_MTXGIVE				,		// aaMutexGive ok				task index, mutex index
	AA_T_MTXGIVEREC				,		// aaMutexGive recursive ok		task index, mutex index
	AA_T_MTXGIVEFAIL			,		// aaMutexGive fail				task index, mutex index

	AA_T_TSWITCH				,		// Task switch					task index out, task index in
	AA_T_TICK					,		// Tick							tick value

	AA_T_SEMCREATED				,		// aaSemCreate ok				sem index
	AA_T_SEMCREATEFAIL			,		// aaSemCreate fail				sem index
	AA_T_SEMDELETED				,		// aaSemDelete ok				sem index
	AA_T_SEMDELETEFAIL			,		// aaSemDelete fail				sem index
	AA_T_SEMTAKE				,		// aaSemTake sem ok				task index, sem index
	AA_T_SEMTAKEFAIL 			,		// aaSemTake sem fail			task index, sem index
	AA_T_SEMGIVE				,		// aaSemGive ok					task index, sem index
	AA_T_SEMGIVEFAIL			,		// aaSemGive fail				task index, sem index
	AA_T_SEMWAIT				,		// aaSemTake mutex wait			task index, sem index
	AA_T_SEMFLUSHFAIL			,		// aaSemFlush fail				task index
	AA_T_SEMRESETFAIL			,		// aaSemReset fail				task index

	AA_T_SIGNALSEND				,		// Signal sent					destination task index, sig
	AA_T_SIGNALWAIT				,		// Waiting for signal			task index, sig
	AA_T_SIGNALPULSE			,		// Pulse signal					task index, sig

	AA_T_QUEUECREATED			,		// aaQueueCreate ok				task index, queue index
	AA_T_QUEUECREATEFAIL		,		// aaQueueCreate failed			task index
	AA_T_QUEUEDELETED			,		// aaQueueDelete ok				queue index
	AA_T_QUEUEDELETEFAIL		,		// aaQueueDelete fail			task index, queue index
	AA_T_QUEUEWAIT				,		// Task wait I/O				task index, queue index
	AA_T_QUEUEGIVE				,		// Put  message ok				task index, queue index
	AA_T_QUEUEGIVEFAIL			,		// Put  message fail			task index, queue index
	AA_T_QUEUETAKE				,		// Get  message ok				task index, queue index
	AA_T_QUEUETAKEFAIL			,		// Get  message fail			task index, queue index
	AA_T_QUEUEPEEK				,		// Peek message ok				task index, queue index
	AA_T_QUEUEPEEKFAIL			,		// Peek message fail			task index, queue index
	AA_T_QUEUEPURGE				,		// Purge message ok				task index, queue index
	AA_T_QUEUEPURGEFAIL			,		// Purge message fail			task index, queue index
	AA_T_QUEUEGETCOUNTFAIL		,		// Get count of message fail	task index, queue index

	AA_T_TIMERCREATED			,		// Create timer ok				task  index, timer index
	AA_T_TIMERCREATEFAIL		,		// Create timer fail			task  index
	AA_T_TIMERDELETED			,		// Delete timer ok				timer index
	AA_T_TIMERDELETEFAIL		,		// Delete timer ok				task  index
	AA_T_TIMERSET				,		// Set    timer ok				timer index
	AA_T_TIMERSETFAIL			,		// Set    timer ok				timer index
	AA_T_TIMERSTART				,		// Start  timer ok				timer index
	AA_T_TIMERSTARTFAIL			,		// Start  timer fail			timer index
	AA_T_TIMERSTOPFAIL			,		// Stop   timer fail			timer index
	AA_T_TIMEREXPIRED			,		// Timer  call callback			timer index

	AA_T_BPOOLCREATED			,		// Create buffer pool ok		task index, buffer pool index
	AA_T_BPOOLCREATEFAIL		,		// Create buffer pool fail		task index
	AA_T_BPOOLDELETED			,		// Delete buffer pool ok		task index, buffer pool index
	AA_T_BPOOLDELETEFAIL		,		// Delete buffer pool fail		task index
	AA_T_BPOOLTAKE				,		// Take buffer pool ok			task index, buffer pool index
	AA_T_BPOOLTAKEFAIL			,		// Take buffer pool fail		task index, buffer pool index
	AA_T_BPOOLGIVE				,		// Give buffer pool ok			task index, buffer pool index
	AA_T_BPOOLGIVEFAIL			,		// Give buffer pool fail		task index, buffer pool index
	AA_T_BPOOLGETCOUNTFAIL		,		// Get count of message fail	task index
	AA_T_BPOOLRESETFAIL			,		// Reset of buffer pool fail	task index

	AA_T_IOWAIT					,		// task waiting I/O				task index

	AA_T_INTENTER				,		// Interrupt enter				irq num
	AA_T_INTEXIT				,		// Interrupt exit				irq num

	AA_T_MESSAGE				,		// Message						string
	AA_T_TASKINFO				,		// Task info: id, priority, name

	AA_T_USER1_1x8				,		// User codes
	AA_T_USER1_2x8				,		//
	AA_T_USER1_1x8_1x16			,		//
	AA_T_USER1_1x32				,		//

	AA_T_USER2_1x8				,		//
	AA_T_USER2_2x8				,		//
	AA_T_USER2_1x8_1x16			,		//
	AA_T_USER2_1x32						//
} ;

//--------------------------------------------------------------------------------

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKINFO == 1
	#define	aaTraceTaskInfo(taskId)	aaLogTrace_TaskInfo (taskId)
#else
	#define	aaTraceTaskInfo(taskId)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MESSAGE == 1
	#define	aaTraceMessage(str)	aaLogTrace_Message (str)
#else
	#define	aaTraceMessage(str)
#endif


#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKREADY == 1
	#define	aaTraceTaskReady(taskIndex)	aaLogTrace_1x8 (AA_T_TASKREADY, taskIndex)
#else
	#define	aaTraceTaskReady(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKCREATED == 1
	#define	aaTraceTaskCreated(taskIndex)	aaLogTrace_1x8 (AA_T_TASKCREATED, taskIndex)
#else
	#define	aaTraceTaskCreated(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKCREATEFAIL == 1
	#define	aaTraceTaskCreateFail(taskIndex)	aaLogTrace_1x8 (AA_T_TASKCREATEFAIL, taskIndex)
#else
	#define	aaTraceTaskCreateFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKDELETED == 1
	#define	aaTraceTaskDeleted(taskIndex)	aaLogTrace_1x8 (AA_T_TASKDELETED, taskIndex)
#else
	#define	aaTraceTaskDeleted(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKDELAYED == 1
	#define	aaTraceTaskDeleyed(taskIndex)	aaLogTrace_1x8 (AA_T_TASKDELAYED, taskIndex)
#else
	#define	aaTraceTaskDeleyed(taskIndex)
#endif

// TODO : useless, always go to ready state with trace
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKRESUMED == 1
	#define	aaTraceTaskResumed(taskIndex)	aaLogTrace_1x8 (AA_T_TASKRESUMED, taskIndex)
#else
	#define	aaTraceTaskResumed(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKSUSPENDED == 1
	#define	aaTraceTaskSuspended(taskIndex)	aaLogTrace_1x8 (AA_T_TASKSUSPENDED, taskIndex)
#else
	#define	aaTraceTaskSuspended(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TASKNEWPRIO == 1
	#define	aaTraceTaskNewPrio(taskIndex, prio)	aaLogTrace_2x8 (AA_T_TASKNEWPRIO, taskIndex, prio)
#else
	#define	aaTraceTaskNewPrio(taskIndex, prio)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXCREATED == 1
	#define	aaTraceMutexCreated(mutexIndex)	aaLogTrace_1x8 (AA_T_MTXCREATED, mutexIndex)
#else
	#define	aaTraceMutexCreated(mutexIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXCREATEFAIL == 1
	#define	aaTraceMutexCreateFail(mutexIndex)	aaLogTrace_1x8 (AA_T_MTXCREATEFAIL, mutexIndex)
#else
	#define	aaTraceMutexCreateFail(mutexIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXDELETED == 1
	#define	aaTraceMutexDeleted(mutexIndex)	aaLogTrace_1x8 (AA_T_MTXDELETED, mutexIndex)
#else
	#define	aaTraceMutexDeleted(mutexIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXDELETEFAIL == 1
	#define	aaTraceMutexDeleteFail(mutexIndex)	aaLogTrace_1x8 (AA_T_MTXDELETEFAIL, mutexIndex)
#else
	#define	aaTraceMutexDeleteFail(mutexIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXWAIT == 1
	#define	aaTraceMutexWait(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXWAIT, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexWait(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXTAKE == 1
	#define	aaTraceMutexTake(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXTAKE, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexTake(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXTAKEFAIL == 1
	#define	aaTraceMutexTakeFail(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXTAKEFAIL, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexTakeFail(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXTAKEREC == 1
	#define	aaTraceMutexTakeRec(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXTAKEREC, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexTakeRec(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXTAKERECFAIL == 1
	#define	aaTraceMutexTakeRecFail(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXTAKERECFAIL, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexTakeRecFail(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXPRIOCHANGE == 1
	#define	aaTraceTaskInheritPriority(taskIndex, prio)	aaLogTrace_2x8 (AA_T_MTXPRIOINHERIT, taskIndex, prio)
#else
	#define	aaTraceTaskInheritPriority(taskIndex, prio)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXGIVE == 1
	#define	aaTraceMutexGive(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXGIVE, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexGive(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXGIVEREC == 1
	#define	aaTraceMutexGiveRec(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXGIVEREC, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexGiveRec(mutexIndex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_MTXGIVEFAIL == 1
	#define	aaTraceMutexGiveFail(mutexIndex, taskIndex)	aaLogTrace_2x8 (AA_T_MTXGIVEFAIL, mutexIndex, taskIndex)
#else
	#define	aaTraceMutexGiveFail(mutexIndex, taskIndex)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TSWITCH == 1
	#define	aaTraceTaskSwitch(oldTaskIndex, nextTaskIndex)	aaLogTrace_2x8 (AA_T_TSWITCH, oldTaskIndex, nextTaskIndex)
#else
	#define	aaTraceTaskSwitch(oldTaskIndex, nextTaskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TICK == 1
	#define	aaTraceTick(tickCount)	aaLogTrace_1x32 (AA_T_TICK, tickCount)
#else
	#define	aaTraceTick(tickCount)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMCREATED == 1
	#define	aaTraceSemCreated(semindex)	aaLogTrace_1x8 (AA_T_SEMCREATED, semindex)
#else
	#define	aaTraceSemCreated(semId)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMCREATEFAIL == 1
	#define	aaTraceSemCreateFail(semindex)	aaLogTrace_1x8 (AA_T_SEMCREATEFAIL, semindex)
#else
	#define	aaTraceSemCreateFail(semId)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMDELETED == 1
	#define	aaTraceSemDeleted(semindex)	aaLogTrace_1x8 (AA_T_SEMDELETED, semindex)
#else
	#define	aaTraceSemDeleted(semId)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMDELETEFAIL == 1
	#define	aaTraceSemDeleteFail(taskIndex)	aaLogTrace_1x8 (AA_T_SEMDELETEFAIL, taskIndex)
#else
	#define	aaTraceSemDeleteFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMTAKE == 1
	#define	aaTraceSemTake(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMTAKE, semindex, taskIndex)
#else
	#define	aaTraceSemTake(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMTAKEFAIL == 1
	#define	aaTraceSemTakeFail(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMTAKEFAIL, semindex, taskIndex)
#else
	#define	aaTraceSemTakeFail(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMGIVE == 1
	#define	aaTraceSemGive(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMGIVE, semindex, taskIndex)
#else
	#define	aaTraceSemGive(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMGIVEFAIL == 1
	#define	aaTraceSemGiveFail(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMGIVEFAIL, semindex, taskIndex)
#else
	#define	aaTraceSemGiveFail(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMWAIT == 1
	#define	aaTraceSemWait(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMGIVE, semindex, taskIndex)
#else
	#define	aaTraceSemWait(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMFLUSHFAIL == 1
	#define	aaTraceSemFlushFail(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMFLUSHFAIL, semindex, taskIndex)
#else
	#define	aaTraceSemFlushFail(semindex, taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SEMRESETFAIL == 1
	#define	aaTraceSemResetFail(semindex, taskIndex)	aaLogTrace_2x8 (AA_T_SEMRESETFAIL, semindex, taskIndex)
#else
	#define	aaTraceSemResetFail(semindex, taskIndex)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SIGNALSEND == 1
	#define	aaTraceSignalSend(taskIndex, signal)	aaLogTrace_1x8_1x16 (AA_T_SIGNALSEND, taskIndex, signal)
#else
	#define	aaTraceSignalSend(taskIndex, signal)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SIGNALWAIT == 1
	#define	aaTraceSignalWait(taskIndex, signal)	aaLogTrace_1x8_1x16 (AA_T_SIGNALWAIT, taskIndex, signal)
#else
	#define	aaTraceSignalWait(taskIndex, signal)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_SIGNALPULSE == 1
	#define	aaTraceSignalPulse(taskIndex, signal)	aaLogTrace_1x8_1x16 (AA_T_SIGNALPULSE, taskIndex, signal)
#else
	#define	aaTraceSignalPulse(taskIndex, signal)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUECREATED == 1
	#define	aaTraceQueueCreated(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUECREATED, taskIndex, queueIndex)
#else
	#define	aaTraceQueueCreated(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUECREATEFAIL == 1
	#define	aaTraceQueueCreateFail(taskIndex)	aaLogTrace_1x8 (AA_T_QUEUECREATED, taskIndex)
#else
	#define	aaTraceQueueCreateFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEDELETED == 1
	#define	aaTraceQueueDeleted(queueIndex)	aaLogTrace_1x8 (AA_T_QUEUEDELETED, queueIndex)
#else
	#define	aaTraceQueueDeleted(queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEDELETEFAIL == 1
	#define	aaTraceQueueDeleteFail(taskIndex)	aaLogTrace_1x8 (AA_T_QUEUEDELETEFAIL, taskIndex)
#else
	#define	aaTraceQueueDeleteFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEWAIT == 1
	#define	aaTraceQueueWait(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEWAIT, taskIndex, queueIndex)
#else
	#define	aaTraceQueueWait(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEGIVE == 1
	#define	aaTraceQueueGive(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEGIVE, taskIndex, queueIndex)
#else
	#define	aaTraceQueueGive(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEGIVEFAIL == 1
	#define	aaTraceQueueGiveFail(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEGIVEFAIL, taskIndex, queueIndex)
#else
	#define	aaTraceQueueGiveFail(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEPEEK == 1
	#define	aaTraceQueuePeek(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEPEEK, taskIndex, queueIndex)
#else
	#define	aaTraceQueuePeek(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEPEEKFAIL == 1
	#define	aaTraceQueuePeekFail(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEPEEKFAIL, taskIndex, queueIndex)
#else
	#define	aaTraceQueuePeekFail(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEPURGE == 1
	#define	aaTraceQueuePurge(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEPURGE, taskIndex, queueIndex)
#else
	#define	aaTraceQueuePurge(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEPURGEFAIL == 1
	#define	aaTraceQueuePurgeFail(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEPURGEFAIL, taskIndex, queueIndex)
#else
	#define	aaTraceQueuePurgeFail(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUEGETCOUNTFAIL == 1
	#define	aaTraceQueueGetCountFail(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUEGETCOUNTFAIL, taskIndex, queueIndex)
#else
	#define	aaTraceQueueGetCountFail(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUETAKE == 1
	#define	aaTraceQueueTake(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUETAKE, taskIndex, queueIndex)
#else
	#define	aaTraceQueueTake(taskIndex, queueIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_QUEUETAKEFAIL == 1
	#define	aaTraceQueueTakeFail(taskIndex, queueIndex)	aaLogTrace_2x8 (AA_T_QUEUETAKEFAIL, taskIndex, queueIndex)
#else
	#define	aaTraceQueueTakeFail(taskIndex, queueIndex)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERCREATED == 1
	#define	aaTraceTimerCreated(taskIndex, timerIndex)	aaLogTrace_2x8 (AA_T_TIMERCREATED, taskIndex, timerIndex)
#else
	#define	aaTraceTimerCreated(taskIndex, timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERCREATEFAIL == 1
	#define	aaTraceTimerCreateFail(taskIndex)	aaLogTrace_1x8 (AA_T_TIMERCREATEFAIL, taskIndex)
#else
	#define	aaTraceTimerCreateFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERDELETED == 1
	#define	aaTraceTimerDeleted(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERDELETED, timerIndex)
#else
	#define	aaTraceTimerDeleted(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERDELETEFAIL == 1
	#define	aaTraceTimerDeleteFail(taskIndex)	aaLogTrace_1x8 (AA_T_TIMERDELETEFAIL, taskIndex)
#else
	#define	aaTraceTimerDeleteFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERSET == 1
	#define	aaTraceTimerSet(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERSET, timerIndex)
#else
	#define	aaTraceTimerSet(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERSETFAIL == 1
	#define	aaTraceTimerSetFail(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERSETFAIL, timerIndex)
#else
	#define	aaTraceTimerSetFail(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERSTART == 1
	#define	aaTraceTimerStart(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERSTART, timerIndex)
#else
	#define	aaTraceTimerStart(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERSTARTFAIL == 1
	#define	aaTraceTimerStartFail(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERSTARTFAIL, timerIndex)
#else
	#define	aaTraceTimerStartFail(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMERSTOPFAIL == 1
	#define	aaTraceTimerStopFail(timerIndex)	aaLogTrace_1x8 (AA_T_TIMERSTOPFAIL, timerIndex)
#else
	#define	aaTraceTimerStopFail(timerIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_TIMEREXPIRED == 1
	#define	aaTraceTimerExpired(timerIndex)	aaLogTrace_1x8 (AA_T_TIMEREXPIRED, timerIndex)
#else
	#define	aaTraceTimerExpired(timerIndex)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLCREATED == 1
	#define	aaTraceBuffPoolCreated(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLCREATED, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolCreated(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLCREATEFAIL == 1
	#define	aaTraceBuffPoolCreateFail(taskIndex)	aaLogTrace_1x8 (AA_T_BPOOLCREATED, taskIndex)
#else
	#define	aaTraceBuffPoolCreateFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLDELETED == 1
	#define	aaTraceBuffPoolDeleted(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLDELETED, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolDeleted(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLDELETEFAIL == 1
	#define	aaTraceBuffPoolDeleteFail(taskIndex)	aaLogTrace_1x8 (AA_T_BPOOLDELETEFAIL, taskIndex)
#else
	#define	aaTraceBuffPoolDeleteFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLTAKE == 1
	#define	aaTraceBuffPoolTake(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLTAKE, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolTake(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLTAKEFAIL == 1
	#define	aaTraceBuffPoolTakeFail(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLTAKEFAIL, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolTakeFail(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLGIVE == 1
	#define	aaTraceBuffPoolGive(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLGIVE, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolGive(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLGIVEFAIL == 1
	#define	aaTraceBuffPoolGiveFail(taskIndex, bufPoolIndex)	aaLogTrace_2x8 (AA_T_BPOOLGIVEFAIL, taskIndex, bufPoolIndex)
#else
	#define	aaTraceBuffPoolGiveFail(taskIndex, bufPoolIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLGETCOUNTFAIL == 1
	#define	aaTraceBuffPoolGetCountFail(taskIndex)	aaLogTrace_1x8 (AA_T_BPOOLGETCOUNTFAIL, taskIndex)
#else
	#define	aaTraceBuffPoolGetCountFail(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_BPOOLRESETFAIL == 1
	#define	aaTraceBuffPoolResetFail(taskIndex)	aaLogTrace_1x8 (AA_T_BPOOLRESETFAIL, taskIndex)
#else
	#define	aaTraceBuffPoolResetFail(taskIndex)
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_IOWAIT == 1
	#define	aaTraceIoWait(taskIndex)	aaLogTrace_1x8 (AA_T_IOWAIT, taskIndex)
#else
	#define	aaTraceIoWait(taskIndex)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_INTENTER == 1
	#define	aaTraceInterruptEnter()	aaLogTraceInterrupt (1)
#else
	#define	aaTraceInterruptEnter()
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_INTEXIT == 1
	#define	aaTraceInterruptExit()	aaLogTraceInterrupt (0)
#else
	#define	aaTraceInterruptExit()
#endif

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#if AA_WITH_TRACE == 1  &&  AA_WITH_T_USER1 == 1
	#define	aaTraceUser1_1x8(arg)				aaLogTrace_1x8      (AA_T_USER1_1x8, arg)
	#define	aaTraceUser1_2x8(arg1, arg2)		aaLogTrace_2x8      (AA_T_USER1_2x8, arg1, arg2)
	#define	aaTraceUser1_1x8_1x16(arg1, arg2)	aaLogTrace_1x8_1x16 (AA_T_USER1_1x8_1x16, arg1, arg2)
	#define	aaTraceUser1_1x32(arg)				aaLogTrace_1x32     (AA_T_USER1_1x32, arg)
#else
	#define	aaTraceUser1_1x8(arg)
	#define	aaTraceUser1_2x8(arg1, arg2)
	#define	aaTraceUser1_1x8_1x16(arg1, arg2)
	#define	aaTraceUser1_1x32(arg)
#endif

#if AA_WITH_TRACE == 1  &&  AA_WITH_T_USER2 == 1
	#define	aaTraceUser2_1x8(arg)				aaLogTrace_1x8      (AA_T_USER2_1x8, arg)
	#define	aaTraceUser2_2x8(arg1, arg2)		aaLogTrace_2x8      (AA_T_USER2_2x8, arg1, arg2)
	#define	aaTraceUser2_1x8_1x16(arg1, arg2)	aaLogTrace_1x8_1x16 (AA_T_USER2_1x8_1x16, arg1, arg2)
	#define	aaTraceUser2_1x32(arg)				aaLogTrace_1x32     (AA_T_USER2_1x32, arg)
#else
	#define	aaTraceUser2_1x8(arg)
	#define	aaTraceUser2_2x8(arg1, arg2)
	#define	aaTraceUser2_1x8_1x16(arg1, arg2)
	#define	aaTraceUser2_1x32(arg)
#endif



// Traces functions
void	aaLogTrace_1x8			(uint32_t code, uint8_t arg) ;
void	aaLogTrace_2x8			(uint32_t code, uint8_t arg1, uint8_t arg2) ;
void	aaLogTrace_1x8_1x16		(uint32_t code, uint8_t arg1, uint16_t arg2) ;
void	aaLogTrace_1x32			(uint32_t code, uint32_t arg) ;
void	aaLogTraceInterrupt		(uint32_t bEnter) ;
void	aaLogTrace_Message		(char * str) ;
void	aaLogTrace_TaskInfo 	(aaTaskId_t taskId) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// AA_BASE_H_
