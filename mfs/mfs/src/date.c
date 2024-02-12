
#include "stdint.h"

typedef struct
{
	uint8_t		hh ;
	uint8_t		mm ;
	uint8_t		ss ;

	uint8_t		dd ;
	uint8_t		mo ;
	uint16_t	yy ;

} localTime_t ;

localTime_t localTime ;

localTime_t localTime0 = { 23 , 59, 59,   28, 02, 2023 } ;
localTime_t localTime1 = { 23 , 59, 59,   28, 02, 2024 } ;
localTime_t localTime2 = { 23 , 59, 59,   29, 02, 2024 } ;
localTime_t localTime3 = { 23 , 59, 59,   29, 01, 2023 } ;
localTime_t localTime4 = { 23 , 59, 59,   30, 01, 2023 } ;
localTime_t localTime5 = { 23 , 59, 59,   31, 01, 2023 } ;

localTime_t localTime6 = { 23 , 59, 59,   28, 04, 2023 } ;
localTime_t localTime7 = { 23 , 59, 59,   29, 04, 2023 } ;
localTime_t localTime8 = { 23 , 59, 59,   30, 04, 2023 } ;

localTime_t localTime9  = { 23 , 59, 59,   30, 12, 2023 } ;
localTime_t localTime10 = { 23 , 59, 59,   31, 12, 2023 } ;

void dispTime (localTime_t * pTime)
{
	printf ("Time: %02u/%02u/%02u  %02u:%02u:%04u\n",
			pTime->hh, pTime->mm, pTime->ss,
			pTime->dd, pTime->mo, pTime->yy) ;
}
	
void	timeTick (localTime_t * pTime)
{
	uint8_t		dd, mo ;

	// Update local time
	pTime->ss ++ ;
	if (pTime->ss == 60u)
	{
		pTime->ss = 0u ;
		pTime->mm++ ;
		if (pTime->mm == 60u)
		{
			pTime->mm = 0u ;
			pTime->hh++ ;
			if (pTime->hh == 24u)
			{
				pTime->hh = 0u ;
				dd = pTime->dd + 1u ;
				mo = pTime->mo ;

				if (dd == 32)
				{
					dd = 1 ;
					mo ++ ;
				}
				if (dd == 31  &&  ( mo == 4 || mo == 6 || mo == 9 || mo == 11))
				{
					dd = 1 ;
					mo ++ ;
				}
				if (dd == 30  &&  (mo == 2))
				{
					dd = 1 ;
					mo ++ ;
				}
				if ((dd == 29)  &&  (mo == 2)  &&  ((pTime->yy %4) != 0))
				{
					dd = 1 ;
					mo ++ ;
				}
				if (mo == 13)
				{
					mo = 1 ;
					pTime->yy++ ;
				}
				pTime->dd = dd ;
				pTime->mo = mo ;
			}
		}
	}
}

void	incDate (localTime_t * pTime)
{
	dispTime (pTime) ;
	timeTick (pTime) ;
	dispTime (pTime) ;
	printf ("------\n") ;
}

void dateTest (void)
{
	incDate (& localTime0) ;

	incDate (& localTime1) ;

	incDate (& localTime2) ;

	incDate (& localTime3) ;

	incDate (& localTime4) ;

	incDate (& localTime5) ;

	incDate (& localTime6) ;
	incDate (& localTime7) ;
	incDate (& localTime8) ;

	incDate (& localTime9) ;
	incDate (& localTime10) ;
}
