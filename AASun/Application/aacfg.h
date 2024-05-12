/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aacfg.h		Kernel wide definitions.

	When		Who	What
	01/05/17	ac	Creation
	12/03/20	ac	Add logMess configuration. Allows to not include this feature.
	21/04/20	ac	Add newlib reent enable: AA_WITH_NEWLIB_REENT
	05/08/20	ac	Add Floating point configuration: AA_FLOAT_T and AA_FLOAT_SEP
	20/09/21	AA_QUEUE_MAX, AA_TIMER_MAX and AA_BUFPOOL_MAX can be 0

	This file contain only definitions and typedef for the kernel
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

#if ! defined AA_CFG_H_
#define AA_CFG_H_
//--------------------------------------------------------------------------------

//	Error numbers returned by kernel and BSP functions

typedef enum
{
	AA_ENONE				= 0u,	// No error
	AA_EFAIL,						// Generic error
	AA_EARG,						// Bad argument value
	AA_ETIMEOUT,					// Timeout
	AA_EDEPLETED,					// Depleted resource
	AA_ESTATE,						// Operation not allowed for this object state
	AA_EWOULDBLOCK,					// Non blocking function would block
	AA_EFLUSH,						// Object flushed
	AA_ENOTALLOWED,					// Not allowed (e.g. from ISR)
	AA_EMEMORY						// Memory allocation error

} aaError_t ;

//--------------------------------------------------------------------------------
//	Kernel configuration

//	To use HAL: define USE_HAL_DRIVER preprocessor symbol in toolchain compiler

//	Count of task priorities managed by the kernel: 4 to 256
//	The lower priority is 0, the highest priority is AA_PRIO_COUNT-1
#define	AA_PRIO_COUNT					8u
#define	AA_PRIO_MAX						(AA_PRIO_COUNT - 1u)

// Choose periodic tick (frequency defined in bsp.h) or tick stretching (power saving)
#define	AA_TICK_STRETCH					0	// 0: periodic tick, 1: tick stretching

//	Max count of task managed by the kernel (min 4)
//	The count should allows: idle task + app tasks + utilities tasks (if any)
#define	AA_TASK_MAX						8u

//	Max count of mutexes managed by the kernel
#define	AA_MUTEX_MAX					4u

//	Max count of semaphores  managed by the kernel
#define	AA_SEM_MAX						6u

// Max count of managed timers
#define	AA_TIMER_MAX					1u

// Max count of managed queues
#define	AA_QUEUE_MAX					0u

// Max count of managed buffer pools
#define	AA_BUFPOOL_MAX					0u


// Count of char for task name (including final \0)
#define	AA_TASK_NAME_SIZE				8u


// aaLogMes task configuration. For M0+ core configure the BSP/loguart.c file
#define	AA_WITH_LOGMES					0		// Generate aaLogMes code
//#define	AA_LOGMES_MAXBUF			85u		// Count of buffers, (1+5)*4 = 24, 85*24=2040 bytes
#define	AA_LOGMES_MAXBUF				21u		// Count of buffers, (1+5)*4 = 24, 21*24=504 bytes

// Idle task configuration
#if (AA_WITH_LOGMES == 1)
	#define	AA_IDLE_STACK_SIZE			128u	// Word count
#else
	#define	AA_IDLE_STACK_SIZE			72u		// Word count
#endif
#define	AA_IDLE_STACK_STATIC			1u		// 1: force static stack allocation
#define	AA_IDLE_FLAGS					AA_FLAG_STACKCHECK


// First user task configuration
#define	AA_INIT_STACK_STATIC			1u			// 1: force static stack allocation
#define	AA_INIT_STACK_SIZE				400u		// Stack word count
#define	AA_INIT_PRIORITY				4u			// Initial task priority. The user can change it later
#define	AA_INIT_FLAGS					AA_FLAG_STACKCHECK	// Task flags
#define	AA_INIT_NAME					"tAASun"	// Task name
#define	AA_INIT_ARG						0u			// Task entry point parameter


