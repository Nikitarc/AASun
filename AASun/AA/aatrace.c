/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aatrace.c	Kernel trace functions
				Use SWO to send binary messages

	When		Who	What
	08/05/17	ac	Creation
	01/07/18	ac	Traces use SWO instead of aaLogMes()

	Some parts are specific to the Cortex-M architecture: SWO, SCB->ICSR

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

// This file should define the functions:
//		void	aaTraceEnable (uint32_t bEnable) ;
// 		The functions used by macro traces, they must be declared in aacfg.h

// This trace use SWO, so only available with debugger

// TODO	: Win32 port

#include	"aa.h"
#include	"aakernel.h"

//--------------------------------------------------------------------------------
//	Kernel and user trace management

#if (AA_WITH_TRACE == 1)

#if (__CORTEX_M == 0)
#error ITM Trace not supported
// TODO: No itm on M0 code => use UART
#endif

#include	"itm.h"			// For swoIsEnabled()

//--------------------------------------------------------------------------------
//	Send traces to SWO
//	SWO is available only in privileged mode
//	Assume ITM already initialized by BSP

#if ! defined (DEBUG)
	#warning SWO trace not available
#endif

#define	TRACE_PORT			1		// SWO stimulus port number

static	uint32_t	traceEnabled = 0 ;

static	void	aaLogTrace_TaskInfo_ (aaTcb_t * pTcb) ;

//--------------------------------------------------------------------------------
// Function to enable/disable traces

