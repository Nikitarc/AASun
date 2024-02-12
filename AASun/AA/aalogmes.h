/*
----------------------------------------------------------------------
	
	AdAstra - Real Time Kernel

	Alain Chebrou

	aalogmes.c	Message logging facility

	When		Who	What
	25/05/17	ac	Creation
	23/07/17	ac	Add aaLogMesSetPutChar()
	12/03/20	ac	logMess task configuration in aacfg.h. Allows to not include this feature.
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

#if ! defined AA_LOGMES_H_
#define AA_LOGMES_H_
//--------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#if (AA_WITH_LOGMES == 1)
	void		aaLogMesSetPutChar	(uintptr_t dev, void (* pPutChar) (uintptr_t dev, char cc)) ;

	void		aaLogMes			(const char * fmt, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5) ;

	// Kernel internal use
	void		aaLogMesInit_		(void) ;
	void		aaLogMesNext_		(void) ;
	uint32_t	aaLogMesCheck_		(void) ;		//	Return 0 if there is no message

#else

	// This allows to undefine logmes, without to have to remove all logMess() calls
	#define	aaLogMes(fmt,a1,a2,a3,a4,a5)

#endif

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// AA_LOGMES_H_
