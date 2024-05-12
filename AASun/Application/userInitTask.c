 /*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	userInitTask.c	The first task

	When		Who	What
	25/11/13	ac	Creation
	27/01/22	ac	Add aaSpuriousInterruptHandler() example
					Add HardFault_Handler_C()
	09/06/22	ac	add BSP_ATTR_ALIGN(8) to ledStack
	15/09/23	ac	Remove ledTask which is useless for AASun

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aaprintf.h"

void		AASun			(void) ;

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	The first user task

void	wifiTest (void) ;

void	userInitTask (uintptr_t arg)
{
	(void) arg ;	// Unused arg

//	wifiTest () ;
	AASun () ;		// Never return
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Manage kernel notifications

void	aaUserNotify (uint32_t	event, uintptr_t arg)
{
	switch (event)
	{
		case AA_NOTIFY_STACKOVFL:	// Stack overflow
			AA_ASSERT (0) ;
			break ;

		case AA_NOTIFY_STACKTHR:	// Stack threshold level is reached
			AA_ASSERT (0) ;
			break ;

		default:
			break ;
	}
	(void) arg ;
}

//--------------------------------------------------------------------------------
//	Display spurious interrupt info on the console
//	The processor jumps here if an unexpected interrupt occurs
//	or a specific handler is not present in the application code

#include "bspcfg.h"			// For bspDisableIrq()
#include "uartbasic.h"

void __attribute__ ((section(".isr_handlers"))) aaSpuriousInterruptHandler (uint32_t exception)
{
	aaDevIo_t	* hDevice ;

	bspDisableIrqAll () ;

	hDevice = aaGetDefaultConsole () ;
	if (hDevice != NULL)
	{
		// Disable UART interrupt management
		uartSetFlag (hDevice->hDev, UART_FLAG_NOIT, 1u) ;
		// Display the real interrupt number
		// If exception is >= 16 this is not a system exception,
		// so subtract 16 to get the peripheral exception number in the isrVectors_ array.
		aaPrintf ("Spurious Interrupt: %u-16=%u\n", exception, exception-16u) ;
	}

	BSP_DEBUG_BKPT() ;
}

//--------------------------------------------------------------------------------
//	Display information about hard fault

//	Uncomment this function to replace the default HardFault handler.

typedef struct
{
	uint32_t	r0 ;
	uint32_t	r1 ;
	uint32_t	r2 ;
	uint32_t	r3 ;
	uint32_t	r12 ;
	uint32_t	lr ;
	uint32_t	pc ;
	uint32_t	psr ;
} exceptionStackFrame ;

void __attribute__ ((used)) HardFault_Handler_C (exceptionStackFrame * frame, uint32_t lr)
{
	aaDevIo_t	* hDevice ;

	bspDisableIrqAll () ;

	hDevice = aaGetDefaultConsole () ;
	if (hDevice != NULL)
	{
		// Disable UART interrupt management
		uartSetFlag (hDevice->hDev, UART_FLAG_NOIT, 1u) ;

		aaPrintf ("HardFault:\n") ;
		aaPrintf ("R0     %08X\n", frame->r0) ;
		aaPrintf ("R1     %08X\n", frame->r1) ;
		aaPrintf ("R2     %08X\n", frame->r2) ;
		aaPrintf ("R3     %08X\n", frame->r3) ;
		aaPrintf ("R12    %08X\n", frame->r12) ;
		aaPrintf ("LR     %08X\n", frame->lr) ;
		aaPrintf ("PC     %08X\n", frame->pc) ;
		aaPrintf ("PSR    %08X\n", frame->psr) ;
		aaPrintf ("LR/EXC_RETURN %08X\n", lr) ;
	}

	// Here you can generate an external signal, like turning on a LED
	while (1)
	{
	}
}

//--------------------------------------------------------------------------------