// USART configuration for console.
#define	AA_WITH_CONSOLE					1		// Allows UART console initialization by the kernel

#if (AA_WITH_CONSOLE == 1)
	#define	AA_UART_CONSOLE				2u		// UART number. Connected to STLINK
	#define	AA_UART_CONSOLE_BAUDRATE	57600u	// Baud rate
	#include	"uartbasic.h"
	extern		uartHandle_t	aaConsoleHandle ;	// The UART handle of the console
#endif

//--------------------------------------------------------------------------------
// Comment or set to 0 the unwanted features

#define	AA_WITH_ARGCHECK				1		// Allows checking of most functions arguments

#define	AA_WITH_DEBUG					1		// Allows debug tests AA_ASSERT

#define	AA_WITH_TASKSTAT				0		// Compute task CPU usage

#define	AA_WITH_CRITICALSTAT			0		// Compute critical section CPU usage

//	Define an assert specific to list module. Imply AA_WITH_DEBUG = 1
//	For performance reason it may be necessary to not enable theses checks
#define	AA_WITH_LISTCHECK				0		// Some sanity check for lists

//	Define an assert specific to kernel. Imply AA_WITH_DEBUG = 1
//	For performance reason it may be necessary to not enable theses checks
#define	AA_WITH_KERNELASSERT			1		// Some sanity check for the kernel

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//	Dynamic memory allocation configuration
//	AA_WITH_MALLOC_TLSF imply AA_WITH_TLSF
//	AA_WITH_MALLOC_BLOC imply AA_WITH_MEMBLOCK
//	AA_WITH_MALLOC_TLSF and AA_WITH_MALLOC_BLOC are exclusive
//	AA_WITH_REALLOC imply AA_WITH_MALLOC_TLSF

#define	AA_WITH_TLSF					1		// Include TLSF to kernel

#define	AA_WITH_MALLOC_TLSF				1		// Dynamic memory allocation aaMalloc() enabled and use TLSF

#define	AA_WITH_REALLOC					0		// include aaRealloc() to kernel


#define	AA_WITH_MEMBLOCK				0		// Include memory block allocator to kernel

#define	AA_WITH_MALLOC_BLOC				0		// Dynamic memory allocation aaMalloc() enabled and use bloc allocator

#define	AA_WITH_MALLOC_BLOC_ERRONFREE	0		// Fatal error if aaFre() or aaTryFree() is used

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//	Floating point configuration

#define	AA_WITH_FLOAT_PRINT				0		// Allows %f in aaPrintf
#define	AA_FLOAT_T						float	// float or double
#define	AA_FLOAT_SEP					"."		// Decimal dot or comma

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Allow use of Newlib's thread-safe features:
// Basically, a _reent structure is added to each TCB, and the task switch manages _impure_ptr
// Experimental
#define	AA_WITH_NEWLIB_REENT			1

// For Newlib version >=3, enable to allocate the locks mutexes (system/newlib/locks.c)
#define	AA_WITH_NEWLIB_LOCKS			0

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Heap defined by user (default: defined by linker script)
#define	AA_WITH_USERHEAP				0

#if (AA_WITH_USERHEAP == 1)
	#define	AA_HEAP_BEGIN				0x8000000u	// Heap address
	#define	AA_HEAP_SIZE				0x2000u		// Byte count
#endif


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Kernel errors management

#define	AA_WITH_ERRORCHECK_NONE			0		// No error check
#define	AA_WITH_ERRORCHECK_ASSERT		1		// Use debugger stop (e.g. BKPT) at the place of the test
#define	AA_WITH_ERRORHECK_NOTIFY		2		// Call bspErrorNotify() error notify function

#define	AA_WITH_ERRORCHECK				AA_WITH_ERRORCHECK_ASSERT

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Debug trace management

// To globally allow (1) or prohibit (0) traces
#define	AA_WITH_TRACE				0