void	aaTraceEnable (uint32_t bEnable)
{
	uint32_t	ii ;
	aaTcb_t		* pTcb ;

	if (bEnable == 0)
	{
		// Disable traces
		traceEnabled = 0 ;
	}
	else
	{
		// Configure and enable traces

		// Check if ITM/SWO is enabled
		if (swoIsEnabled () != 0)
		{
			// Enable stimulus port
			ITM->LAR	= 0xC5ACCE55 ;			// ITM Lock Access Register, C5ACCE55 enables more write access to Control Register 0xE00 :: 0xFFC
			ITM->TER	|= 1 << TRACE_PORT ;	// ITM Trace Enable Register. Enabled tracing on stimulus ports. One bit per stimulus port.

			traceEnabled = 1 ;

			// Send tasks information
			for (ii = 0 ; ii < AA_TASK_MAX ; ii++)
			{
				if (AA_ENONE == aaTaskGetTcb_ (ii | AA_HTAG_TASK, & pTcb))
				{
					// Valid task handle
					aaLogTrace_TaskInfo_ (pTcb) ;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------

void	aaLogTrace_1x8	(uint32_t code, uint8_t arg)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

		// Write code and parameter
		while (ITM->PORT [TRACE_PORT].u16 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u16 = __REV16 ((code << 8u) | (arg & 0x0FFu)) ;

		// write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------

void	aaLogTrace_2x8	(uint32_t code, uint8_t arg1, uint8_t arg2)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

		// Write code and parameter
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV ((code << 24u) | ((arg1 & 0x0FFu) << 8) | (arg2 & 0x0FFu)) ;	// Big endian

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------

void	aaLogTrace_1x8_1x16	(uint32_t code, uint8_t arg1, uint16_t arg2)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

		// Write code and parameters
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV ((code << 24) | ((arg1 & 0x0FFu) << 16) | (arg2 & 0x0FFFFu)) ;

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;


		bspRestoreIrq (intState) ;
	}
}
//--------------------------------------------------------------------------------

void	aaLogTrace_1x32		(uint32_t code, uint32_t arg)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

		// Write code
		while (ITM->PORT [TRACE_PORT].u8 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u8 = code ;

		// Write 32 bits arg
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (arg) ;

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------

void	aaLogTraceInterrupt		(uint32_t bEnter)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;
	uint32_t		data ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

#if (AA_WITH_TRACE_NOSYSTICK == 1)
		// Ignore Systick interrupt
		if ((SCB->ICSR & 0x1FFu) == 15)
		{
			bspRestoreIrq (intState) ;
			return ;
		}
#endif

		if (bEnter != 0)
		{
			// Enter interrupt
			data = AA_T_INTENTER ;	// Trace code
		}
		else
		{
			// Exit interrupt
			data = AA_T_INTEXIT ;	// Trace code
		}

		// Write code and interrupt number
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		data = (data << 24) | (SCB->ICSR & 0x1FFu) ;
		ITM->PORT[TRACE_PORT].u32 = __REV (data) ;

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------

void	aaLogTrace_Message (char * str)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;
	uint32_t		length ;
	uint8_t			* ptr  = (uint8_t *) str ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;


		length = aaSTRLEN (str) ;
		if (length != 0)
		{
			if (length > 256)
			{
				length = 256 ;
			}

			// Write code and length
			while (ITM->PORT [TRACE_PORT].u16 == 0)
			{
			}
			ITM->PORT[TRACE_PORT].u16 = __REV16 ((AA_T_MESSAGE << 8u) | (length - 1u)) ;

			// Write string
			while (length >= 4)
			{
				while (ITM->PORT [TRACE_PORT].u32 == 0)
				{
				}
				ITM->PORT[TRACE_PORT].u32 = * (uint32_t *) ptr ;
				ptr += 4 ;
				length -= 4 ;
			}

			while (length >= 2)
			{
				while (ITM->PORT [TRACE_PORT].u16 == 0)
				{
				}
				ITM->PORT[TRACE_PORT].u16 = * (uint16_t *) ptr ;
				ptr += 2 ;
				length -= 2 ;
			}

			while (length >= 1)
			{
				while (ITM->PORT [TRACE_PORT].u8 == 0)
				{
				}
				ITM->PORT[TRACE_PORT].u8 = * ptr ;
				ptr ++ ;
				length -- ;
			}
		}

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------

void	aaLogTrace_TaskInfo (aaTaskId_t taskId)
{
	aaTcb_t * pTcb ;

	if (aaTaskGetTcb_ (taskId, & pTcb) == AA_ENONE)
	{
		aaLogTrace_TaskInfo_ (pTcb) ;
	}
}

static void	aaLogTrace_TaskInfo_ (aaTcb_t * pTcb)
{
	aaIrqStatus_t	intState ;
	uint32_t		ts ;
	uint32_t		length ;
	uint8_t			* ptr ;

	if (traceEnabled != 0)
	{
		intState = bspSaveAndDisableIrq () ;

		ts = bspTsGet () ;

		ptr  = (uint8_t *) pTcb->name ;

		length = aaSTRLEN (pTcb->name) ;
		if (length != 0)
		{
			if (length > 254)
			{
				length = 254 ;
			}
		}

		// Write code, length, task Id, and priority
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (AA_T_TASKINFO << 24 | (length - 1 + 2) << 16 | (pTcb->taskIndex & 0x0FFu) << 8 | pTcb->priority) ;

		// Write name
		while (length >= 4)
		{
			while (ITM->PORT [TRACE_PORT].u32 == 0)
			{
			}
			ITM->PORT[TRACE_PORT].u32 = * (uint32_t *) ptr ;
			ptr += 4 ;
			length -= 4 ;
		}

		while (length >= 2)
		{
			while (ITM->PORT [TRACE_PORT].u16 == 0)
			{
			}
			ITM->PORT[TRACE_PORT].u16 = * (uint16_t *) ptr ;
			ptr += 2 ;
			length -= 2 ;
		}

		while (length >= 1)
		{
			while (ITM->PORT [TRACE_PORT].u8 == 0)
			{
			}
			ITM->PORT[TRACE_PORT].u8 = * ptr ;
			ptr ++ ;
			length -- ;
		}

		// Write time stamp
		while (ITM->PORT [TRACE_PORT].u32 == 0)
		{
		}
		ITM->PORT[TRACE_PORT].u32 = __REV (ts) ;

		bspRestoreIrq (intState) ;
	}
}

//--------------------------------------------------------------------------------
#endif // AA_WITH_TRACE
