
/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	startup.c	The entry point after reset

	When		Who	What
	16/05/20	ac	This header added
					Move preinit/init routines in aaMain_()
					Remove fini
	18/12/23	ac	Initialize main stack content with 0

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
// ----------------------------------------------------------------------------
// This module contains the startup code for embedded C/C++ application
//
// Control reaches start_() from the reset handler.
//
// The actual steps performed by start_() are:
// - minimal system initialization: bspSystemInit_()
// - copy the initialized data region(s)
// - clear the BSS region(s)
// - branch to bspMain_()
//
// - The preinit/init array execution (for the C++ static constructors) is done in aaMain_()
//   when malloc and mutex are initialized
// - As the code never end, the fini array is ignored (for the C++ static destructors)
//
// The configuration is bare metal:
// The project linker must be configured without the startup sequence (-nostartfiles).

// ----------------------------------------------------------------------------

#include <stdint.h>

// ----------------------------------------------------------------------------
// These symbols are provided by the linker.

extern uint32_t	__data_regions_array_start ;
extern uint32_t	__data_regions_array_end ;
extern uint32_t	__bss_regions_array_start ;
extern uint32_t	__bss_regions_array_end ;

extern uint32_t _sstack_ ;			// From linker script: lower address of main stack
extern uint32_t _estack_ ;			// From linker script: address after end of stack


// ----------------------------------------------------------------------------
// Forward declarations

void	start_				(void);		// Entry point at startup

void	bspMain_			(void) ;	// Entry function in BSP
void	bspSystemInit_		(void) ;	// Very early minimum hardware initialization

// ----------------------------------------------------------------------------
// This is the place where the Cortex-M reset handler goes
//
// After Reset the Cortex-M processor is in Thread mode,
// priority is Privileged, and the Stack is set to Main.

void __attribute__ ((section(".startup"),noreturn))	start_ (void)
{
	uint32_t 	* p ;

	// Very early minimum hardware initialization
	// If you use ITCM, DTCM, ... you must enable clocks of theses devices
	// It also may be the place to stop watch dog
	// Or enable external RAM
	bspSystemInit_ () ;

	// Initialize the main stack content with 0
	p = (uint32_t *) & _sstack_ ;
	while (p != (uint32_t *) & _estack_)
	{
		* p++ = 0u ;
	}

	// Copy multiple DATA or CODE sections from FLASH to SRAM.
	// Iterate and copy word by word. It is assumed that the pointers are word aligned.
	for (p = & __data_regions_array_start ; p < & __data_regions_array_end ; )
	{
		uint32_t * from 		= (uint32_t *) (* p++) ;
		uint32_t * region_begin	= (uint32_t *) (* p++) ;
		uint32_t * region_end 	= (uint32_t *) (* p++) ;

		while (region_begin < region_end)
	    {
			* region_begin++ = * from++ ;
	    }
	}

	// Zero fill multiple BSS sections
	// Iterate and copy word by word. It is assumed that the pointers are word aligned.
	for (p = & __bss_regions_array_start ; p < & __bss_regions_array_end ; )
	{
		uint32_t * region_begin	= (uint32_t *) (* p++) ;
		uint32_t * region_end	= (uint32_t *) (* p++) ;

		while (region_begin < region_end)
	    {
			* region_begin++ = 0u ;
	    }
	}

	// Call the BSP entry point
	bspMain_ () ;	// Will never return

	// Should never reach this
	while (1)
	{
	}
}

// ----------------------------------------------------------------------------
