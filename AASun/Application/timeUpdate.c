/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	timeUpdate.c

	When		Who	What
	24/04/24	ac	Creation, split from aaSun.c and enhanced

----------------------------------------------------------------------
*/

#include	"AASun.h"
#include	"wifi.h"

//--------------------------------------------------------------------------------
// The states of the state machine to update the date/time

typedef enum
{
	STS_IDLE	= 0,
	STS_LAN,
	STS_LAN_WAIT,
	STS_WIFI,
	STS_WIFI_WAIT,
	STS_LINKY,
	STS_LINKY_WAIT,
	STS_NONE,
	STS_DELAY

} timeStatus_t ;

static	timeStatus_t 	timeStatus ;

static	bool			bLinkyDateUpdate ;	// If true, the request to Linky to update the date/time is on

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Date/time update from the LAN interface

//	SNTP callback
//	result: 0 success, -1 timeout

void sntpCb (int32_t result, datetime * pTime)
{
	if (result == 0u)
	{
		// Success
		localTime_t		lTime ;

		lTime.hh = pTime->hh ;
		lTime.mm = pTime->mm ;
		lTime.ss = pTime->ss ;
		lTime.yy = pTime->yy ;
		lTime.mo = pTime->mo ;
		lTime.dd = pTime->dd ;
		lTime.wd = pTime->wd ;
		dstAdjust (& lTime) ;		// Adjust for Daylight Saving Time

aaPrintf("SNTP %02d:%02d:%02d  %04d/%02d/%02d\n", pTime->hh, pTime->mm, pTime->ss, pTime->yy, pTime->mo, pTime->dd) ;

		bspDisableIrq () ;			// Called from low priority task: need exclusive use
		localTime  = lTime ;
		bDstWinter = 0 ;
		bspEnableIrq () ;

		statusWSet (STSW_TIME_OK) ;

		// If required, signal to AASun task to start/synchronize power history
		if (timeIsUpdated == 1)
		{
			timeIsUpdated = 2 ;
		}
		timeNext (NEXT_RESET) ;	// Date/time updated
	}
	else
	{
aaPuts ("SNTP timeout\n") ;
		timeNext (NEXT_RETRY) ;	// Try next time source
	}
}

//--------------------------------------------------------------------------------
//	DNS callback
//	This callback is used to translate pool.ntp.org domain name to an ip address
//	then do a SNTP request to set the date/time

// Result:
//	0	Success, ip is valid
//	1	DNS primary server fail
//	2	DNS alternate server fail

