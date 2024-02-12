/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	newlib_reclaim.h	A function to release buffers and FILE structures in _rent,
						which Newlib does not do...

	When		Who	What
	21/04/20	ac	Creation

	Newlib is a complex library, which has many configuration flags,
	and we do not know which ones were used to compile Newlib.
	The functions provided here are compatible with Newlib provided at the time.
	There is no guarantee that they will be correct with future versions.
----------------------------------------------------------------------
*/

#ifndef NEWLIB_RECLAIM_H_
#define NEWLIB_RECLAIM_H_
//--------------------------------------------------------------------------------

#include	<stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

__ALWAYS_STATIC_INLINE void newlib_free_buffer (FILE * pFile)
{
	// Free the buffer of standard IO file
	if (pFile->_flags & __SMBF)
	{
		aaFree (pFile->_bf._base) ;
		pFile->_flags &= ~__SMBF ;
		pFile->_bf._base = (unsigned char *) NULL ;
		pFile->_p = (unsigned char *) NULL ;
	}

	pFile->_flags = 0 ;
}

__ALWAYS_STATIC_INLINE void	newlib_reclaim_reent (struct _reent * ptr)
{
	// Avoid to destroy _global_impure_ptr
	if (ptr != _global_impure_ptr)
	{
		// Call _fclose_r for all files in __sglue and releases their buffer (???)
		// _reclaim_reent() does nothing if ptr == _impure_ptr !!!!
		// We are sure ptr is not _impure_ptr
//		_impure_ptr = _global_impure_ptr ;
		_reclaim_reent (ptr) ;

		// Release buffers and FILE of standard IO (in _global_impure_ptr)
		newlib_free_buffer (ptr->_stdin) ;
		newlib_free_buffer (ptr->_stdout) ;
		newlib_free_buffer (ptr->_stderr) ;
	}
}

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// NEWLIB_RECLAIM_H_
