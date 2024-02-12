/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	_cxx.cpp		Local versions of some C++ support for debug

	When		Who	What
	02/11/19	ac	Creation

----------------------------------------------------------------------
*/

#include <cstdlib>
#include <sys/types.h>

// ----------------------------------------------------------------------------
// This function is redefined locally to warn the user about calling a 'pure virtual function'

namespace __gnu_cxx
{
	void	 __attribute__((noreturn))   __verbose_terminate_handler () ;

	void	__verbose_terminate_handler ()
	{
		abort () ;
	}
}

// ----------------------------------------------------------------------------
// This function is redefined locally to warn the user about C++ exception

extern "C"
{
	void	__attribute__((noreturn))  __cxa_pure_virtual () ;

	void	__cxa_pure_virtual ()
	{
		abort () ;
	}
}

// ----------------------------------------------------------------------------

