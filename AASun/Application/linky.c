/*
----------------------------------------------------------------------

	Alain Chebrou

	linky.c		Linky TIC receiver
				Historic and Standard mode

	When		Who	What
	12/04/23	ac	Creation

	Historic frame:
	00 ADCO     022164996259
	01 OPTARIF          BASE
	02 ISOUSC             30
	03 BASE        007191269
	04 PTEC             TH..
	05 IINST             001
	06 IMAX              090
	07 PAPP            00430
	08 HHPHC               A
	09 MOTDETAT       000000


	The Linky is managed from the AASun task, so no need of mutex or critical section

----------------------------------------------------------------------
*/

#include "aa.h"
#include "gpiobasic.h"

#include "AASun.h"
#include "uartbasic.h"

#include "string.h"

//--------------------------------------------------------------------------------

// We don't check the byte parity so use 8 bits/no_parity instead of 7 bits/even_parity
#define	LUART			(100+1)		// LPUART1. LP => offset 100
#define	BAUDRATE_HIS	1200		// For historic TIC mode
#define	BAUDRATE_STD	9600		// For standard TIC mode

#define	TIC_TMO			3000		// Time interval to receive STX/ETX, switch to other mode

#define	TIC_TIME_TMO	3000		// Time interval to receive a time/date group

//------------------------------------------------------------------------------
//	TIC frame management

// Special protocol characters
#define	CC_STX			0x02
#define	CC_ETX			0x03
#define	CC_LF			0x0A
#define	CC_CR			0x0D

#define	TIC_GROUP_MAX	128		// Max size of a group line

// The states of the TIC automaton
typedef enum
{
	ticIdle		= 0,	// Waiting for STX
	ticSynchro,			// STX received, waiting for LF
	ticGroup,			// LF received, waiting for CR
	ticSearchSTX,		// Search for 1st STX
	ticSearchLF,		// Search for LF after STX
	ticSearchETX,		// Search for 1st ETX

} ticState_t ;

		uint8_t			ticMode ;			// MODE_HIS or MODE_STD
static	uint8_t			ticState ;			// The automaton current state
static	uint8_t			ticIndex ;			// Number of the current group
static	bool			ticDisplay ;		// Display groups if true
static	uint32_t		ticStartTime ;		// For the mode search timeout
static	char			ticBuffer [TIC_GROUP_MAX] ;	// Buffer to receive a group line
static	char			* ticPtr ;			// Current position in the group buffer
static	ticCallback_t	ticCallback ;		// The user function to call
static	uartHandle_t	hUart ;				// The UART handle

static	uint32_t		ticTimeTimeout ;

//------------------------------------------------------------------------------
//	The array of required TIC informations
//	This must be configured by the user

#define	TIC_VALUES_MAX	6		// Count of elements in pTicLabel

static	const	char * pTicLabel [TIC_VALUES_MAX] =
{
	// Values from standard mode
	"DATE",			// Date et heure courante
	"EAST",			// Energie active soutirée totale (Wh)
	"SINSTS",		// Puissance app. Instantanée soutirée (VA)
	"UMOY1",		// Tension moy. ph. 1 (V)

	// Values from historic mode
	"BASE",			// Energie active soutirée totale (Wh)
	"PAPP",			// Puissance app. Instantanée soutirée (VA)
} ;

//------------------------------------------------------------------------------
//	This starts the period to receive a TIC message

void	ticTimeoutStart (void)
{
	ticTimeTimeout = aaGetTickCount () ;
}

//------------------------------------------------------------------------------
//	A valid group is present in ticBuffer

static	void ticDecode (void)
{
	uint32_t	ii ;
	char		* pLabel  ;
	char		* pSavePtr ;

	ticTimeoutStart () ;	// Restart time out

	// If required dump the group
	if (ticDisplay)
	{
		if (ticIndex == 0u)
		{
			aaPuts ("---\n") ;	// Frame separator
		}
		aaPuts (ticBuffer) ;
		aaPuts ("\n") ;
	}

	// Extract the label
	pLabel = strtok_r (ticBuffer, " \t", & pSavePtr) ;
	if (pLabel != NULL)
	{
		for (ii = 0 ; ii < TIC_VALUES_MAX ; ii++)
		{
			if (! strcmp (pLabel, pTicLabel [ii]))
			{
				// Found the label, call the user function
				if (ticCallback != NULL)
				{
					(* ticCallback) (ii, pLabel, pSavePtr) ;
				}
				break ;
			}
		}
	}
}

//------------------------------------------------------------------------------
//	Receive the characters from the UART and reconstruct the groups of the frame.
//	Must be called periodically.

