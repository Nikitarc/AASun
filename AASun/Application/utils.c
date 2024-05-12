/*
----------------------------------------------------------------------

	Alain Chebrou

	utils.c		Some utility functions

	When		Who	What
	11/11/22	ac	Creation

----------------------------------------------------------------------
*/

#include	"aa.h"

#include	"util.h"

#include	"stdint.h"
#include	"stdlib.h"		// For div()
#include	"string.h"

//----------------------------------------------------------------------

bool				bDstWinter ;			// To manage DST

//----------------------------------------------------------------------
// Software timer management

softTimer_t		softTimers [TIMER_COUNT] ;

char	* timeDayName [7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" } ;

//----------------------------------------------------------------------
// To call only once at the start of the application
// Initialize the software timers

void	timersInit	(void)
{
	uint32_t	ii  ;

	for (ii = 0 ; ii < TIMER_COUNT ; ii++)
	{
		softTimers [ii].count = -1 ;	// Inactive
		softTimers [ii].done  = 0 ;		// Not expired
	}
}

//----------------------------------------------------------------------
// To call every second for software timer management

static	void	timersTick	(void)
{
	uint32_t	ii  ;

	for (ii = 0 ; ii < TIMER_COUNT ; ii++)
	{
		if (softTimers [ii].count >= 0)
		{
			// Active timer
			softTimers [ii].count-- ;
			if (softTimers [ii].count < 0)
			{
				// Expired
				softTimers [ii].done = 1 ;						// Signal
				softTimers [ii].count = softTimers [ii].init ;	// Restart
			}
		}
	}
}

//--------------------------------------------------------------------------------
//----------------------------------------------------------------------
// Local date/time management

// Initialize a localTime_t structure

void	timeInit (localTime_t * pTime)
{
	memset (pTime, 0 , sizeof (localTime_t)) ;
	pTime->dd = 1 ;
	pTime->mo = 1 ;
	pTime->yy = 2023 ;
	pTime->wd = 0 ;		// Sunday
	bDstWinter = 0 ;
}

//----------------------------------------------------------------------
// Return a word with the day date: year, month, day

uint32_t	timeGetDayDate (const localTime_t * pTime)
{
	return (pTime->yy << 16) + (pTime->mo << 8) + (pTime->dd) ;
}

//--------------------------------------------------------------------------------
//  Return 1 if the date is in winter Daylight Saving Time
//	Central europe only:
//		Switch to summer time on last Sunday of March.   At 02:00:00 add 1 hour
//		Switch to winter time on last Sunday of October. At 03:00:00 remove 1 hour

uint32_t	dstAdjust (localTime_t * pTime)
{
	uint32_t	dst = 0 ;

	if (pTime->mo == 3  &&  pTime->dd > 24)
	{
		// Last week of March
		dst += pTime->wd <= (pTime->dd - 25) ;
		if (pTime->wd == 0)
		{
			// The day to switch to summer time, only after 02:00:00
			if (pTime->hh < 2)
			{
				dst-- ;
			}
		}
	}
	else if (pTime->mo >= 4)
	{
		dst++ ;
		if (pTime->mo > 10)
		{
			dst-- ;
		}
		else if (pTime->mo == 10  &&  pTime->dd > 24)
		{
			// Last week of October
			dst -= pTime->wd <= (pTime->dd - 25) ;
			if (pTime->wd == 0)
			{
				// The day to switch to winter time, only after 03:00:00
				if (pTime->hh < 3)
				{
					dst++ ;
				}
			}
		}
	}

	if (dst > 0)
	{
		timeAddHour (pTime) ;
	}
	return dst ;
}

//----------------------------------------------------------------------
// Compute week day from date (Gregorian calendar)
// Sakamoto's methods https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
// https://www.quora.com/How-does-Tomohiko-Sakamotos-Algorithm-work
// Slightly modified to use only 1 division and 1 modulo

uint32_t timeDateToWd (const localTime_t * lt)
{
    uint32_t	i, y, y100 ;
    uint8_t		t [12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4} ;
                      //{ 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5} ;
    y = lt->yy - (lt->mo < 3) ;		// (lt->mo < 3) is 1 or 0
	y100 = y/100 ;
	i = (y + y/4 - y100 + (y100>>2) + t[lt->mo-1] + lt->dd) % 7 ;
    return i;
}

//--------------------------------------------------------------------------------
//	Add 1 hour to the date
//	This is used to adjust winter Daylight Saving Time

void	timeAddHour (localTime_t * pTime)
{
	int8_t		dd, mo ;

	pTime->hh++ ;
	if (pTime->hh == 24)
	{
		pTime->hh = 0u ;
		pTime->wd++ ;
		if (pTime->wd == 7)
		{
			pTime->wd = 0 ;
		}

		dd = pTime->dd + 1 ;
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

//----------------------------------------------------------------------
// This calendar works from 2001 until 2099 (no special care of leap years)
// return the time
// On the day of the changeover to winter time it will be 2 times 3 o'clock! But you only have to turn back the clock once...
// bDstWinter is used to manage this feature

uint32_t	timeTick (localTime_t * pTime)
{
	pTime->ss ++ ;
	if (pTime->ss == 60)
	{
		pTime->ss = 0u ;
		pTime->mm++ ;
		if (pTime->mm == 60)
		{
			pTime->mm = 0u ;

			// Daylight Saving Time (European only)
			// Here pTime->mm == 0  and  pTime->ss == 0
			if (pTime->dd > 24  &&  pTime->wd == 0)
			{
				if (pTime->mo == 3  &&  pTime->hh == 1)
				{
					pTime->hh++ ;	// Switch to summer time
				}
				else if (pTime->mo == 10  &&  pTime->hh == 2)
				{
					// On the day of the changeover to winter time it will be 2 times 3 o'clock
					// But you only have to turn back the clock once.
					if (! bDstWinter)
					{
						pTime->hh-- ;	// Switch to winter time
					}
					bDstWinter ^= 1 ;
				}
			}
			timeAddHour (pTime) ;
		}
	}
	// Update timers
	timersTick () ;
	return (pTime->hh << 16) | (pTime->mm << 8) | pTime->ss ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// https://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
// http://web.archive.org/web/20080303101624/http://c.snippets.org/snip_lister.php?fname=isqrt.c

/* usqrt:
    ENTRY x: unsigned long
    EXIT  returns floor(sqrt(x) * pow(2, BITSPERLONG/2))

    Since the square root never uses more than half the bits
    of the input, we use the other half of the bits to contain
    extra bits of precision after the binary point.

    EXAMPLE
        suppose BITSPERLONG = 32
        then    usqrt(144) = 786432 = 12 * 65536
                usqrt(32) = 370727 = 5.66 * 65536

    NOTES
        (1) change BITSPERLONG to BITSPERLONG/2 if you do not want
            the answer scaled.  Indeed, if you want n bits of
            precision after the binary point, use BITSPERLONG/2+n.
            The code assumes that BITSPERLONG is even.
        (2) This is really better off being written in assembly.
            The line marked below is really a "arithmetic shift left"
            on the double-long value with r in the upper half
            and x in the lower half.  This operation is typically
            expressible in only one or two assembly instructions.
        (3) Unrolling this loop is probably not a bad idea.

    ALGORITHM
        The calculations are the base-two analogue of the square
        root algorithm we all learned in grammar school.  Since we're
        in base 2, there is only one nontrivial trial multiplier.

        Notice that absolutely no multiplications or divisions are performed.
        This means it'll be fast on a wide range of processors.
*/

#define BITSPERLONG 32

#define TOP2BITS(x) ((x & (3u << (BITSPERLONG-2))) >> (BITSPERLONG-2))

// Modified to return only the rounded square root value

uint32_t usqrt (uint32_t x)
{
      uint32_t	a = 0u ;		// accumulator
      uint32_t	r = 0u ;		// remainder
      uint32_t	e = 0u ;		// trial product

      int32_t	ii ;

      for (ii = 0 ; ii < BITSPERLONG ; ii++)   /* NOTE 1 */
      {
            r = (r << 2) + TOP2BITS (x) ; x <<= 2 ; /* NOTE 2 */
            a <<= 1 ;
            e = (a << 1) + 1 ;
            if (r >= e)
            {
                  r -= e ;
                  a++ ;
            }
      }

	  a += 0x8000 ;			// Rounding
	  return (a >> 16) ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
/**
 * \brief    Fast Square root algorithm, with rounding
 *
 * This does arithmetic rounding of the result. That is, if the real answer
 * would have a fractional part of 0.5 or greater, the result is rounded up to
 * the next integer.
 *      - SquareRootRounded(2) --> 1
 *      - SquareRootRounded(3) --> 2
 *      - SquareRootRounded(4) --> 2
 *      - SquareRootRounded(6) --> 2
 *      - SquareRootRounded(7) --> 3
 *      - SquareRootRounded(8) --> 3
 *      - SquareRootRounded(9) --> 3
 *
 * \param[in] a_nInput - unsigned integer for which to find the square root
 *
 * \return Integer square root of the input value.
 */
uint32_t SquareRootRounded (uint32_t a_nInput)
{
    uint32_t op  = a_nInput ;
    uint32_t res = 0 ;
    uint32_t one = 1uL << 30 ; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type


    // "one" starts at the highest power of four <= than the argument.
    while (one > op)
    {
        one >>= 2 ;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one) ;
            res = res +  2 * one ;
        }
        res >>= 1 ;
        one >>= 2 ;
    }

    // Do arithmetic rounding to nearest integer
    if (op > res)
    {
        res++ ;
    }

    return res ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	Non blocking get string
//	Reads in at most one less than size characters from the console
//	and stores them into the buffer.
//	Reading stops after a \r, it is not stored into the buffer.
//	Characters exceeding size-1 are discarded.
//	A terminating null byte (\0) is stored after the last character in the buffer.
//	The return value is 1 when the \r is encountered, else 0.

//	Limited cooking of ANSI/xterm escape sequences : left/right arrow, backspace, SUP, home, end
//	Tested with putty as terminal

#define	DEL_CHAR	0x7Fu	// Sent by backspace key of PC keyboard
#define	BS_CHAR		0x08u	// ASCII backspace

// Copy src to the right (char insertion)
// count must be > 0
__STATIC_INLINE void	copyToRight (char * pSrc, uint32_t count)
{
	uint32_t	ii ;

	pSrc = & pSrc [count-1] ;
	for (ii = 0 ; ii < count ; ii++)
	{
		pSrc [1] = * pSrc ;
		pSrc-- ;
	}
}

// Copy src to the left (char deletion)
__STATIC_INLINE void	copyToLeft (char * pSrc, uint32_t count)
{
	aaMEMCPY (& pSrc [-1], pSrc, count) ;
}


uint32_t	aaGetsNonBlock		(char * pBuffer, uint32_t size)
{
	static	uint32_t 	nn ;
	static	uint32_t 	pos ;
	static	char		* pStr ;
	static	uint32_t	state = 0 ;

	uint32_t	ii  ;
	char		cc ;
	uint32_t	result = 0 ;	// 1 at the end of the input

	while (result == 0  &&  aaCheckGetChar () != 0)
	{
		switch (state)
		{
			case 0:			// Idle state, this is the beginning of an input
				nn = 0 ;
				pos = 0 ;
				pStr = pBuffer ;
				state = 1 ;
				break ;

			case 1:			// Waiting for standard char
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == '\r')
					{
						aaPutChar (cc) ;
						aaPutChar ('\n') ;
						pBuffer [nn] = 0 ;
						result = 1u ;
						state = 0 ;
						break ;		// End of input
					}
					if (cc == 0x1B)			// ESC
					{
						state = 2 ;
						break ;		// Wait for ESC parameter
					}
					else if (cc == BS_CHAR)
					{
						// Backspace: delete char at the left of current position
						if (pos > 0)
						{
							copyToLeft (pStr, nn-pos) ;
							aaPutChar (BS_CHAR) ;
							pStr-- ;
							pos-- ;
							nn-- ;

							for (ii = 0 ; ii < nn-pos ; ii++)
							{
								aaPutChar (pStr [ii]) ;
							}
							aaPutChar (' ') ;
							for (ii = 0 ; ii < nn-pos+1 ; ii++)
							{
								aaPutChar (BS_CHAR) ;
							}
						}
/*
						// Backspace: move one char left
						if (pos > 0)
						{
							pos-- ;
							pStr-- ;
							aaPutChar (BS_CHAR) ;
						}
*/
					}
					else if (cc == DEL_CHAR)
					{
						// DEL: delete char under the cursor
						if (pos != nn)
						{
							copyToLeft (& pStr [1], nn-pos-1) ;
							nn-- ;

							for (ii = 0 ; ii < nn-pos ; ii++)
							{
								aaPutChar (pStr [ii]) ;
							}
							aaPutChar (' ') ;
							for (ii = 0 ; ii < nn-pos+1 ; ii++)
							{
								aaPutChar (BS_CHAR) ;
							}
						}
					}
					else if (nn < (size - 1))
					{
						// Standard char, and buffer not full
						// If the 1st char is \n or \0 ignore
						if (nn != 0u  ||  (cc != '\n' && cc != 0u))
						{
							if (pos == nn)
							{
								aaPutChar (cc) ;
								* pStr++ = cc ;
								pos++ ;
								nn++ ;
							}
							else
							{
								// pos < nn, insert char at current position
								copyToRight (pStr, nn - pos) ;
								* pStr = cc ;
								nn++ ;

								for (ii = 0 ; ii < nn-pos ; ii++)
								{
									aaPutChar (pStr [ii]) ;
								}
								for (ii = 0 ; ii < nn-pos-1 ; ii++)
								{
									aaPutChar (BS_CHAR) ;
								}
								pStr++ ;
								pos++ ;
							}
						}
					}
					else
					{
						// Buffer overflow, ignore the char
					}
				}
				break ;

			case 2:			// Waiting for ESC parameter
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x5B)		// [
					{
						state = 3 ;
					}
					else
					{
						// Ignore NEXT char
						state = 6 ;
					}
				}
				break ;

			case 3:			// Waiting for ESC 0x5B parameter
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					switch (cc)
					{
						case '3':		// DEL char under the cursor
							if (pos != nn)
							{
								copyToLeft (& pStr [1], nn-pos-1) ;
								nn-- ;

								for (ii = 0 ; ii < nn-pos ; ii++)
								{
									aaPutChar (pStr [ii]) ;
								}
								aaPutChar (' ') ;
								for (ii = 0 ; ii < nn-pos+1 ; ii++)
								{
									aaPutChar (BS_CHAR) ;
								}
							}
							break ;

						case '4':		// END
							if (pos != nn)
							{
								for ( ; pos < nn ; pos++)
								{
									aaPutChar (* pStr++) ;
								}
							}
							break ;

						case 'D':		// LEFT
							if (pos > 0)
							{
								pos-- ;
								pStr-- ;
								aaPutChar (BS_CHAR) ;
							}
							state = 1 ;
							break ;

						case 'C':		// RIGHT
							if (pos != nn)
							{
								pos++ ;
								aaPutChar (* pStr++) ;
							}
							state = 1 ;
							break ;

						case 'B':		// DOWN
						case 'A':		// UP
						default:		// ???
							state = 1 ;
							break ;
					}
					if (cc >= '0'  && cc <= '9')
					{
						// HOME is ESC [ 1 0x7E, but other sequences are ESC [ 1 XX 0x7E
						if (cc == '1')
						{
							state = 4 ;
						}
						else
						{
							state = 5 ;
						}
					}
				}
				break ;

			case 4:			// Waiting for the 0x7E of ESC [ 1 0x7E
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x7E)
					{
						// This is home command
						while (pos > 0)
						{
							aaPutChar (BS_CHAR) ;
							pos-- ;
							pStr-- ;
						}
						state = 1 ;
					}
					else
					{
						state = 5 ;
					}
				}
				break ;

			case 5:			// Ignore chars until 0x7E
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x7E)
					{
						state = 1 ;		// End of ESC sequence
					}
				}
				break ;

			case 6:			// Ignore next char
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					state = 1 ;
				}
				break ;

			default:
				break ;
		}
	}
	return result ;
}

