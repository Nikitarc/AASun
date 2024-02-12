/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	aaprintf.c	Minimal printf, for format x X i d u o b c s f
				Use GCC compiler intrinsics, no need of stdarg.h

				For 32 bits compilers only

	When		Who	What
	08/05/17	ac	Creation
	23/07/17	ac	Add aaPrintfEx()
	12/09/18	ac	Improve standard input and output handling
	10/10/19	ac	Add %p to print pointers
	18/05/20	ac	uint32_t aaDevIo_t.dev	=>	void * aaDevIo_t.hDev
	13/07/20	ac	Set minus sign close to the number for %d
	05/08/20	ac	Add * width support, width is now the minimum width, not the max width
					Add %i format (same as %d)
					Add simple %f format
	22/07/21	ac	Add aaPuts()
	08/06/22	ac	Less restriction on %f format
	13/08/22	ac	Add + flag : Allow to prepends a plus for positive signed-numeric types
					Remove bad - sign position when left padding with 0

----------------------------------------------------------------------
*/
/*
----------------------------------------------------------------------
	Copyright (c) 2013-2023 Alain Chebrou - AdAstra-Soft . com
	All rights reserved.

	This file is part of the AdAstra Real Time Kernel distribution.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.

	- Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.

	- Neither the name of the copyright holder nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aaprintf.h"

#include	"aalogmes.h"

#include	"math.h"

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	Standard input and output management
//	Standard output is used by aaPrintf()

// Default functions: theses functions allows to build a 'NUL' device
static	uint32_t	defCheck (void * hDev)
{
	(void) hDev ;
	return 0 ;
}

static	void		defPutChar (void * hDev, char cc)
{
	(void) hDev ;
	(void) cc ;
}

static	char 		defGetChar (void * hDev)
{
	(void) hDev ;
	return 0 ;
}

// The default descriptor avoids application fail when the standard in or out is not defined
STATIC	aaDevIo_t	defStdDev = { defGetChar, defPutChar, defCheck, defCheck, 0} ;

STATIC	aaDevIo_t	* pStdDevIn  = & defStdDev ;
STATIC	aaDevIo_t	* pStdDevOut = & defStdDev ;

//--------------------------------------------------------------------------------
// Set device for standard input

void	aaInitStdIn	(aaDevIo_t * pDev)
{
	// pStdDevIn must always contain a valid value
	if (pDev == NULL)
	{
		pStdDevIn = & defStdDev ;
	}
	else
	{
		pStdDevIn = pDev ;
	}
}

// Set device for standard output

void	aaInitStdOut (aaDevIo_t * pDev)
{
	// pStdDevOut must always contain a valid value
	if (pDev == NULL)
	{
		pStdDevOut = & defStdDev ;
	}
	else
	{
		pStdDevOut = pDev ;
	}
}

//--------------------------------------------------------------------------------
//	Return count of available char to write to standard output

uint32_t	aaCheckPutChar	(void)
{
	return (* pStdDevIn->checkPutCharPtr) (pStdDevOut->hDev) ;
}

//--------------------------------------------------------------------------------
//	Send a character on standard output

void	aaPutChar	(char cc)
{
	(* pStdDevOut->putCharPtr) (pStdDevOut->hDev, cc) ;
}

//--------------------------------------------------------------------------------
//	Return count of available char to read from standard input

uint32_t	aaCheckGetChar	(void)
{
	return (* pStdDevIn->checkGetCharPtr) (pStdDevIn->hDev) ;
}

//--------------------------------------------------------------------------------
//	Get a char from standard input

char	aaGetChar	(void)
{
	return (* pStdDevIn->getCharPtr) (pStdDevIn->hDev) ;
}

//--------------------------------------------------------------------------------
//	Send a string to the standard output. No \n added.

void	aaPuts	(const char * str)
{
	const char * pStr = str ;

	while (* pStr != 0)
	{
		(* pStdDevOut->putCharPtr) (pStdDevOut->hDev, * pStr++) ;
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	This does the actual parsing of the format. Very simplified for 32 bits CPU.
//	The number of chars written is returned.
//	Supports %{-}{+}{0| }{number|*}{.{prec|*}}{l}[%iduxXobcsp], base 2 8 10 16 only, no support for 64 bit numbers
//	Use < 64 bytes of stack, the compiler will optimize this.
//	Doesn't use malloc, thread safe

//	The l modifier is ignored, so no support of 64 bits integer nor very small or large double
//	To print a % use %%, not \%

//	Simple float number printing, support only 'f' format with restrictions:
//		The integral part and the fractional part of the float value must each fit into a 32 bits integer.
//		So the displayed integral and fractional parts can't be more than 9 digits each.

//	The width field specifies the MINIMUM number of characters to be output.
//	If the output value is shorter than the given width, it is padded to the appropriate width by putting blanks (default) on the left
//	If more characters are needed, the output will be wider than width
//	Example: %7.2f :	-3.128  => '  -3.13'
//						10000	=> '10000.00'

//	The precision number is the number of digits printed after the decimal dot.
//	If the precision number is 0 or the field is just a '.' with no number following, no decimal dot is printed


#define	STK_MAX		33		// To print 32 bits numbers as binary

static	uint32_t aaPrintf_ (void (* xputc) (char, uintptr_t), uintptr_t arg, const char * fmt, aaVA_LIST argptr)
{
	char	 	* ptr = NULL ;		// To avoid false warning from some compiler
	uint32_t 	width, prec, value, ii, total ;
	char		zero, chr, lJustify, minus ;
#if (AA_WITH_FLOAT_PRINT == 1)
	AA_FLOAT_T	fVal ;
#endif //AA_WITH_FLOAT_PRINT

	total = 0 ;
	while (* fmt != 0)
	{
		chr = * fmt++ ;
		if (chr != '%')
		{
			// Not a format code, just move to output
			(* xputc) (chr, arg) ;
			total++ ;
		}
		else
		{
			// Format code
			chr = * fmt++ ;
			if (chr == '%')
			{
				(* xputc) (chr, arg) ;
				total++ ;
				continue ;
			}
			lJustify	= 0 ;
			minus		= 0 ;
			width		= 0 ;
			prec		= 0 ;
			value		= 0 ;
			ii			= 0 ;
			zero		= ' ' ;

			if (chr == '-')
			{
				// Left justify
				lJustify = 1 ;
				chr = * fmt++ ;
			}
			if (chr == '+')		// Add + if the number is positive
			{
				minus = '+' ;
				chr = * fmt++ ;
			}
			if (chr == '0')		// Leading zeros
			{
				zero = '0' ;
				chr = * fmt++ ;
			}
			if (chr == ' ')		// Leading space
			{
				zero = ' ' ;
				chr = * fmt++ ;
			}

			if (chr == '*')
			{
				// Variable width
				width = (uint32_t) aaVA_ARG (argptr, long) ;	// Get width value
				chr = * fmt++ ;
			}
			else
			{
				while (aaISDIGIT ((int)chr))
				{
					// Field width specifier
					width = (width * 10u) + ((uint32_t) chr - '0') ;
					chr = * fmt++ ;
				}
			}

#if (AA_WITH_FLOAT_PRINT == 1)
			if (chr == 'm')
			{
				// Force minus sign. Internal use for float
				minus = '-' ;
				chr = * fmt++ ;
			}
#endif //AA_WITH_FLOAT_PRINT

			if (chr == '.')		// Precision
			{
				chr = * fmt++ ;
				if (chr == '*')
				{
					// Variable prec
					prec = (uint32_t) aaVA_ARG (argptr, long) ;	// Get prec value
					chr = * fmt++ ;
				}
				else
				{
					while (aaISDIGIT ((int)chr))
					{
						// Field precision specifier
						prec = (prec * 10u) + ((uint32_t) chr - '0') ;
						chr = * fmt++ ;
					}
				}
			}

			if (chr == 'l')
			{
				// Ignore the argument size specification
				chr = * fmt++ ;
			}

			// Get the parameter value, special case for %f
#if (AA_WITH_FLOAT_PRINT == 1)
			if (chr == 'f')
			{
				// float is promoted to double through ...
				volatile  double dbl = aaVA_ARG (argptr, double) ;
				fVal = (AA_FLOAT_T) dbl ;
			}
			else
#endif //AA_WITH_FLOAT_PRINT
			{
				value = (uint32_t) aaVA_ARG (argptr, long) ;	// Get parameter value
			}

			switch (chr)
			{
				case 'd':		// Decimal signed number
				case 'i':
					if ((value & 0x80000000u) != 0)
					{
						// The value is negative
						value = (uint32_t) (-(int32_t) value) ;
						minus = '-' ;
					}
					ii = 10 ;
					break ;

				case 'u':		// Unsigned number
					ii = 10 ;
					break ;

				case 'x':		// Hexadecimal number
				case 'X':
					ii = 16 ;
					break ;

				case 'o':		// Octal number
					ii = 8 ;
					break ;

				case 'b':		// Binary number
					ii = 2 ;
					break ;

				case 'c':		// Character data
					break ;

				case 's':		// String
					if (value == 0u)
					{
						ptr = "(null)" ;		// The pointer value is NULL
					}
					else
					{
						ptr = (char *) value ;	// Value is the ptr to the string
					}
					break ;

				case 'p':		// A pointer, displayed as %08x
					zero = '0' ;
					width = 8 ;
					ii = 16 ;
					break ;

#if (AA_WITH_FLOAT_PRINT == 1)
				case 'f':
					{
						int32_t		iTemp ;
						int32_t		fact ;
						int32_t		sign = 1 ;		// Default is positive value
						int32_t		wi = (int32_t) width ;
						int32_t		iInt, iFract ;

						if (fVal < 0.0)
						{
							sign = -1 ;
							fVal = -fVal ;
						}

						{
							AA_FLOAT_T	fInt ;
							// One of this branch will be removed by the compiler optimization
							if (sizeof (fInt) == 4)
							{
								fVal = modff ((float) fVal, (float *) & fInt) ;
							}
							else
							{
								fVal = modf ((double) fVal, (double *) & fInt) ;
							}
							iInt = (int32_t) fInt ;
						}
						// Now fVal is the fractional part

						// WARNING: Recursive call to aaPrintfEx(), but only once
						if (prec == 0)
						{
							// No precision, so don't display the decimal dot
							// Round to nearest
							if (fVal > 0.5)
							{
								iInt+= 1 ;
							}
							total += aaPrintfEx (xputc, arg, "%*d", wi, (iInt * sign)) ;
						}
						else
						{
							const char * pFmt ;

							// Adjust width to print before the '.'
							wi -= (int32_t) prec + 1 ;	// +1 for '.'
							if (wi < 0)
							{
								wi = 0 ;
							}

							// Build an integer from the fractional part
							fact = 1 ;
							for (iTemp = 0 ; iTemp < (int32_t) prec ; iTemp++)
							{
								fact *= 10 ;
							}
							fVal *= fact ;

							// Round fractional part to nearest integer
							iFract = (int32_t) (fVal + 0.5) ;
							if (iFract >= fact)
							{
								iFract -= fact ;
								iInt   += 1 ;
							}

							// Display the value
							if (sign == 1  ||  (iInt == 0  &&  iFract == 0))
							{
								// Display positive value or 0.00 values (to avoid -0.00)
								pFmt = "%*d" AA_FLOAT_SEP "%0*d" ;
							}
							else
							{
								// Display negative value, force  minus sign to allow to print -0.1 (integral part is 0)
								pFmt = "%*md" AA_FLOAT_SEP "%0*d" ;
							}
							total+= aaPrintfEx (xputc, arg, pFmt, wi, iInt, prec, iFract) ;
						}
					}
					continue ;
#endif //AA_WITH_FLOAT_PRINT

				default:		// All others
					(* xputc) ('%', arg) ;
					(* xputc) (chr, arg) ;
					total += 2 ;
					continue ;
			}

			{
				char		numstk [STK_MAX] ;

				if (chr != 's')		// ptr is already set for string
				{
					ptr = & numstk [STK_MAX-1] ;
					* ptr = 0 ;
				}

				if (chr == 'c')
				{
					*--ptr = (char) value ;
				}

				if (ii != 0u)		// For all numbers format, generate the ASCII string
				{
					do
					{
						chr = (char) ((value % ii) + '0') ;
						if (chr > '9')
						{
							chr = (char) (chr +  7) ;
						}
						*--ptr = chr ;
						value /= ii ;
					} while (value != 0) ;

					// if there is no padding then add sign if any
					if (width == 0  &&  minus != 0)
					{
						*--ptr = minus ;
					}
				}

				// Pad with 'zero' value if right justify enabled
				if (width != 0  &&  lJustify == 0)
				{
					ii = (uint32_t) aaSTRLEN ((const char *) ptr) ;
					if (zero == ' ')
					{
						// 'zero' is ' ', so add sign after padding
						if (minus != 0)
						{
							width-- ;
						}
						for ( ; ii < width ; ii++)
						{
							(* xputc) (zero, arg) ;
							total++ ;
						}
						if (minus != 0)
						{
							(* xputc) (minus, arg) ;
							total++ ;
						}
					}
					else
					{
						// 'zero' is 0, so add sign before padding
						if (minus != 0)
						{
							(* xputc) (minus, arg) ;
							width-- ;
							total++ ;
						}
						for ( ; ii < width ; ii++)
						{
							(* xputc) (zero, arg) ;
							total++ ;
						}
					}
				}

				// Move out data
				ii = 0 ;			// Count of char moved out
				if (lJustify != 0  &&  minus != 0)
				{
					(* xputc) (minus, arg) ;
					total++ ;
					ii++ ;
				}
				while (* ptr != 0)
				{
					(* xputc) (*ptr++, arg) ;
					total++ ;
					ii++ ;
				}
			}

			// Pad with 'zero' value if left justify enabled
			if (width != 0  &&  lJustify != 0)
			{
				while (ii < width)
				{
					(* xputc) (zero, arg) ;
					total++ ;
					ii++ ;
				}
			}
		}
	}

	return (total) ;
}

/*
	// to test sign and padding of integer print
 	uint32_t n ;
	aaPuts ("123456789\n") ;
	n = aaPrintf ("%+05d ",  122) ;	aaPrintf ("'+0122' %u\n", n) ;
	n = aaPrintf ("%+05d ", -122) ;	aaPrintf ("'-0122' %u\n", n) ;
	n = aaPrintf ("%05d ",   123) ;	aaPrintf ("'00123' %u\n", n) ;
	n = aaPrintf ("%05d ",  -123) ;	aaPrintf ("'-0123' %u\n", n) ;
	n = aaPrintf ("%+5d ",   124) ;	aaPrintf ("' +124' %u\n", n) ;
	n = aaPrintf ("%+5d ",  -124) ;	aaPrintf ("' -124' %u\n", n) ;
	n = aaPrintf ("%+ 5d ",  125) ;	aaPrintf ("' +125' %u\n", n) ;
	n = aaPrintf ("%+ 5d ", -125) ;	aaPrintf ("' -125' %u\n", n) ;
	n = aaPrintf ("%-5d ",   126) ;	aaPrintf ("'126  ' %u\n", n) ;
	n = aaPrintf ("%-+5d ",  126) ;	aaPrintf ("'+126 ' %u\n", n) ;
	n = aaPrintf ("%-5d ",  -126) ;	aaPrintf ("'-126 ' %u\n", n) ;
	n = aaPrintf ("%5d ",    127) ;	aaPrintf ("'  127' %u\n", n) ;
	n = aaPrintf ("%5d ",   -127) ;	aaPrintf ("' -127' %u\n", n) ;

	// To test float print
	n = aaPrintf ("%7.2f", -3.128 ) ; aaPrintf ("'  -3.13' %u\n", n) ;
	n = aaPrintf ("%7.2f",  1.985 ) ; aaPrintf ("'   1.99' %u\n", n) ;
	n = aaPrintf ("%7.2f",  1.995 ) ; aaPrintf ("'   2.00' %u\n", n) ;
	n = aaPrintf ("%7.2f", -1.985 ) ; aaPrintf ("'  -1.99' %u\n", n) ;
	n = aaPrintf ("%7.2f", -1.995 ) ; aaPrintf ("'  -2.00' %u\n", n) ;
	n = aaPrintf ("%7.2f", 10000.0) ; aaPrintf ("'10000.00' %u\n", n) ;
	n = aaPrintf ("%7.2f",    0.0 ) ; aaPrintf ("'   0.00' %u\n", n) ;
	n = aaPrintf ("%7.2f",   0.01 ) ; aaPrintf ("'   0.01' %u\n", n) ;
	n = aaPrintf ("%7.2f",  -0.01 ) ; aaPrintf ("'  -0.01' %u\n", n) ;
	n = aaPrintf ("%7.2f", -0.001 ) ; aaPrintf ("'   0.00' %u\n", n) ;
*/

