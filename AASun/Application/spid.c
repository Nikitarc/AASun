/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	spid.c		PI controller for main period synchro of AASun (sample time is always 1, and no derivative)

	When		Who	What
	12/08/22	ac	Creation
	18/10/22	ac	Adapted and configured for for AASun synchro

	For a good "Beginner’s PID" :
	http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aalogmes.h"
#include	"aaprintf.h"

#include	"spid.h"

//----------------------------------------------------------------------

#define	SPID_MAX_ERROR	100			// ADC step
#define SPID_MAX_VALUE	5000		// Max value for PID value (Timer ARR)

static	int32_t			pFactor ;
static	int32_t			iFactor ;
static	int32_t			iTerm ;
static	int32_t			maxSumError ;
static	uint32_t		traceEn ;

//----------------------------------------------------------------------

void	sPidTraceEnable		(int32_t bEnable)
{
	traceEn = bEnable ;
}

//----------------------------------------------------------------------
//	Initialize the PI controller with the current output value

void	sPidInit (int32_t outValue)
{
	iTerm   = outValue * SPID_SCALE_FACTOR ;

	maxSumError = (int32_t) SPID_MAX_VALUE * SPID_SCALE_FACTOR ;
}

//----------------------------------------------------------------------
// After a manual change of the output value, warn the controller of this new value

void	sPidInitIntegrator (int32_t outValue)
{
	iTerm = outValue * SPID_SCALE_FACTOR ;
}

//----------------------------------------------------------------------
// Set new PI factors (i32.14 notation, 14 is SPID_SCALE_FACTOR)

void	sPidFactors (int32_t pFact, int32_t iFact)
{
	pFactor = pFact ;
	iFactor = iFact ;
}

//----------------------------------------------------------------------
//	Simplified algorithm because sample time is always 1, and no derivative

int32_t	sPid (int32_t error)
{
	int32_t		pTerm, pidValue ;

	if (error >  SPID_MAX_ERROR) error =  SPID_MAX_ERROR ;
	if (error < -SPID_MAX_ERROR) error = -SPID_MAX_ERROR ;

	// Calculate Pterm
	pTerm = pFactor * error ;

	// Calculate Iterm and limit integral runaway
	iTerm += error * iFactor ;

	if (iTerm > maxSumError)
	{
		iTerm = maxSumError ;
	}
	else if (iTerm < -maxSumError)
	{
		iTerm = -maxSumError ;
	}
	else
	{
	}

	pidValue = (pTerm + iTerm) / SPID_SCALE_FACTOR ;
	if (pidValue > SPID_MAX_VALUE)
	{
		pidValue = SPID_MAX_VALUE ;
	}
	else if (pidValue < 0)
	{
		pidValue = 0 ;
	}
	else
	{
	}

	if (traceEn != 0)
	{
//		aaPrintf ("pid(%8ld) = p=%+09ld i=%+09ld v=%5ld\r\n", error, pTerm, iTerm, pidValue) ;
//		aaPrintf ("(%4ld) p=%+09ld i=%+09ld v=%5ld\n", error, pTerm, iTerm, pidValue) ;
		aaPrintf ("(%4ld) v=%5ld\n", error, pidValue) ;
	}
	return pidValue ;
}

//----------------------------------------------------------------------