//------------------------------------------------------------------
//	Format an integer with fixed point notation: x.shift or Qshift
//	The returned value is the count of output chars : width+prec+1    (1 for the dot)
//	The width is the count of digits before the dot, and prec the count of digits after the dot
//	BEWARE: the size of the result buffer is not checked, and must be large enough

// https://www.i-programmer.info/programming/cc/13669-applying-c-fixed-point-arithmetic.html?start=2

static	const char  iFracDisplay_ [] = { "          -0" } ;

uint32_t 	iFracFormat (char * buffer, int32_t value, uint32_t shift, uint32_t width, uint32_t prec)
{
	int32_t		integer  = value >> shift ;
	uint32_t	frac     = value & ((1u << shift) - 1u) ;
	uint32_t	factor ;
	uint32_t	ii ;
	int32_t		sign = 1 ;

	if (value < 0)
	{
		sign = -1 ;
		value = -value ;
	}
	integer  = value >> shift ;
	frac     = value & ((1u << shift) - 1u) ;

	if (prec == 0)
	{
		// Special case if no digit after the dot
		ii = aaSnPrintf (buffer, width+1, "%*d", width, integer * sign) ;	// +1 terminating 0
	}
	else
	{
		// General case with width and precision
		factor = 10 ;
		for (ii = 1 ; ii < prec ; ii++)
		{
			factor *= 10 ;
		}

		frac = (factor * frac) ;		// Multiply to get the digits
		frac += 1u << (shift - 1) ;		// rounding
		frac >>= shift ;				// Adjust
		// Rounding could change the integer part
		if (frac >= factor)
		{
			// Rounding overflow
			integer++ ;
			frac -= factor ;
		}

		if (sign == -1  &&  integer == 0  &&  frac != 0)
		{
			// Special case: the integer is null and the fraction is negative
			// How to print the - before the 0? (there is no negative 0...)
//			aaPuts (iFracDisplay_ + sizeof (iFracDisplay_) - 1u - width) ;
//			ii = width + aaPrintf (".%0*u", prec, frac) ;
			strcpy (buffer, iFracDisplay_ + sizeof (iFracDisplay_) - 1u - width) ;
			ii = width + aaSnPrintf (buffer+width, prec+2, ".%0*u", prec, frac) ;
		}
		else
		{
			ii = aaSnPrintf (buffer, width+prec+1+1, "%*d.%0*u", width, integer*sign, prec, frac) ; // +1 dot, +1 terminating 0
		}
	}
	return ii ;
}