//----------------------------------------------------------------------
//	Print to stdout

static void printfFn_ (char cc, uintptr_t arg)
{
	(void) arg ;
	aaPutChar (cc) ;
}

uint32_t aaPrintf (const char * fmt, ...)
{
	aaVA_LIST	va ;
	uint32_t 	nn = 0 ;

	aaVA_START (va, fmt) ;		// Set up parameter list pointer
	nn = aaPrintf_ (printfFn_, 0, fmt, va) ;
	aaVA_END (va) ;
	return nn ;
}

//----------------------------------------------------------------------
//	Print using the provided char output function
//	Allows to print to any device with the provided function

uint32_t aaPrintfEx (void (* fnPutc) (char, uintptr_t), uintptr_t arg, const char * fmt, ...)
{
	aaVA_LIST	va ;
	uint32_t 	nn ;

	aaVA_START (va, fmt) ;		// Set up parameter list pointer
	nn = aaPrintf_ (fnPutc, arg, fmt, va) ;
	aaVA_END (va) ;

	return nn ;
}

//----------------------------------------------------------------------
//	Print to a char string

typedef struct snPrintfEnv_s
{
	char		* pDst ;
	uint32_t	cnt ;

} snPrintfEnv_t ;

static void snPrintFn_ (char cc, uintptr_t arg)
{
	snPrintfEnv_t	* pEnv = (snPrintfEnv_t *) arg ;
	if (pEnv->cnt > 0)
	{
		* pEnv->pDst++ = cc ;
		pEnv->cnt-- ;
	}
}