void timeDnsCb (int32_t result, uint8_t * ip)
{
	if (result == 0u)
	{
		// Success
		aaPrintf ("DNS Translated to [%d.%d.%d.%d]\n", ip[0], ip[1], ip[2], ip[3]) ;
		sntpRequest (ip, SNTP_TZ, sntpCb) ; ;
	}
	else if (result == 1u)
	{
		// DNS primary fail, try alternate DNS
		aaPrintf ("DNS Pri fail\n") ;
		timeDnsRequest (1, NULL, timeDnsCb) ;
	}
	else
	{
		// DNS alternate fail
		aaPrintf ("DNS Alt fail\n") ;
		timeNext (NEXT_RETRY) ;	// Time update failed, try next time source
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Date/time update from the Linky
//	pStr is the string from the DATE group of the Linky standard mode
//	The Linky is managed by AASun task, so no need of exclusive use

void	timeUpdateTic (char * pStr)
{
	// Update the date/time only if it is required
	if (bLinkyDateUpdate)
	{
		if (pStr == NULL)
		{
			timeNext (NEXT_RETRY) ;	// Timeout, try next source
		}
		else
		{
			localTime.hh = ((pStr [7]  & 0x0F) * 10) + (pStr [8]  & 0x0F) ;
			localTime.mm = ((pStr [9]  & 0x0F) * 10) + (pStr [10] & 0x0F) ;
			localTime.ss = ((pStr [11] & 0x0F) * 10) + (pStr [12] & 0x0F) ;

			localTime.yy = ((pStr [1] & 0x0F) * 10) + (pStr [2] & 0x0F) + 2000 ;
			localTime.mo = ((pStr [3] & 0x0F) * 10) + (pStr [4] & 0x0F) ;
			localTime.dd = ((pStr [5] & 0x0F) * 10) + (pStr [6] & 0x0F) ;
			localTime.wd = timeDateToWd (& localTime) ;
			bDstWinter = 0 ;
aaPrintf("%02d:%02d:%02d  %04d/%02d/%02d\n", localTime.hh, localTime.mm, localTime.ss, localTime.yy, localTime.mo, localTime.dd) ;

			bLinkyDateUpdate = false ;
			statusWSet (STSW_TIME_OK) ;
			timeNext (NEXT_RESET) ;	// Date/time updated

			// If required, signal to AASun task to start/synchronize power history
			if (timeIsUpdated == 1)
			{
				timeIsUpdated = 2 ;
			}
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void	timeUpdateWifi (struct tm * pTm)
{
	if (pTm == NULL)
	{
		timeNext (NEXT_RETRY) ;	// Timeout, try next source
	}
	else
	{
		// If the year < 2000 => date not set, tm.tm_year is: year - 1900
		if (pTm->tm_year < 100)
		{
aaPuts ("WIFI date not set\n") ;
			timeNext (NEXT_RETRY) ;	// timeout, try next source
		}
		else
		{
aaPrintf ("WIFI %u %u/%02u/%02u %02u:%02u:%02u\n", pTm->tm_wday, 1900+pTm->tm_year, 1+pTm->tm_mon, pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec) ;

			bspDisableIrq () ;			// Called from low priority task: need exclusive use
			localTime.hh = pTm->tm_hour ;
			localTime.mm = pTm->tm_min ;
			localTime.ss = pTm->tm_sec ;
			localTime.yy = pTm->tm_year + 1900 ;
			localTime.mo = pTm->tm_mon ;
			localTime.dd = pTm->tm_mday ;
			localTime.wd = pTm->tm_wday ;
			bDstWinter = 0 ;
			bspEnableIrq () ;

			statusWSet (STSW_TIME_OK) ;
			timeNext (NEXT_RESET) ;		// Date/time updated

			// If required, signal to AASun task to start/synchronize power history
			if (timeIsUpdated == 1)
			{
				timeIsUpdated = 2 ;
			}
		}
	}
}

//--------------------------------------------------------------------------------
// Date/time update State Machine

void	timeNext (nextComman_t command)
{
	bool	bEnd = false ;

	if (command == NEXT_RESET)
	{
		// Date/time updated then reset the state machine
//aaPuts ("timeNext reset\n") ;
		timerStop (TIMER_DATE_IX) ;
		timeStatus = STS_IDLE ;
		return ;
	}

	if (command == NEXT_RETRY && timeStatus == STS_IDLE)
	{
		// Retry not allowed in IDLE status
		// This is the end of a manual date update
		timeStatus = STS_IDLE ;
		return ;
	}

	while (! bEnd)
	{
		switch (timeStatus)
		{
			case STS_IDLE:
				timeStatus = STS_LAN ;
				break ;

			case STS_LAN:
				if (statusWTest (STSW_NET_LINK_OFF) == 0)
				{
					// Wired LAN is ON
					// This starts a procedure to get an IP address, then send a SNTP request, then set the date/time
//aaPuts ("timeNext timeDnsRequest\n") ;
					timeDnsRequest (0, "pool.ntp.org", timeDnsCb) ;
					bEnd = true ;
					timeStatus = STS_LAN_WAIT ;		// Try next time source
				}
				else
				{
					timeStatus = STS_WIFI ;		// Try next time source
				}
				break ;

			case STS_LAN_WAIT:
//aaPuts ("timeNext LAN timeout\n") ;
				timeStatus = STS_WIFI ;		// Try next time source
				break ;

			case STS_WIFI:
//aaPuts ("timeNext WIFI date request\n") ;
				wifiDateRequest () ;
				bEnd = true ;
				timeStatus = STS_WIFI_WAIT ;
				break ;

			case STS_WIFI_WAIT:
//aaPuts ("timeNext WIFI timeout\n") ;
				timeStatus = STS_LINKY ;		// Try next time source
				break ;

			case STS_LINKY:
				if (ISOPT_LINKY  &&  ticMode == MODE_STD)
				{
					// The Linky is in standard mode, then it will updated date/time
//aaPuts ("timeNext Linky date request\n") ;
					ticTimeoutStart () ;
					bLinkyDateUpdate = true ;
					bEnd = true ;
					timeStatus = STS_LINKY_WAIT ;
				}
				else
				{
					bLinkyDateUpdate = false ;
					timeStatus = STS_NONE ;		// Try next time source
				}
				break ;

			case STS_LINKY_WAIT:
//aaPuts ("timeNext Linky timeout\n") ;
				bLinkyDateUpdate = false ;
				timeStatus = STS_NONE ;		// Try next time source
				break ;

			case STS_NONE:
				// No time source available, start a delay before retry
//aaPuts ("timeNext Delay start\n") ;
				timerStart  (TIMER_DATE_IX, TIMER_DATE_PERIOD) ;
				bEnd = true ;
				timeStatus = STS_DELAY ;
				break ;

			case STS_DELAY:
				// End of delay, retry
				timerStop (TIMER_DATE_IX) ;
//aaPuts ("timeNext Delay end\n") ;
				timeStatus = STS_LAN ;
				break ;

			default:
				AA_ASSERT (0) ;		// Unknown state
				break ;
		}
	}
}

//--------------------------------------------------------------------------------
// Need to update the time. Source may be SNTP, WIFI or Linky.
// If mode is 1: When the time is updated then synchronize the power history

void	timeUpdateRequest (uint32_t mode)
{
	if (mode == 1u)
	{
		// After time update synchronize power history
		timeIsUpdated = 1 ;		// This will be 2 when the date/time is updated
	}

	if (timeStatus == STS_IDLE)
	{
		// Date/time update not already requested
		timeNext (NEXT_START) ;
	}
}

//--------------------------------------------------------------------------------
//	Start a manual time update from the wired LAN

void	lanTimeRequest (uint32_t mode)
{
	timeDnsRequest (0, "pool.ntp.org", timeDnsCb) ;
	timeIsUpdated = mode ;
}

//--------------------------------------------------------------------------------
//	Start a manual time update from the wired LAN

void	linkyTimeRequest (uint32_t mode)
{
	ticTimeoutStart () ;
	bLinkyDateUpdate = true ;
	timeIsUpdated    = mode ;
}

//--------------------------------------------------------------------------------
