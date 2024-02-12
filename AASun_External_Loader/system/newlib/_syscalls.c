/**
*****************************************************************************
**
**  File        : syscalls.c
**
**  Abstract    : System Workbench Minimal System calls file
**
** 		          For more information about which c-functions
**                need which of these lowlevel functions
**                please consult the Newlib libc-manual
**
**  Environment : System Workbench for MCU
**
**  Distribution: The file is distributed “as is,” without any warranty of any kind.
**
*****************************************************************************
**
** <h2><center>&copy; COPYRIGHT(c) 2014 Ac6</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of Ac6 nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	_syscalls.c	Target BSP specific configuration
				The API of this file is for the exclusive use of the kernel and drivers
				The BSP API usable by applications are in bsp.h

	When		Who		What
	20/04/20	ac		Remove LF -> CRLF conversion in _write()
	01/06/20	ac		remove _sbrk() from AA_WITH_MALLOC condition

----------------------------------------------------------------------
*/

/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

//----------------------------------------------------------------------

//char	* __env[1] = { 0 } ;
//char	** environ = __env ;

//----------------------------------------------------------------------
//	These functions are used by newlib-nano
//	They are valid only if some sort of heap management is allowed

//	These functions replace those of newlib.
//	Heap is managed by the kernel: Deterministic, thread safe, without sbrk
/*
_PTR	_EXFUN_NOTHROW(_malloc_r,(struct _reent *, size_t));
_PTR	_EXFUN_NOTHROW(_calloc_r,(struct _reent *, size_t, size_t));
_VOID	_EXFUN_NOTHROW(_free_r,(struct _reent *, _PTR));
_PTR	_EXFUN_NOTHROW(_realloc_r,(struct _reent *, _PTR, size_t));
*/
#if (AA_WITH_MALLOC == 1)

void *	_malloc_r (struct _reent * pReent, size_t size)
{
	(void) pReent ;
	return aaMalloc (size) ;
}

void *	_calloc_r (struct _reent * pReent, size_t nmemb, size_t size)
{
	(void) pReent ;
	return aaCalloc (nmemb, size) ;
}

void	_free_r (struct _reent * pReent, void * pMem)
{
	(void) pReent ;
	aaFree (pMem) ;
}

void *	_realloc_r (struct _reent * pReent, void * pMem, size_t size)
{
	(void) pReent ;
	return aaRealloc (pMem, size) ;
}

#endif	// (AA_WITH_MALLOC == 1)

caddr_t _sbrk (int incr)
{
	(void) incr ;

	// WARNING The heap is not managed by _sbrk !!!!
//	AA_ASSERT (0) ;

	errno = ENOMEM ;
	return (caddr_t) -1 ;
}

//----------------------------------------------------------------------

int _getpid (void)
{
	return 1 ;
}

//----------------------------------------------------------------------

int _kill (int pid, int sig)
{
	(void) pid ;
	(void) sig ;

	errno = EINVAL ;
	return -1 ;
}

//----------------------------------------------------------------------
// On Release, call the hardware reset procedure.
// On Debug, triggers a debugger break point.
//
// It can be redefined in the application, if other functionality is required.

__attribute__((weak)) void _exit (int status)
{
	(void) status ;
/*
	#if ! defined (DEBUG)

		bspResetHardware() ;

	#else

		BSP_DEBUG_BKPT () ;

	#endif
*/
	while (1)	// To avoid message about no return
	{
	}
}

// ----------------------------------------------------------------------------

void __attribute__((weak,noreturn)) abort (void)
{
	_exit (1) ;
	while (1)	// To avoid message about no return
	{
	}
}

//----------------------------------------------------------------------
//	If file is STDIN_FILENO, the function attempts to read len bytes from the console.
//	The value returned may be less than len if the number of bytes available is less than len.

__attribute__((weak)) int _read (int file, char * ptr, int len)
{
	(void) file ;
	(void) ptr ;
	(void) len ;

	errno = ENOSYS ;
	return -1 ;
}

//----------------------------------------------------------------------

__attribute__((weak)) int _write (int file, char * ptr, int len)
{
	(void) file ;
	(void) ptr ;
	(void) len ;

	errno = ENOSYS ;
	return 0 ;
}

//----------------------------------------------------------------------

int _close (int file)
{
	(void) file ;

	return -1 ;
}


//----------------------------------------------------------------------

int _fstat (int file, struct stat * st)
{
	(void) file ;

	st->st_mode = S_IFCHR ;
	return 0 ;
}

//----------------------------------------------------------------------

int _isatty (int file)
{
	(void) file ;

	return 1 ;
}

//----------------------------------------------------------------------

int _lseek (int file, int ptr, int dir)
{
	(void) file ;
	(void) ptr ;
	(void) dir ;

	return 0 ;
}

//----------------------------------------------------------------------

int _open (char * path, int flags, ...)
{
	/* Pretend like we always fail */
	(void) path ;
	(void) flags ;

	return -1 ;
}

//----------------------------------------------------------------------

int _wait (int * status)
{
	(void) status ;

	errno = ECHILD;
	return -1 ;
}

//----------------------------------------------------------------------

int _unlink (char * name)
{
	(void) name ;

	errno = ENOENT;
	return -1 ;
}

//----------------------------------------------------------------------

int _times (struct tms * buf)
{
	(void) buf ;
	return -1 ;
}

//----------------------------------------------------------------------

int _stat (char * file, struct stat * st)
{
	(void) file ;
	(void) st ;

	st->st_mode = S_IFCHR ;
	return 0 ;
}

//----------------------------------------------------------------------

int _link (char *old, char *new)
{
	(void) old ;
	(void) new ;

	errno = EMLINK ;
	return -1 ;
}

//----------------------------------------------------------------------

int _fork (void)
{
	errno = EAGAIN;
	return -1 ;
}

//----------------------------------------------------------------------

int _execve (char * name, char ** argv, char ** env)
{
	(void) name ;
	(void) argv ;
	(void) env ;

	errno = ENOMEM ;
	return -1 ;
}

//----------------------------------------------------------------------
