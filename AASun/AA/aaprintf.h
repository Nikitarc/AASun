/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aaprintf.h	Minimal printf, for format x X i d u o b c s f
				Use compiler intrinsics, no need of stdarg.h

				For 32 bits compilers only

	When		Who	What
	08/05/17	ac	Creation
	05/07/21	ac	Add aaGetDefaultConsole()
	22/07/21	ac	Add aaPuts()

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

#ifndef AA_PRINTF_H_
#define AA_PRINTF_H_
//--------------------------------------------------------------------------------
//	Standard input and output management

typedef struct aaDevIo_s
{
	char		(* getCharPtr)		(void * hDev) ;				// Get one char from dev
	void		(* putCharPtr)		(void * hDev, char cc) ;	// Put one char to   dev
	uint32_t	(* checkGetCharPtr)	(void * hDev) ;				// Get count of char available to read from dev
	uint32_t	(* checkPutCharPtr)	(void * hDev) ;				// Get count of char free space to write to dev without blocking
	void 		* hDev ;										// Handle to the device to use for input / output

} aaDevIo_t  ;

//--------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif


void		aaInitStdIn			(aaDevIo_t * pDev) ;
void		aaInitStdOut		(aaDevIo_t * pDev) ;
aaDevIo_t *	aaGetDefaultConsole	(void) ;

uint32_t	aaCheckPutChar		(void) ;
void		aaPutChar			(char cc) ;
uint32_t	aaCheckGetChar		(void) ;
char		aaGetChar			(void) ;

uint32_t	aaPrintf			(const char * fmt, ...) ;
uint32_t	aaPrintfEx			(void (* fnPutc) (char, uintptr_t), uintptr_t arg, const char * fmt, ...) ;
uint32_t	aaSnPrintf			(char * pBuffer, uint32_t size, const char * fmt, ...) ;
uint32_t	aaGets				(char * pBuffer, uint32_t size) ;
void		aaPuts				(const char * str) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif /* AA_PRINTF_H_ */
