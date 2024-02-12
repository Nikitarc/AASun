/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aalogmes.c	Message logging facility, use aaPrintf

	When		Who	What
	25/05/17	ac	Creation
	23/07/17	ac	Add aaLogMesSetPutChar()
	12/03/20	ac	logMess task configuration in aacfg.h. Allows to not include this feature.
	30/05/20	ac	The stack is always statically allocated
	08/09/21	ac	No more logMess task. The messages are displayed by the idle task.

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
#include	"aaerror.h"
#include	"aaprintf.h"
#include	"aalogmes.h"
#include	"bspcfg.h"		// For critical section

#if (AA_WITH_LOGMES == 1)
//--------------------------------------------------------------------------------

#define	LM_MAX_PARAM		5	// aaLogMes() Parameters count, excluding format

static	void				defPutChar (uintptr_t dev, char cc) ;

//--------------------------------------------------------------------------------
// Messages buffers

STATIC	uintptr_t			lmBuffer [AA_LOGMES_MAXBUF][LM_MAX_PARAM+1] BSP_ATTR_NOINIT ;

STATIC	uint32_t			iwBuffer ;				// Index of the next free buffer
STATIC	uint32_t			irBuffer ;				// Index of the next buffer to display
STATIC	volatile uint32_t	nbMes ;					// Count of free buffers
STATIC	uint32_t			minFree = AA_LOGMES_MAXBUF ;	// Memorize buffer max level

STATIC	uint32_t			lostCount ;				// Amount of messages lost due to full buffer

STATIC	void (* pLmPutChar) (uintptr_t dev, char cc) = defPutChar ;	// Putchar Function
STATIC	uintptr_t			lmPutCharDev ;			// PutChar device

//--------------------------------------------------------------------------------
//	Set the function to output a char

// The function used by log task to output a char
static	void logMesPutChar (char cc, uintptr_t arg)
{
	(void) arg ;
	(* pLmPutChar) (lmPutCharDev, cc) ;
}

// The default output function outputs nothing
static	void	defPutChar (uintptr_t dev, char cc)
{
	(void) dev ;
	(void) cc ;
}

void	aaLogMesSetPutChar	(uintptr_t dev, void (* pPutChar) (uintptr_t dev, char cc))
{
	// pLmPutChar must always contain a valid value
	if (pPutChar != NULL)
	{
		pLmPutChar	= pPutChar ;
		lmPutCharDev	= dev ;
	}
	else
	{
		pLmPutChar	= defPutChar ;
	}
}

//--------------------------------------------------------------------------------
//	User message logging facility function

void	aaLogMes (const char * fmt, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
	uintptr_t	* pi ;
	uint32_t	ended = 0u ;

	// Check if logMes can output chars
	if (pLmPutChar != NULL)
	{
		while (ended == 0u)
		{
			aaCriticalEnter () ;

			// Try to get a buffer
			if (nbMes == 0u)
			{
				lostCount ++ ;
				ended = 1u ;
				aaCriticalExit () ;
			}
			else
			{
				pi = lmBuffer [iwBuffer] ;

				if (lostCount != 0u)
				{
					// Special message "Lost message"
					pi[0] = (uintptr_t) "logMes lost: %u\n";
					pi[1] = lostCount ;
					lostCount = 0u ;
				}
				else
				{
					// Fill the buffer
					pi[0] = (uintptr_t) fmt ;
					pi[1] = a1 ;
					pi[2] = a2 ;
					pi[3] = a3 ;
					pi[4] = a4 ;
					pi[5] = a5 ;

					ended = 1u ;
				}

				// nbMes is the only variable common to aaLogMes() and aaLogMesNext_()
				nbMes -- ;
				if (nbMes < minFree)
				{
					minFree = nbMes ;
				}

				iwBuffer++ ;
				if (iwBuffer == AA_LOGMES_MAXBUF)
				{
					iwBuffer = 0u ;
				}

				aaCriticalExit () ;
			}
		}
	}
}

//--------------------------------------------------------------------------------
// The function called from the idle task to display next message

void	aaLogMesNext_ (void)
{
	uintptr_t	* pi ;

	if (nbMes != AA_LOGMES_MAXBUF)
	{
		// Print one message
		pi = lmBuffer [irBuffer] ;
		aaPrintfEx (logMesPutChar, 0, (const char *) pi[0], pi[1], pi[2], pi[3], pi[4], pi[5]) ;

		// Free buffer
		aaCriticalEnter () ;
		nbMes++ ;
		aaCriticalExit () ;

		irBuffer++ ;
		if (irBuffer == AA_LOGMES_MAXBUF)
		{
			irBuffer = 0 ;
		}
	}
}

//--------------------------------------------------------------------------------
//	Check if there is a message to display
//	Return 0 if there is no message

uint32_t	aaLogMesCheck_ (void)
{
	return (nbMes != AA_LOGMES_MAXBUF) ;
}

//--------------------------------------------------------------------------------
//	Logging facility initialization

void	aaLogMesInit_ (void)
{
	irBuffer	= 0u ;
	iwBuffer	= 0u ;
	lostCount	= 0u ;
	nbMes		= AA_LOGMES_MAXBUF ;
}

//--------------------------------------------------------------------------------
#endif	// AA_WITH_LOGMES
