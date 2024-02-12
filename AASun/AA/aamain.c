/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aamain.c	First kernel function called by BSP after boot

	When		Who	What
	30/12/13	ac	Creation
	13/03/20	ac	Conditional initialization of logMess 
	16/05/20	ac	Add preinit/init routines (C++ static initialization)
					Must be done after dynamic memory allocation initialization
	05/07/21		Rename devConsole -> aaConsoleDevice and aaConsoleUartHandle -> aaConsoleHandle
					Add aaGetDefaultConsole()
	12/02/22	ac	Add AA_WITH_NEWLIB_LOCKS support: call init_retarget_locks_()
	09/06/22	ac	add BSP_ATTR_ALIGN(8) to tIdleStack and tInitStack

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
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aamalloc.h"
#include	"aaprintf.h"
#include	"aaqueue.h"
#include	"aatimer.h"

#include	"uartbasic.h"		// For the UART console

#if (AA_WITH_NEWLIB_LOCKS == 1)
	extern	void init_retarget_locks_ (void) ;
#endif

//--------------------------------------------------------------------------------

// If dynamic memory is allowed, and static allocation not required use dynamic memory for idle task stack
// Else use static stack
#if (AA_WITH_MALLOC == 1  &&  AA_IDLE_STACK_STATIC == 0)
	#define tIdleStack	NULL	// Stack is allocated by aaTaskCreate()
#else
	static	bspStackType_t	tIdleStack [AA_IDLE_STACK_SIZE] BSP_ATTR_ALIGN(8) BSP_ATTR_NOINIT ;
#endif		// AA_WITH_MALLOC

// If dynamic memory is allowed, and static allocation not required use dynamic memory for 1st user task stack
#if (AA_WITH_MALLOC == 1  &&  AA_INIT_STACK_STATIC == 0)
	#define tInitStack	NULL	// Stack is allocated by aaTaskCreate()
#else
	static	bspStackType_t	tInitStack [AA_INIT_STACK_SIZE] BSP_ATTR_ALIGN(8) BSP_ATTR_NOINIT ;
#endif		// AA_WITH_MALLOC


#if (AA_WITH_CONSOLE == 1)
	static	aaDevIo_t 	aaConsoleDevice ;
	uartHandle_t		aaConsoleHandle ;		// The UART handle of the console
#endif	// AA_WITH_CONSOLE

// The first task entry point
static	void	aaFirstTask (uintptr_t arg) ;

//--------------------------------------------------------------------------------
// These symbols are provided by the linker.
// C++ initializations.

extern void	(* __preinit_array_start [])	(void) ;
extern void	(* __preinit_array_end   [])	(void) ;
extern void	(* __init_array_start    [])	(void) ;
extern void	(* __init_array_end      [])	(void) ;

//--------------------------------------------------------------------------------

aaDevIo_t *	aaGetDefaultConsole	(void)
{
	#if (AA_WITH_CONSOLE == 1)
	{
		return & aaConsoleDevice ;
	}
	#else
	{
		AA_ASSERT (0) ;
		return NULL ;
	}
	#endif
}

//--------------------------------------------------------------------------------
//	Kernel entry point