void	ticNext (void)
{
	uint32_t	cc ;

	// Check TIC timeout
	if ((aaGetTickCount () - ticTimeTimeout) >= TIC_TIME_TMO)
	{
		if (ticCallback != NULL)
		{
			(* ticCallback) (0, NULL, NULL) ;	// Signal the timeout
		}
		statusWClear (STSW_LINKY_ON) ;
		ticTimeoutStart () ;
	}

	// The UART baud rate is low so the loop count will be low
	// So we can use while()
	while (0u != uartCheckGetChar (hUart))
	{
		cc = uartGetChar (hUart) & 0x7F ;		// Chars are 7 bits

		switch (ticState)
		{
			// -------------------------------- Search of the TIC mode: historic or standard
			case ticSearchSTX:
				if (cc == CC_STX)
				{
					ticState = ticSearchLF ;
				}
				else
				{
					if ((aaGetTickCount () - ticStartTime) >= TIC_TMO)
					{
						// Timeout to receive STX, change mode
						uartClose (hUart) ;
						ticMode ^= 1u ;
						uartInit (LUART, (ticMode == MODE_HIS) ? BAUDRATE_HIS : BAUDRATE_STD, 0, & hUart) ;

						ticStartTime = aaGetTickCount () ;
					}
				}
				break ;

			case ticSearchLF:
				if (cc == CC_LF)
				{
					// Found a start of frame, now search for an ETX
					ticStartTime = aaGetTickCount () ;
					ticState = ticSearchETX ;
				}
				else
				{
					// This is not an LF, so search for STX again
					ticState = ticSearchSTX ;
				}
				break ;

			case ticSearchETX:
				if (cc == CC_ETX)
				{
					// The baud rate is OK, start to receive frames
					ticState = ticIdle ;
				}
				else
				{
					if ((aaGetTickCount () - ticStartTime) >= TIC_TMO)
					{
						// Timeout to receive ETX, change mode
						uartClose (hUart) ;
						ticMode ^= 1u ;
						uartInit (LUART, (ticMode == MODE_HIS) ? BAUDRATE_HIS : BAUDRATE_STD, 0, & hUart) ;

						ticStartTime = aaGetTickCount () ;
						ticState = ticSearchSTX ;
					}
				}
				break ;
			// ------------------------------------------------------------------------------

			case ticIdle:				// Wait for the start of the frame
				if (cc == CC_STX)
				{
					// Start of frame
					ticIndex = 0 ;
					ticState = ticSynchro ;
				}
				break ;

			case ticSynchro:			// Wait for the start of a group or the end of the frame
				if (cc == CC_LF)
				{
					// Start of group (LF is not stored)
					ticPtr = ticBuffer ;
					ticState = ticGroup ;
				}
				if (cc == CC_ETX)
				{
					// End of frame
					statusWSet (STSW_LINKY_ON) ;	// Valid frame received, so Linky ON
					ticState = ticIdle ;
				}
				break ;

			case ticGroup:				// Receive the group chars until CR
				if (cc == CC_CR)
				{
					uint32_t	crc ;
					char		* ptr ;

					// End of group (CR is not stored)
					* ticPtr = 0 ;		// End of the line
					ticPtr -= 2 ;		// Pointer to the separator before the CRC byte

					// Check the CRC, then use the received group
					ptr = ticBuffer ;
					crc = 0 ;
					while (ptr < ticPtr)
					{
						crc += * ptr++ ;
					}
					// The history mode CRC does not take into account the separator preceding the checksum.
					// The standard mode CRC includes the separator preceding the checksum.
					if (ticMode == MODE_STD)
					{
						crc += * ticPtr ;	// Standard mode: add the last separator
					}
					crc = (crc & 0x3F) + 0x20 ;
					if (crc != ticPtr [1])
					{
						// CRC error, cancel the current frame
//aaPuts ("CRC error\n") ;
						ticState = ticIdle ;
					}
					else
					{
						// The group is valid
						ticDecode () ;

						// Ready for the next group
						ticIndex++ ;
						ticState = ticSynchro ;
					}
				}
				else
				{
					if (ticPtr < & ticBuffer [TIC_GROUP_MAX-1])	// -1 for final 0
					{
						* ticPtr++ = cc ;
					}
					else
					{
						// Buffer overflow, cancel the current frame
						// Should never happen, the buffer size is sufficient for any group
						ticState = ticIdle ;
					}
				}
				break ;

			default:
				AA_ASSERT (0) ;
				break ;
		}
	}
}

//------------------------------------------------------------------------------
//	Register the user function

void	ticSetCallback (ticCallback_t pCallback)
{
	ticCallback = pCallback ;
}

//------------------------------------------------------------------------------
//	Toggle the group dump on the console

void		ticToggleDisplay	(void)
{
	ticDisplay = ! ticDisplay ;
}

//------------------------------------------------------------------------------
//	Initialize the Linky TIC package

void	ticInit (void)
{
	// Set the configuration to search for the TIC mode
	ticMode = MODE_STD ;
	uartInit (LUART, BAUDRATE_STD, 0, & hUart) ;
	ticState = ticSearchSTX ;
	ticStartTime = aaGetTickCount () ;

	meterPapp = 0 ;		// Valid values not yet received
	meterBase = 0 ;
	meterVolt = 0 ;

	ticTimeoutStart () ;
}

//------------------------------------------------------------------------------
