/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aautils.c	Some utility functions

	When		Who	What
	21/04/18	ac	Creation
	20/09/21	ac	Add argument to displaytaskInfo()

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
#include	"aaprintf.h"
#include	"aalogmes.h"

#include	"aautils.h"

#include	<ctype.h>

//----------------------------------------------------------------------
//	Memory dump
//	Dump a memory buffer to standard output

void     aaDump (uint8_t * pBuffer, uint32_t nb)
{
	uint8_t		* pSrc, * pDisp ;
	uint32_t   cpt, ii, c, na ;

	pSrc = pBuffer ;
	for (cpt = 0u ; cpt < nb ; cpt += 16u)
	{
		pDisp = pSrc ;

		// ii is the count of byte to display on this line
		ii = (nb - cpt) + 1u ;
		if (ii > 17u)
		{
			ii = 17u ;
		}

		// Display 1 line of 16 bytes
		aaPrintf ("%08X  ", pDisp) ;
		for (na = 1 ; na < ii ; na++, pSrc++)
		{
			c = ((* pSrc) >> 4) & 0xFu ;
			c += (c < 10u) ? (uint32_t) '0' : (uint32_t) 'A' - 10u ;
			aaPutChar ((char) c) ;

			c = (* pSrc) & 0x0Fu ;
			c += (c < 10u) ? (uint32_t) '0' : (uint32_t) 'A' - 10u ;
			aaPutChar ((char) c) ;

			if ((na & 0x03u) == 0u)
			{
				aaPutChar (' ') ;
			}
		}
		aaPutChar ('\n') ;
	}
}

//----------------------------------------------------------------------
//	Like dump, with better presentation, but uses more stack
//	Can provide the address to display. if pAddrs is NULL then use pBuffer

void     aaDumpEx (uint8_t * pBuffer, uint32_t nb, void * pAddrs)
{
	uint8_t    hx [36], as [19] ;
	uint8_t		* pSrc, * pDisp ;
	uint32_t   cpt, ii, c, na, nh ;

	if (pAddrs != NULL)
	{
		pDisp = * (uint8_t **) pAddrs ;
	}
	else
	{
		pDisp = pBuffer ;
	}
	pSrc = pBuffer ;
	for (cpt = 0u ; cpt < nb ; cpt += 16u)
	{
		aaMEMSET (hx, ' ', sizeof (hx)-1u) ;
		hx [35] = 0u ;
		aaMEMSET (as, ' ', sizeof (as)-1u) ;
		as [18] = 0u ;
		as [0] = as [17] = '-' ;

		// ii is the count of byte to display on this line
		ii = (nb - cpt) + 1u ;
		if (ii > 1u)
		{
			ii = 17u ;
		}

		// Display 1 line of 16 bytes
		for (na = 1u, nh = 0u ; na < ii ; na++)
		{
			if (na == 5u  ||  na == 9u  ||  na == 13u)
			{
				nh++ ;
			}

			c = ((* pSrc) >> 4u) & 0x0Fu ;
			c += (c < 10u) ? '0' : 'A' - 10u ;
			hx [nh++] = (uint8_t) c ;

			c = (* pSrc) & 0x0Fu ;
			c += (c < 10u) ? '0' : 'A' - 10u ;
			hx [nh++] = (uint8_t) c ;

			c = * (pSrc++) ;
			if (c < ' '  ||  c > 127u)
			{
				c = '.' ;	// Control char
			}
			as [na] = (uint8_t) c ;
		}
		aaPrintf ("%08X  %s  %s\n", pDisp, hx, as) ;			// 32 bits systems
		pDisp += 16 ;
	}
}

//----------------------------------------------------------------------
//	Simple random generator
//	Probably the most common type has k=1, and needs a single seed, with each new integer a function of the previous one.
//	An example is this congruential RNG, a form of which was the system RNG in VAXs for many years:

// A random initial x to be assigned by the calling program
static uint32_t aaRandom1SeedValue = 123456789;

void aaRandom1Seed (uint32_t seed)
{
	aaRandom1SeedValue = seed ;
}

// Lightweight reentrancy protection because the computing time is very short
uint32_t aaRandom1 (void)
{
	uint32_t		value ;
	aaIrqStatus_t	intState ;

	intState = bspSaveAndDisableIrq () ;
	value = aaRandom1SeedValue = 69069 * aaRandom1SeedValue + 362437 ;
	bspRestoreIrq (intState) ;

	return value ;
}

//----------------------------------------------------------------------
//	Here is an example with k=5, period about 2^160, one of the fastest long period RNGs,
//	returns more than 120 million random 32-bit integers/second (1.8MHz CPU), seems to pass all tests

// Replace defaults with five random seed values in calling program
static uint32_t x = 123456789, y = 362436069, z = 521288629, w = 88675123, v = 886756453 ;

void aaRandom2Seed (uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint32_t s5)
{
	x = s1 ;
	y = s2 ;
	z = s3 ;
	w = s4 ;
	v = s5 ;
}

uint32_t aaRandom2 (void)
{
	aaIrqStatus_t	intState ;
	uint32_t		t ;

	intState = bspSaveAndDisableIrq () ;
	t = (x ^ (x >> 7)) ;
	x = y ;
	y = z ;
	z = w ;
	w = v ;
	v = (v ^ (v << 6)) ^ (t ^ (t << 13)) ;
	t =  (y + y + 1) * v ;
	bspRestoreIrq (intState) ;

	return t ;
}

//----------------------------------------------------------------------

void	displaytaskInfo (aaTaskInfo_t * pTaskInfo)
{
	aaTaskInfo_t	* pInfo ;
	uint32_t		taskCount ;
	uint32_t		totalCpuUsage;
	uint32_t		criticalUsage;
	uint32_t		ii, nn ;
	const char		* pStr ;

	(void) aaTaskInfo (pTaskInfo, AA_TASK_MAX, & taskCount, & totalCpuUsage, & criticalUsage, 0) ;

	aaPrintf ("\ntI tName     State      BP   P  Stack        CPU  %%\n") ;
	pInfo = pTaskInfo ;
	nn = 0 ;
	for (ii = 0 ; ii < taskCount ; ii++)
	{
		aaTaskGetName (pInfo->taskId, & pStr) ;
		aaPrintf ("%02u %-8s  %-9s %3u %3u  ", pInfo->taskId & AA_HTAG_IMASK, (uintptr_t) pStr, (uintptr_t) aaTaskStateName[pInfo->state], pInfo->basePriority, pInfo->priority) ;
		aaPrintf ("%5u %10u ", pInfo->stackFree, pInfo->cpuUsage) ;
		if (totalCpuUsage != 0)
		{
			if ((totalCpuUsage & 0xFF000000u) != 0u)
			{
				aaPrintf ("%2u\n", ((pInfo->cpuUsage >> 8u) * 100u) / (totalCpuUsage >> 8u)) ;
			}
			else
			{
				aaPrintf ("%2u\n", (pInfo->cpuUsage * 100u) / totalCpuUsage) ;
			}
		}
		aaPrintf ("\n") ;
		nn += pInfo->cpuUsage;
		pInfo++ ;
	}
	aaPrintf ("TCPU:%u TTask:%u  Delta:%u\n", totalCpuUsage, nn, totalCpuUsage-nn) ;
	aaPrintf ("Critical max:%u\n\n", criticalUsage) ;
}

//----------------------------------------------------------------------

void		aaStrToUpper		(char * pStr)
{
	while (* pStr != 0)
	{
		* pStr = toupper (* pStr) ;
		pStr++ ;
	}
}

//----------------------------------------------------------------------
