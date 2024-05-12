 /*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	wifiMsg.h	WIFI / AASUN messages

	When		Who	What
	25/03/24	ac	Creation

	This file is common to AASun and the WIFI interface on ESP32

----------------------------------------------------------------------
*/

#if ! defined WIFIMSG_H_
#define WIFIMSG_H_
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <time.h>		// For struct tm

#define	WIFIMSG_MAGIC		0x44332211
#define	WIFIMSG_PAGE_CHUNK	1024		// To send HTTP file system to the ESP32. Mandatory power of 2
#define	WIFIMSG_BBR			230400		// UART baud rate

// The size of the TX and RX UART buffers
#define		WBUF_POW2		11			// 2^WBUF_POW2 is WBUF_SIZE. Example WBUF_POW2 of 5 for 32 bytes, 11 for 2048 bytes
#define		WBUF_SIZE		(1 << WBUF_POW2)
#define		WBUF_MASK		(WBUF_SIZE-1)

// Values for wifiMsgHdr_t.msgId
#define	WM_ID_ACK			0			// Acknowledge answer, nothing to do. Data length 0
#define	WM_ID_NACK			1			// Not acknowledge answer, nothing to do. Data length 0
#define	WM_ID_SYNC			2			// Send by AASun to activate the WIFI HTTP server
#define	WM_ID_CGI			3			// HTTP request with CGI name and type
#define	WM_ID_CGI_RESP		4			// Response data to HTTP request with CGI
#define	WM_ID_ERROR_404		5			// Not found, data size 0
#define	WM_ID_GET_FS		6			// WIFI request for HTTP FS to write to flash. Message data is wifiPageMsg_t.
#define	WM_ID_FS			7			// Response to WM_ID_GET_FS. The message data is WIFIMSG_PAGE_CHUNK bytes.
#define	WM_ID_GET_INFO		8			// Request for AASun info, data length 0
#define	WM_ID_INFO			9			// Response to WM_ID_GET_INFO, wifiInfoMsg_t
#define	WM_ID_TELNET_START	10			// New Telnet connection, response is ACK or NACk
#define	WM_ID_TELNET_STOP	11			// Telnet connection closed, response is ACK
#define	WM_ID_TELNET		12			// Telnet data, from both side

#define	WM_ID_REQ			20			// Request from WIFI to AASun. Data length 0
#define	WM_ID_REQ_DATE		21			// Request of the current date from AASUN, also the response

typedef struct
{
	uint32_t	magic ;
	uint16_t	msgId ;
	uint16_t	dataLength ;
	uint8_t		message [0] ;

} wifiMsgHdr_t ;

static	const uint32_t	wifiHdrSize = sizeof (wifiMsgHdr_t) ;

static	const uint32_t	dataMessageMax = WBUF_SIZE - wifiHdrSize ;

//-----------------------------------------------------------------------------

// Values for wifiMsgCgi.type
#define	WM_TYPE_GET				1			// HTTP GET  CGI
#define	WM_TYPE_POST			2			// HTTP POST CGI

#define	WM_URINAME_MAX			32			// E.G. "setConfig.cgi"
#define	WM_CONTENT_MAX			32			// E.G. "application/json" or "application/octet-stream"

#define contentTypeJson	"application/json"

// This message is used as CGI request from ESP to AASun
typedef struct
{
	uint16_t	type ;				// WM_TYPE_GET or WM_TYPE_POST
	uint16_t	uriSize ;			// Length of the URI data
	char		uriName [WM_URINAME_MAX] ;
	char		uri [0] ;			// The URI data

} wifiReqMsgCgi_t ;
static	const uint32_t	wifiReqMsgCgiSize = sizeof (wifiReqMsgCgi_t) ;

// This message us used as CGI response of AASun to ESP request
typedef struct
{
	uint32_t	respSize ;			// Length of the response data
	char		contentType [WM_CONTENT_MAX] ;
	char		resp [0] ;			// The response data

} wifiRespMsgCgi_t ;
static	const uint32_t	wifiRespMsgCgiSize = sizeof (wifiRespMsgCgi_t) ;

// The request from WIFI to AASun
typedef struct
{
	uint32_t	version ;	// The version of the WIFI application
	uint32_t	softAP ;	// True if the WIFI is in Access Point mode

} wifiGetInfoMsg_t ;
static	const uint32_t	wifiGetInfoMsgSize = sizeof (wifiGetInfoMsg_t) ;

// The answer of AAsun to WM_ID_GET_INFO
typedef struct
{
	uint32_t	ipAddress ;
	uint32_t	ipMask ;
	uint32_t	ipGw ;
	uint32_t	dns1 ;
	uint32_t	dns2 ;


	uint32_t	fsSize ;
	uint32_t	fsCRC ;

} wifiInfoMsg_t ;
static	const uint32_t	wifiInfoMsgSize = sizeof (wifiInfoMsg_t) ;

typedef struct
{
	uint32_t	offset ;
	uint32_t	size ;

} wifiPageMsg_t ;
static	const uint32_t	wifiPageMsgSize = sizeof (wifiPageMsg_t) ;

typedef struct
{
	struct tm timeinfo ;

} wifiDateMsg_t ;
static	const uint32_t	wifiDateMsgSize = sizeof (wifiDateMsg_t) ;

//-----------------------------------------------------------------------------
#endif	// WIFIMSG_H_