//----------------------------------------------------------------------

// Use a static buffer: save bytes on the stack, but not thread safe
static	char	iFracBuffer [16] ;

uint32_t 	iFracPrint (int32_t value, uint32_t shift, uint32_t width, uint32_t prec)
{
	uint32_t	len ;

	len = iFracFormat (iFracBuffer, value, shift, width, prec) ;
	aaPuts (iFracBuffer) ;
	return len ;
}

//----------------------------------------------------------------------
//	Formated itoa, slightly faster than aaPrintf ("%*d", width, val)
//	witdh is the minimal count of of the resulting string

#if defined(__CC_ARM)
	#pragma push
	#pragma Otime
#else
	#pragma GCC push_options
	#pragma GCC optimize ("O2")
#endif


int32_t 	inttoa (int32_t val, char * pStr, uint32_t len, uint32_t width)
{
	div_t		qr ;
	int32_t 	sum ;
	uint32_t	w = 0 ;
	uint32_t	neg = 0 ;

	if (len == 0)
	{
		* pStr = '\0' ;
		return -1 ;
	}
	if (val < 0)
	{
		neg = 1  ;
		sum = - val ;
	}
	else
	{
		sum = val ;
	}

	do
	{
		qr = div (sum, 10u) ;
		pStr [w++] = '0' + qr.rem ;
		sum = qr.quot ;
	} while (sum != 0  &&  (w < (len - 1))) ;

	if (w == (len - 1)  &&  sum != 0)
	{
		* pStr = '\0' ;
		return -1 ;
	}
	if (neg != 0)
	{
		pStr [w++] = '-' ;
	}

	// Fill to reach the required width
	while (w < width)
	{
		pStr [w++] = ' ' ;
	}

	pStr [w] = '\0';

	// Reverse the string in place
	uint32_t	ii = 0 ;
	uint32_t	jj = w - 1 ;
	char		cc ;
	while (ii < jj)
	{
		cc = pStr[ii] ;
		pStr[ii++] = pStr[jj] ;
		pStr[jj--] = cc ;
	}

	return 0 ;
}

