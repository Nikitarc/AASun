 /*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	wizLan.c	Interface to Wiznet W5500

	When		Who	What
	09/03/23	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined WIZLAN_H_
#define WIZLAN_H_
//-----------------------------------------------------------------------------
#include "sntp.h"
#include "util.h"

typedef	void *	sockHandle_t ;

// These flags can be used with wSockInit(), wSockSetFlag() and wSockGetFlag()
#define	WSOCK_FLAG_MASK		0xFFu
#define	WSOCK_FLAG_ECHO		0x01u		// Echo received characters
#define	WSOCK_FLAG_LFCRLF	0x02u		// On TX, convert LF to CR-LF

// To register the lan configuration in EEPROM
typedef struct lanCfg_t
{
	uint8_t		ip  [4] ;	// Source IP Address
	uint8_t		sn  [4] ;	// Subnet Mask
	uint8_t		gw  [4] ;	// Gateway IP Address
	uint8_t		dns [4] ;	// DNS server IP Address
	uint8_t		mac [6] ;	// Source Mac Address		TODO UNUSED
	uint16_t	dhcp;		// 1 - Static, 2 - DHCP

} lanCfg_t ;

//-----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// Telnet console
uint32_t		wCheckPutChar		(sockHandle_t hSock) ;
void 			wPutChar			(sockHandle_t hSock, char cc) ;
uint32_t		wCheckGetChar		(sockHandle_t hSock) ;
char 			wGetChar			(sockHandle_t hSock) ;
const uint8_t *	wGetMacAddress		(void) ;

// LAN configuration
void			wizSetCfg			(lanCfg_t * pCfg) ;
void			wizGetCfg			(lanCfg_t * pCfg) ;

// DNS utility
typedef void (* dnsCb_t) (int32_t result, uint8_t * ip) ;

uint32_t		dnsRequest			(uint32_t server, const char * domainName, dnsCb_t dnsCb) ;

// SNTP utility
typedef void (* sntpCb_t) (int32_t result, datetime * pDateTime) ;

uint32_t		sntpRequest			(uint8_t * serverIp, uint8_t tz, sntpCb_t sntpCb) ;
void			sntpNext			(void) ;
uint32_t		sntpDateToSecond	(localTime_t * pTime) ;
uint32_t		sntpSecondToWd		(uint32_t seconds) ;

// Miscellaneous
void			lowProcessesInit		(void) ;
void			displayYesterdayHisto	(uint32_t mode, uint32_t rank) ;

#ifdef __cplusplus
}
#endif

//-----------------------------------------------------------------------------
#endif 	// WIZLAN_H_