void	aaMain_ ()
{
	//----------------------------------------------
	// Initialize basic hardware (console, ...)
	//----------------------------------------------
	bspInit_ () ;

	#if (AA_WITH_CONSOLE == 1)
	{
		// Initialize standard output on USART
		// aaPrintf will send characters to this USART
		// As kernel is not started, no interrupt management for the UART
		uartInit (AA_UART_CONSOLE, AA_UART_CONSOLE_BAUDRATE, UART_FLAG_LFCRLF | UART_FLAG_NOIT, & aaConsoleHandle) ;

		uartInitStdDev (& aaConsoleDevice, aaConsoleHandle) ;
		aaInitStdIn	 (& aaConsoleDevice) ;
		aaInitStdOut (& aaConsoleDevice) ;

		// Configure aaLogMes output, if not in DEBUG mode
		// When DEBUG is defined (by makefile), aaLogMes output is configured by the BSP
		#if (! defined DEBUG)
		{
			// Route aaLogMes() output to console
			aaLogMesSetPutChar (aaConsoleDevice.hDev, aaConsoleDevice.putCharPtr) ;
		}
		#endif
	}
	#endif	// AA_WITH_CONSOLE

	// Initialize kernel
	aaInit_ () ;

	// --- From now we can use mutexes

	// Display banner
	#if (BSP_WITH_BANNER == 1)
 	{
		uint32_t	temp ;

		temp = aaVersion () ;
		aaPrintf ("\nAdAstra - RTK   Version %u.%u\n", temp >> 16u, temp & 0xFFFFu) ;

		bspBanner_ ();
	}
	#endif // BSP_WITH_BANNER

#if (AA_WITH_USER_HEAP == 1)
#define
#endif
 	//----------------------------------------------
	// Initialize more hardware and libraries here
	//----------------------------------------------
	#if (AA_WITH_MALLOC == 1)
	{
		#if (AA_WITH_USER_HEAP == 1)
		{
			// Head defined by user configuration
			if (AA_ENONE != aaMallocInit_ ((uint8_t *) AA_HEAP_BEGIN, (uint8_t *) (AA_HEAP_BEGIN+AA_HEAP_SIZE)), AA_WITH_CONSOLE == 1)
			{
				AA_ERRORNOTIFY (AA_ERROR_MAIN_1) ;
			}
		}
		#else
		{
			// Heap defined by linker data: Mem part aligned on 4
			AA_ERRORASSERT ((((uintptr_t) & _heap_begin_) & 3)  ==  0, AA_ERROR_MAIN_2) ;
			if (AA_ENONE != aaMallocInit_ ((uint8_t *) & _heap_begin_, (uint8_t *) & _heap_end_, AA_WITH_CONSOLE == 1))
			{
				AA_ERRORNOTIFY (AA_ERROR_MAIN_3) ;
			}
		}
		#endif		// AA_WITH_USER_HEAP
	}
	#endif		// AA_WITH_MALLOC

	// --- From now we can call aaMalloc() if AA_WITH_MALLOC is defined

	// Initialize the newlib locks mutexes
	#if (AA_WITH_NEWLIB_LOCKS == 1)
		init_retarget_locks_ () ;
	#endif

	// Iterate over all the preinit/init routines:
	// Call the standard library initialization (mandatory for C++ to
	// execute the constructors for the static objects).
	// Can't be called sooner: use malloc()
	{
		int count ;
		int ii ;

		count = __preinit_array_end - __preinit_array_start ;
		for (ii = 0 ; ii < count ; ii++)
		{
			__preinit_array_start [ii] () ;
		}

		count = __init_array_end - __init_array_start;
		for (ii = 0 ; ii < count ; ii++)
		{
			__init_array_start [ii] () ;
		}
	}

	// Set idle task. There must always be a task at priority 0.
	// This task should never be delayed or suspended (=> can't take synchro objects,...)
	{
		aaTaskId_t	taskId ;
		uint8_t		res ;

		res = aaTaskCreate (0, "tIdle", aaIdleTask_, 0, tIdleStack, AA_IDLE_STACK_SIZE, AA_IDLE_FLAGS, & taskId) ;
		if (res != AA_ENONE)
		{
			AA_ERRORNOTIFY (AA_ERROR_MAIN_4) ;
			while (1)
			{
			}
		}
		 pAaCurrentTcb = & aaTcbArray [taskId & AA_HTAG_IMASK] ;
	}

	// Other kernel initializations
	#if (AA_WITH_LOGMES == 1)
	{
		aaLogMesInit_ () ;
	}
	#endif
	#if (AA_TIMER_MAX > 0)
		aaTimerInit_ () ;
	#endif

	#if (AA_QUEUE_MAX > 0)
		aaQueueInit_ () ;
	#endif

	#if	(AA_BUFPOOL_MAX > 0)
		aaBufferPoolInit_ () ;
	#endif

	// Create the first task, after the idle task is built, before kernel is started
	{
		aaTaskId_t	taskId ;
		uint8_t		res ;

		// This task will run as soon as the kernel is started
		res = aaTaskCreate (AA_INIT_PRIORITY,	// Task priority
							AA_INIT_NAME,		// Task name
							aaFirstTask,		// Entry point,
							AA_INIT_ARG,		// Entry point parameter
							tInitStack,			// Stack pointer
							AA_INIT_STACK_SIZE,	// Stack size
							AA_INIT_FLAGS,		// Flags
							& taskId) ;			// Created task id
		if (res != AA_ENONE)
		{
			AA_ERRORNOTIFY (AA_ERROR_MAIN_5) ;
			while (1)
			{
			}
		}
	}

	// Start the kernel
	aaStart_ () ;	// Never return

	// Must never go here
	AA_ERRORNOTIFY (AA_ERROR_MAIN_6) ;
	while (1)
	{
	}
}

//--------------------------------------------------------------------------------
//	The first kernel task function
//	This task executes the kernel initialization that requires the kernel to be started
//	and then calls the function of the first user task.

static	void	aaFirstTask (uintptr_t arg)
{
	(void) arg ;

	// Continue kernel initialization
	#if (AA_WITH_CONSOLE == 1)
	{
		// Enable console UART interrupt management
		uartSetFlag	(aaConsoleDevice.hDev, UART_FLAG_NOIT, 0) ;
	}
	#endif	// AA_WITH_CONSOLE

	// Call user task function
	userInitTask (arg) ;
}

//--------------------------------------------------------------------------------
//	This function is called by aaTaskDelete() or aaTaskFreeZombies() if the task stack was provided by the application.
//	Should try to release the stack and return AA_ENONE
//	Should not block
//	If the stack cannot be released, should return AA_EFAIL
//	Application allocated stacks are not intended to be allocated on heap

//	Size is count of aaBspStackType_t

//	Don't modify this code. It can be redefined in the application

aaError_t	BSP_ATTR_WEAK aaUserReleaseStack (uint8_t * pStack, uint32_t size)
{
	(void) pStack ;
	(void) size;
	return AA_ENONE ;	// Memory released
}

//--------------------------------------------------------------------------------
//	Handle kernel notifications
//	Don't modify this code. It can be redefined in the application

void	BSP_ATTR_WEAK	aaUserNotify (uint32_t	event, uintptr_t arg)
{
	(void) event ;
	(void) arg ;
}

//--------------------------------------------------------------------------------