#if defined(__CC_ARM)
	#pragma pop
#else
	#pragma GCC pop_options
#endif

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// The crc64() function is based on Nuttx works

/****************************************************************************
 * libs/libc/misc/lib_crc64.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#define CRC64_POLY   ((uint64_t)0x42f0e1eba9ea3693ull)
#define CRC64_INIT   ((uint64_t)0xffffffffffffffffull)
#define CRC64_XOROUT ((uint64_t)0xffffffffffffffffull)
#define CRC64_CHECK  ((uint64_t)0x62ec59e3f1a4f00aull)

//----------------------------------------------------------------------
// Continue CRC calculation on a part of the buffer

uint64_t crc64part (const uint8_t *src, size_t len, uint64_t crc64val)
{
	uint32_t i ;
	uint32_t j ;

	for (i = 0; i < len; i++)
	{
		crc64val ^= (uint64_t) src[i] << 56 ;
		for (j = 0; j < 8; j++)
        {
			if ((crc64val & ((uint64_t)1 << 63)) != 0)
			{
				crc64val = (crc64val << 1) ^ CRC64_POLY ;
			}
			else
			{
				crc64val = crc64val << 1 ;
			}
        }
	}
	return crc64val ;
}

//----------------------------------------------------------------------
// Return a 64-bit CRC of the contents of the 'src' buffer, length 'len'

uint64_t crc64 (const uint8_t *src, uint32_t len)
{
	return crc64part (src, len, CRC64_INIT) ^ CRC64_XOROUT;
}

//----------------------------------------------------------------------
// To check the crc64 implementation:
// CRC64_CHECK is the CRC64 of the string "123456789" without the null byte.
/*
static	const uint8_t checkbuf[] =
{
	'1', '2', '3', '4', '5', '6', '7', '8', '9'
} ;

bool 	cc64Check (void)
{
	uint64_t crc = crc64 (checkbuf, sizeof(checkbuf)) ;
	AA_ASSERT (crc == CRC64_CHECK) ;

	return (crc == CRC64_CHECK) ;
}
*/
//----------------------------------------------------------------------

void	getMacAddress (uint8_t * pMac)
{
	uint64_t crc ;

//	crc = crc64 ((const uint8_t *) UID_BASE, 12) ;
	crc = crc64part ((const uint8_t *) UID_BASE, 12, CRC64_INIT) ^ CRC64_XOROUT ;
	pMac [0]  = (crc >> 0) | 0x02 ;
	pMac [0] &= ~0x1 ;
	pMac [1]  = crc >> 8;
	pMac [2]  = crc >> 16;
	pMac [3]  = crc >> 24;
	pMac [4]  = crc >> 32;
	pMac [5]  = crc >> 40;
}

//----------------------------------------------------------------------