//	The function aaSnPrintf() doesn't write more than size bytes, including the terminating '\0'.
//	The result string is always '\0' terminated.
//	If size is 0, then nothing is written to pBuffer, which can be NULL in this case.
//	The function returns the number of characters that would have been written had size
//	been sufficiently large, not counting the terminating '\0'.
//	Thus, a return value of size or more means that the output was truncated.

uint32_t aaSnPrintf (char * pBuffer, uint32_t size, const char * fmt, ...)
{
	aaVA_LIST		va ;
	uint32_t 		nn ;
	snPrintfEnv_t	env ;

	aaVA_START (va, fmt) ;		// Set up parameter list pointer
	env.pDst = pBuffer ;
	env.cnt = size ;
	nn = aaPrintf_ (snPrintFn_, (uintptr_t) & env, fmt, va) ;
	aaVA_END (va) ;

	// Add terminating null character
	if (size > 0)
	{
		if (env.cnt == 0)
		{
			// Overflow
			pBuffer [size-1] = 0 ;
		}
		else
		{
			pBuffer [nn] = 0 ;
		}
	}
	return nn ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

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

//----------------------------------------------------------------------
//	Reads in at most one less than size characters from the console
//	and stores them into the buffer.
//	Reading stops after a newline, it is not stored into the buffer.
//	Characters exceeding size-1 are discarded.
//	A terminating null byte (\0) is stored after the last character in the buffer.
//	The return value is the effective number of char in pBuffer, excluding final 0.

//	Limited cooking of ANSI/xterm escape sequences : left/right arrow, backspace, SUP, home, end
//	Tested with putty as terminal

uint32_t	aaGets		(char * pBuffer, uint32_t size)
{
	uint32_t	ii  ;
	uint32_t 	nn = 0 ;
	uint32_t 	pos = 0 ;
	char		* pStr = pBuffer ;
	char		cc ;

	while (1)
	{
		cc = aaGetChar () ;
		if (cc == '\r')
		{
			aaPutChar (cc) ;
			aaPutChar ('\n') ;
			pBuffer [nn] = 0 ;
			break ;	// End of input
		}
		if (cc == 0x1B)			// ESC
		{
			// Manage ESC [ xx
			cc = aaGetChar () ;
			if (cc == 0x5B)		// [
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
						break ;

					case 'C':		// RIGHT
						if (pos != nn)
						{
							pos++ ;
							aaPutChar (* pStr++) ;
						}
						break ;

					case 'B':		// DOWN
					case 'A':		// UP
					default:		// ???
						break ;
				}
				if (cc >= '0'  && cc <= '9')
				{
					// HOME is ESC [ 1 0x7E, but other sequences are ESC [ 1 XX 0x7E
					if (cc == '1')
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
						}
					}

					// Ignore chars until 0x7E
					while (cc != 0x7E)
					{
						cc = aaGetChar () ;
					} ;
				}
			}
			else
			{
				// Ignore next char
				cc = aaGetChar () ;
			}

		}
		else if (cc == BS_CHAR)
		{
			if (pos > 0)
			{
				pos-- ;
				pStr-- ;
				aaPutChar (BS_CHAR) ;
			}
		}
		else if (cc == DEL_CHAR)
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
		}
		else if (nn < (size - 1))
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
		else
		{
			// Buffer overflow, ignore the char
		}
	}

	return nn ;
}

//----------------------------------------------------------------------