// To ignore Systick interrupt when AA_WITH_T_INTENTER or AA_WITH_T_INTEXIT is selected
// This has no effect on the timer interrupt when AA_TICK_STRETCH is selected
#define	AA_WITH_TRACE_NOSYSTICK		1

//	To enable  a specific trace: define AA_WITH_T_xx with the value 1
//	To disable a specific trace: comment the line or set its value to 0

#define	AA_WITH_T_TASKREADY			1		// Task added to ready list		task index
#define	AA_WITH_T_TASKCREATED		1		// New task created ok,			New task index
#define	AA_WITH_T_TASKCREATEFAIL	1		// New task create failed,		Caller task index
#define	AA_WITH_T_TASKDELETED		1		// aaTaskDelete,				task index
#define	AA_WITH_T_TASKDELAYED		1		// aaTaskDelay,					task index
#define	AA_WITH_T_TASKRESUMED		1		// aaTaskResume					task index
#define	AA_WITH_T_TASKSUSPENDED		1		// aaTaskDelete,				task index
#define	AA_WITH_T_TASKNEWPRIO		1		// aaTaskSetPriority,			task index, prio
#define	AA_WITH_T_MTXCREATED		1		// aaMutexCreate ok				mutex index
#define	AA_WITH_T_MTXCREATEFAIL		1		// aaMutexCreate fail			task index
#define	AA_WITH_T_MTXDELETED		1		// aaMutexDelete ok				mutex index
#define	AA_WITH_T_MTXDELETEFAIL		1		// aaMutexDelete fail			mutex index
#define	AA_WITH_T_MTXWAIT			1		// aaMutexTake delayed			task index, mutex index
#define	AA_WITH_T_MTXTAKE			1		// aaMutexTake ok				task index, mutex index
#define	AA_WITH_T_MTXTAKEFAIL		1		// aaMutexTake fail				task index, mutex index
#define	AA_WITH_T_MTXTAKEREC		1		// aaMutexTake ok recursive		task index, mutex index
#define	AA_WITH_T_MTXTAKERECFAIL	1		// aaMutexTake fail recursive	task index, mutex index
#define	AA_WITH_T_MTXPRIOCHANGE		1		// aaMutexTake prio inheritance	task index, prio
#define	AA_WITH_T_MTXGIVE			1		// aaMutexGive ok				task index, mutex index
#define	AA_WITH_T_MTXGIVEREC		1		// aaMutexGive recursive ok		task index, mutex index
#define	AA_WITH_T_MTXGIVEFAIL		1		// aaMutexGive fail				task index, mutex index

#define	AA_WITH_T_TSWITCH			1		// Task switch					task index out, task index in	  Inutile ?
#define	AA_WITH_T_TICK				1		// Tick							tick value

#define	AA_WITH_T_SEMCREATED		1		// aaSemCreate ok				sem index
#define	AA_WITH_T_SEMCREATEFAIL		1		// aaSemCreate fail				task index
#define	AA_WITH_T_SEMDELETED		1		// aaSemDelete ok				sem index
#define	AA_WITH_T_SEMDELETEFAIL		1		// aaSemDelete fail				task index
#define	AA_WITH_T_SEMTAKE			1		// aaSemTake sem ok				task index, sem index
#define	AA_WITH_T_SEMTAKEFAIL		1		// aaSemTake sem fail			task index, sem index
#define	AA_WITH_T_SEMGIVE			1		// aaSemGive ok					task index, sem index
#define	AA_WITH_T_SEMGIVEFAIL		1		// aaSemGive fail				task index, sem index
#define	AA_WITH_T_SEMWAIT			1		// aaSemTake sem wait			task index, sem index
#define	AA_WITH_T_SEMFLUSHFAIL		1		// aaSemFlush sem				task index, sem index
#define	AA_WITH_T_SEMRESETFAIL		1		// aaSemFlush sem				task index, sem index

