// -------------------- Retarget Locks --------------------
//
// share/doc/gcc-arm-none-eabi/pdf/libc.pdf:
//
// Newlib was configured to allow the target platform to provide the locking routines and
// static locks at link time. As such, a dummy default implementation of these routines and
// static locks is provided for single-threaded application to link successfully out of the box on
// bare-metal systems.
//
// For multi-threaded applications the target platform is required to provide an implementa-
// tion for *all* these routines and static locks. If some routines or static locks are missing, the
// link will fail with doubly defined symbols.
//
// (see also newlib/libc/misc/lock.c)
//
#include "aa.h"
#include "newlib.h"

#if (AA_WITH_NEWLIB_LOCKS == 1)

#if (__NEWLIB__ < 3  ||  _RETARGETABLE_LOCKING != 1)
	#error "This code is only for newlib version >= 3 with _RETARGETABLE_LOCKING enabled"
#endif

#include <sys/lock.h>
#include <reent.h>

//----------------------------------------------------------------------

struct __lock
{
	aaMutexId_t	mutexId ;
} ;

// To use less static mutexes: use only 1 mutex whose name is aliased to newlib static locks names

static	struct __lock	commonMutex ;

extern struct __lock	__attribute__((alias("commonMutex"))) __lock___sinit_recursive_mutex ;	// recursive lock used by stdio functions
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___sfp_recursive_mutex ;	// recursive lock used by stdio functions
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___atexit_recursive_mutex ;	// recursive lock used by atexit()
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___at_quick_exit_mutex ;	// lock used by at_quick_exit()
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___tz_mutex ;				// lock used by time zone related functions
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___dd_hash_mutex ;			// lock used by telldir(), seekdir() and cleanupdir()
extern struct __lock	__attribute__((alias("commonMutex"))) __lock___arc4random_mutex ;		// lock used by arc4random()

//----------------------------------------------------------------------
/*
// To build the locks before the constructors: can't use __attribute__((constructor))
// https://stackoverflow.com/questions/21244313/how-to-append-to-preinit-array-start-on-linux
__attribute__((used, section(".preinit_array"))) static void (*init_locks)(void) = init_retarget_locks_ ;

__attribute__(( section(".preinit_array"), used ))
    static void (* const preinit_array[])(void) = { & init_retarget_locks_ };
*/

// The init_retarget_locks_() function is called by the kernel main()
// after the kernel is initialized (to allow mutex use), but before the kernel is started,
// and before the C++ constructors which may use newlib.

void init_retarget_locks_ (void)
{
	if (AA_ENONE != aaMutexCreate	(& commonMutex.mutexId))
	{
		AA_ASSERT (0) ;
	}
}

//----------------------------------------------------------------------
// Special case for malloc API
// Without this, the default __malloc_lock() will use the __lock___malloc_recursive_mutex.
// AdAstra already provide a mutex to guard the malloc API

void __malloc_lock (struct _reent * ptr)
{
	(void) ptr ;
}

void __malloc_unlock (struct _reent * ptr)
{
	(void) ptr ;
}


//----------------------------------------------------------------------
// Special case for env API
// Without this, the default __env_lock() will use the __lock___env_recursive_mutex.
// AdAstra does not support env API: setenv(), getenv(), unsetenv()

void __env_lock (struct _reent * ptr)
{
	(void) ptr ;
}

void __env_unlock (struct _reent * ptr)
{
	(void) ptr ;
}

//----------------------------------------------------------------------
// Create a dynamic mutex

void __retarget_lock_init (_LOCK_T * lock_ptr)
{
	__retarget_lock_init_recursive (lock_ptr) ;
}


void __retarget_lock_init_recursive (_LOCK_T * lock_ptr)
{
	aaMutexId_t	mutexId ;

	if (AA_ENONE != aaMutexCreate	(& mutexId))
	{
		* lock_ptr = (_LOCK_T) (uint32_t)  AA_INVALID_MUTEX ;
		AA_ASSERT (0) ;
	}
	else
	{
		* lock_ptr = (_LOCK_T) (uint32_t) mutexId ;
	}
}

//----------------------------------------------------------------------
// Release a dynamic mutex

void __retarget_lock_close (_LOCK_T lock)
{
	__retarget_lock_close_recursive (lock) ;
}

void __retarget_lock_close_recursive (_LOCK_T lock)
{
	if (lock != & commonMutex)
	{
		aaMutexDelete ((aaMutexId_t) (uint32_t) lock) ;
	}
}

//----------------------------------------------------------------------

void __retarget_lock_acquire (_LOCK_T lock)
{
	__retarget_lock_acquire_recursive (lock) ;
}


void __retarget_lock_acquire_recursive (_LOCK_T lock)
{
	aaMutexId_t	mutexId = (aaMutexId_t) (uint32_t) lock ;

	if (lock == & commonMutex)
	{
		mutexId = lock->mutexId ;
	}
	aaMutexTake (mutexId, AA_INFINITE) ;
}

//----------------------------------------------------------------------
// return 1 if the caller successfully locked the mutex

int __retarget_lock_try_acquire (_LOCK_T lock)
{
	return __retarget_lock_try_acquire_recursive (lock) ;
}


int __retarget_lock_try_acquire_recursive (_LOCK_T lock)
{
	aaMutexId_t	mutexId = (aaMutexId_t) (uint32_t) lock ;

	if (lock == & commonMutex)
	{
		mutexId = commonMutex.mutexId ;
	}
	return AA_ENONE == aaMutexTryTake (mutexId) ? 1 : 0 ;
}

//----------------------------------------------------------------------

void __retarget_lock_release(_LOCK_T lock)
{
	__retarget_lock_release_recursive (lock)  ;
}


void __retarget_lock_release_recursive(_LOCK_T lock)
{
	aaMutexId_t	mutexId = (aaMutexId_t) (uint32_t) lock ;

	if (lock == & commonMutex)
	{
		mutexId = commonMutex.mutexId ;
	}
	aaMutexGive (mutexId) ;
}

//----------------------------------------------------------------------
#endif // AA_WITH_NEWLIB_LOCKS
