 /*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	wifi.h		Communication with ESP32 (WIFI)

	When		Who	What
	20/03/24	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined WIFI_H_
#define WIFI_H_
//-----------------------------------------------------------------------------

#include	"ringbuffer.h"

// The states of the Telnet protocol state machine
typedef enum
{
	TN_ST_IDLE		= 0,	// Idle
	TN_ST_IAC1,				// 1st IAC received
	TN_ST_CMD,				// Command received, only 1 byte to receive
	TN_ST_SB,				// SB received, waiting for IAC
	TN_ST_IAC2				// 2nd IAC received, waiting for SE

} tnProtoState_t ;

// The descriptor for the console "telnet" server
typedef struct
{
	int8_t			sn ;			// W5500 Socket number
	uint32_t		flags ;
	tnProtoState_t	tnState ;

	// Buffers management
	ringBuf_t		txBuffer ;		// Output from the application to the LAN interface
	ringBuf_t		rxBuffer ;		// Input of the application from the LAN interface

	aaDriverDesc_t	txWriteList ;	// The list of tasks waiting to write to  the TX buffer
	aaDriverDesc_t	rxReadList ;	// The list of tasks waiting to read from the RX buffer
	aaDriverDesc_t	rxWriteList ;	// The list of tasks waiting to write to  the RX buffer

} telnetDesc_t ;

//-----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// In wizlan.c
extern	telnetDesc_t	telnetDesc ;
extern	bool			bTelneInUse ;	// True if Telnet is in use
extern	bool			bWifiTelnet	;	// True if WIFI Telnet in use, else W5500 Telnet

extern	void			telnetSwitchOn	(void) ;
extern	void			telnetSwitchOff (void) ;

//-----------------------------------------------------------------------------

void	wifiInit			(void) ;
void	wifiNext			(void) ;

bool	wifiDateRequest		(void) ;

// In withlan.c
void	telnetSetFlag		(telnetHandle_t hTelnet, uint32_t flag, uint32_t bSet) ;
void	telnetGetFlag		(telnetHandle_t hTelnet, uint32_t * pFlag) ;

#ifdef __cplusplus
}
#endif
//-----------------------------------------------------------------------------
#endif	// WIFI_H_