#define	AA_WITH_T_SIGNALSEND		1		// Signal obtained				task index, sig
#define	AA_WITH_T_SIGNALWAIT		1		// Waiting for signal			task index, sig
#define	AA_WITH_T_SIGNALPULSE		1		// Pulse signals				task index, sig

#define	AA_WITH_T_QUEUECREATED		1		// aaQueueCreate ok				task index, queue index
#define	AA_WITH_T_QUEUECREATEFAIL	1		// aaQueueCreate failed			task index
#define	AA_WITH_T_QUEUEDELETED		1		// aaQueueDelete				queue index
#define	AA_WITH_T_QUEUEDELETEFAIL	1		// aaQueueDelete fail			task index, queue index
#define	AA_WITH_T_QUEUEWAIT			1		// Task wait I/O				task index, queue index
#define	AA_WITH_T_QUEUEGIVE			1		// Give message ok				task index, queue index
#define	AA_WITH_T_QUEUEGIVEFAIL		1		// Give message fail			task index, queue index
#define	AA_WITH_T_QUEUEPEEK			1		// Peek message ok				task index, queue index
#define	AA_WITH_T_QUEUEPEEKFAIL		1		// Peek message fail			task index, queue index
#define	AA_WITH_T_QUEUEPURGE		1		// Purge message ok				task index, queue index
#define	AA_WITH_T_QUEUEPURGEFAIL	1		// Purge message ok				task index, queue index
#define	AA_WITH_T_QUEUETAKE			1		// Take message ok				task index, queue index
#define	AA_WITH_T_QUEUETAKEFAIL		1		// Take message fail			task index, queue index
#define	AA_WITH_T_QUEUEGETCOUNTFAIL	1		// Get count of message fail	task index, queue index

#define	AA_WITH_T_TIMERCREATED		1		// Create timer ok				task  index, timer index
#define	AA_WITH_T_TIMERCREATEFAIL	1		// Create timer fail			task  index
#define	AA_WITH_T_TIMERDELETED		1		// Delete timer ok				timer index
#define	AA_WITH_T_TIMERDELETEFAIL	1		// Delete timer ok				timer index
#define	AA_WITH_T_TIMERSET			1		// Timer set ok					timer index
#define	AA_WITH_T_TIMERSETFAIL		1		// Timer set fail				timer index
#define	AA_WITH_T_TIMERSTART		1		// Timer start ok				timer index
#define	AA_WITH_T_TIMERSTARTFAIL	1		// Timer start fail				timer index
#define	AA_WITH_T_TIMERSTOPFAIL		1		// Timer stop fail				timer index
#define	AA_WITH_T_TIMEREXPIRED		1		// Timer call callback			timer index

#define	AA_WITH_T_BPOOLCREATED		1		// Create buffer pool ok		task index, buffer pool index
#define	AA_WITH_T_BPOOLCREATEFAIL	1		// Create buffer pool fail		task index
#define	AA_WITH_T_BPOOLDELETED		1		// Delete buffer pool ok		task index, buffer pool index
#define	AA_WITH_T_BPOOLDELETEFAIL	1		// Delete buffer pool fail		task index
#define	AA_WITH_T_BPOOLTAKE			1		// Take   buffer pool ok		task index, buffer pool index
#define	AA_WITH_T_BPOOLTAKEFAIL		1		// Take   buffer pool fail		task index, buffer pool index
#define	AA_WITH_T_BPOOLGIVE			1		// Give   buffer pool ok		task index, buffer pool index
#define	AA_WITH_T_BPOOLGIVEFAIL		1		// Give   buffer pool fail		task index, buffer pool index
#define	AA_WITH_T_BPOOLGETCOUNTFAIL	1		// Get count of message fail	task index
#define	AA_WITH_T_BPOOLRESETFAIL	1		// Reset of buffer pool fail	task index

#define	AA_WITH_T_IOWAIT			1		// Task waiting I/O				task index

#define	AA_WITH_T_INTENTER			0		// Interrupt enter				irq num
#define	AA_WITH_T_INTEXIT			0		// Interrupt exit				irq num

