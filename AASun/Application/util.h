/*
----------------------------------------------------------------------

	Alain Chebrou

	utils.c		Some utility functions

	When		Who	What
	11/11/22	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined UTIL_H_
#define UTIL_H_
//--------------------------------------------------------------------------------
#include "stdbool.h"

// Must be same as datetime in sntp.h
typedef struct
{
	int16_t		yy ;
	int8_t		mo ;
	int8_t		dd ;

	int8_t		hh ;
	int8_t		mm ;
	int8_t		ss ;

	int8_t		wd ;	// Week day, 0 is Sunday

} localTime_t ;

typedef struct
{
	int32_t		count ;		// Current timer count, inactive if < 0
	int32_t		init ;		// Initialization value
	uint8_t		done ;		// #0 if expired and not acknowledged

} softTimer_t ;

#define	TIMER_COUNT			3

extern	softTimer_t			softTimers [TIMER_COUNT] ;
extern	char				* timeDayName []  ;


#define	TIMER_ENERGY_IX		0						// Write energy counters every TIMER_ENERGY_PERIOD
#define	TIMER_ENERGY_PERIOD	(2*3600)				// 3600 is an hour: write total energy counters to flash every TIMER_ENERGY_PERIOD seconds

#define	TIMER_DATE_IX		1						//  Update date/time TIMER_DATE_PERIOD seconds after midnight
#define	TIMER_DATE_PERIOD	(POWER_HISTO_PERIOD/2)

#define	TIMER_HISTO_IX		2						// Update power history every TIMER_HISTO_PERIOD
#define	TIMER_HISTO_PERIOD	POWER_HISTO_PERIOD

//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------

void		timersInit			(void) ;

void		timeInit			(localTime_t * pTime) ;
uint32_t	timeTick 			(localTime_t * pTime) ;	// To call every second
uint32_t	timeGetDayDate		(localTime_t * pTime) ;

//--------------------------------------------------------------------------------
//	Start a software timer
//	At the end of the delay: timerExpired() returns 1, and the timer is restarted

__ALWAYS_STATIC_INLINE	void timerStart (uint32_t timerIx, uint32_t count)
{
	softTimers [timerIx].init  = count - 1 ;
	softTimers [timerIx].count = softTimers [timerIx].init ;
	softTimers [timerIx].done  = 0u ;
}

//--------------------------------------------------------------------------------
// This allows to have the first period different from the following ones

__ALWAYS_STATIC_INLINE	void timerNext (uint32_t timerIx, uint32_t count)
{
	softTimers [timerIx].init  = count - 1 ;
}

//--------------------------------------------------------------------------------

__ALWAYS_STATIC_INLINE	void timerStop (uint32_t timerIx)
{
	softTimers [timerIx].count = -1 ;
	softTimers [timerIx].done  = 0u ;
}

//--------------------------------------------------------------------------------
// This function allow to know if the timer as expired, then acknowledges the timer.
// Then the timer can only be tested once.

__ALWAYS_STATIC_INLINE	bool timerExpired (uint32_t timerIx)
{
	if (softTimers [timerIx].done != 0u)
	{
		softTimers [timerIx].done = 0u ;
		return true ;
	}
	return false ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

uint32_t	usqrt				(uint32_t x) ;
uint32_t	aaGetsNonBlock		(char * pBuffer, uint32_t size) ;
uint32_t 	iFracPrint			(int32_t value, uint32_t shift, uint32_t width, uint32_t prec) ;
uint32_t 	iFracDisplay		(char * buffer, int32_t value, uint32_t shift, uint32_t width, uint32_t prec) ;
int32_t 	inttoa				(int32_t val, char * pStr, uint32_t len, uint32_t width) ;

bool 		cc64Check			(void) ;
uint64_t 	crc64				(const uint8_t *src, uint32_t len) ;
void		getMacAddress		(uint8_t * pMac) ;

//--------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif
//--------------------------------------------------------------------------------
#endif	// W25Q_H_

