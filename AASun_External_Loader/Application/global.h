/*
----------------------------------------------------------------------

	Alain Chebrou

	global.h	Definitions to include in every source files

	When		Who	What
	21/05/23	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined GLOBAL_H_
#define GLOBAL_H_
//--------------------------------------------------------------------------------

#include <stm32g0xx.h>
#include <stdint.h>

#define	VERSION			" V1.1"


// BEWARE: When you change WITH_MAIN value
//         don't forget to set the according linker script (in project linker general properties)
//         G071xx.ld    for test
//         extLoader.ld for external loader

// Value 1 to compile for test, 0 to compile for external loader
#define	WITH_MAIN		0



#define __ALWAYS_STATIC_INLINE	__attribute__ ((always_inline)) static inline
#define	BSP_ATTR_USED			__attribute__ ((used))

void	bspAssertFailed 	(uint8_t * pFileName, uint32_t line) ;
#define AA_ASSERT(expr) 	((expr) ? (void)0 : bspAssertFailed ((uint8_t *)__FILE__, __LINE__))

// .bss section limits
extern	uint32_t			_sbss ;
extern	uint32_t			_ebss ;

void	initTick			(void) ;
void	aaTaskDelay			(uint32_t ms) ;

void	bspSystemClockInit	(void) ;

void 	uartInit			(void) ;
void	uartPutChar			(char cc) ;
void	uartPuts			(char * pStr) ;
void	uartPrintI			(int32_t value) ;
void	uartPrintX			(uint32_t value, uint32_t width) ;

typedef enum
{
	AA_ENONE				= 0u,	// No error
	AA_EFAIL,						// Generic error
	AA_EARG,						// Bad argument value
	AA_ETIMEOUT,					// Timeout
	AA_EDEPLETED,					// Depleted resource
	AA_ESTATE,						// Operation not allowed for this object state
	AA_EWOULDBLOCK,					// Non blocking function would block
	AA_EFLUSH,						// Object flushed
	AA_ENOTALLOWED,					// Not allowed (e.g. from ISR)
	AA_EMEMORY						// Memory allocation error

} aaError_t ;

//--------------------------------------------------------------------------------
#endif	// GLOBAL_H_