#define	AA_WITH_T_MESSAGE			1		// Message  					ASCII String,
#define	AA_WITH_T_TASKINFO			1		// Task info

#define	AA_WITH_T_USER1				1		// User1  						1x8 or 2x8
#define	AA_WITH_T_USER2				1		// User2  						1x8 or 2x8

//--------------------------------------------------------------------------------
// Check the consistency of the configuration

#if (AA_PRIO_COUNT < 4  ||  AA_PRIO_COUNT > 256)
	#error AA_PRIO_COUNT must be > 4 and <= 256
#endif

#if (AA_TASK_MAX < 4)
	#error AA_TASK_MAX must be > 3
#endif

#if (AA_WITH_MALLOC_TLSF == 1)  &&  (AA_WITH_MALLOC_BLOC == 1)
	#error AA_WITH_MALLOC_TLSF and AA_WITH_MEMBLOCK are exclusive
#endif

#if (AA_WITH_MALLOC_TLSF == 1) || (AA_WITH_MALLOC_BLOC == 1)
	// AA_WITH_MALLOC is an indicator of dynamic allocation availability
	#define AA_WITH_MALLOC		1
#endif

#if (AA_WITH_MALLOC_BLOC == 1) &&  (AA_WITH_MEMBLOCK != 1)
	#error AA_WITH_MALLOC_BLOC imply AA_WITH_MEMBLOCK
#endif

#if (AA_WITH_MALLOC_TLSF == 1) && (AA_WITH_TLSF != 1)
	#error AA_WITH_MALLOC_TLSF imply AA_WITH_TLSF
#endif

#if (AA_WITH_REALLOC == 1)  &&  (AA_WITH_MALLOC_TLSF != 1)
	#error AA_WITH_REALLOC imply AA_WITH_MALLOC_TLSF
#endif

#if (defined AA_HEAP_BEGIN  && ! defined AA_HEAP_SIZE)  || (defined AA_HEAP_SIZE  && ! defined AA_HEAP_BEGIN)
	#error AA_HEAP_BEGIN and  AA_HEAP_SIZE should both be defined
#endif

#if (AA_WITH_NEWLIB_REENT == 1) && (AA_WITH_MALLOC_TLSF != 1)
	#error AA_WITH_NEWLIB_REENT imply AA_WITH_MALLOC_TLSF
#endif

//--------------------------------------------------------------------------------

// If traces allowed, Define a function to enable/disable traces
#if (AA_WITH_TRACE == 1)
	// Use aaTraceEnable() no enable/disable traces (default is disable)
	extern	void	aaTraceEnable (uint32_t bEnable) ;
#else
	// Ignore aaTraceEnable
	#define	aaTraceEnable(bEnable)
#endif


// AA_WITH_DEBUG implies using AA_ASSERT()
#if (AA_WITH_DEBUG == 1)
	#ifdef __cplusplus
		extern	"C" void bspAssertFailed (uint8_t * pFileName, uint32_t line) ;
	#else
		extern	void bspAssertFailed (uint8_t * pFileName, uint32_t line) ;
	#endif

	#define AA_ASSERT(expr) ((expr) ? (void)0 : bspAssertFailed ((uint8_t *)__FILE__, __LINE__))
#else
	#define AA_ASSERT(expr)
#endif

//	Define an assert specific to kernel.
//	For performance reason it may be necessary to not enable theses checks
#if (AA_WITH_DEBUG == 1  &&  AA_WITH_KERNELASSERT == 1)
	#define		AA_KERNEL_ASSERT(test) 		AA_ASSERT (test)
#else
	#define		AA_KERNEL_ASSERT(test)
#endif



// Some debuggers can't access static variable.
// Enable static only in release configuration (AA_WITH_DEBUG not defined)
#if (AA_WITH_DEBUG == 1)
	#define	STATIC
#else
	#define	STATIC static
#endif

//--------------------------------------------------------------------------------
#endif	// AA_CFG_H_
