 /*
----------------------------------------------------------------------

	Alain Chebrou

	wizLan.c	Interface to Wiznet W5500, Provide:
					A simple telnet server, and use is as standard I/O
					A small HTTP server with FLASH file system
					Access to DNS and SNTP
				Manage temperature sensors

	When		Who	What
	09/03/23	ac	Creation
	06/09/23	ac	Check W5500 version register: allows to test if the chip is physically present
	20/03/24	ac	Add ESP32 communication to WIFI



	BEWARE: In the W5500 ioLibrary the close() function collide with the C standard library one.
	So it is renamed closesocket()
----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aaprintf.h"
#include	"aatimer.h"

#include	"socket.h"
#include	"wizchip_conf.h"
#include	"httpServer.h"
#include	"dns.h"
#include	"sntp.h"

#include	"AASun.h"
#include	"spi.h"
#include	"wizLan.h"
#include	"wifi.h"
#include	"util.h"		// For getMacAddress()

#include	"mfs.h"
#include	"w25q.h"
#include	"temperature.h"

#include	"stdbool.h"
#include	"string.h"

#include	"aautils.h"

static void	telnetFlush (void) ;

static	int8_t			mfsOk ;		// MFS_ENONE if the file system is mounted

//--------------------------------------------------------------------------------

#define	w5500Spi		SPI2		// For W5500 only

#define	SPI_W5500_BRDIV	LL_SPI_BAUDRATEPRESCALER_DIV4

#define	CS_PORT			GPIOC		// W5500 chip select
#define	CS_PIN			6

#define	RST_PORT		GPIOC		// W5500 hard reset
#define	RST_PIN			7

// All this is resolved into constants at compile time to get the minimal code.
// BSRR register: low half word to set (high) the GPIO, high half word to reset (low) the GPIO

#define	csSel()		CS_PORT->BSRR = (1u << (CS_PIN + 16))	// CS set active (CS is active low)
#define	csDesel()	CS_PORT->BSRR = (1u << CS_PIN)

#define	rstOff()	RST_PORT->BSRR = (1u << RST_PIN)		// Reset is active low
#define	rstOn()		RST_PORT->BSRR = (1u << (RST_PIN + 16))

//--------------------------------------------------------------------------------
// Socket error message
// Errors are negative, so negate the value before use as index in this array

const char * socketErrorMsg [] =
{
	"SOCKERR_OK",
	"SOCKERR_SOCKNUM",
	"SOCKERR_SOCKOPT",
	"SOCKERR_SOCKINIT",
	"SOCKERR_SOCKCLOSED",
	"SOCKERR_SOCKMODE",
	"SOCKERR_SOCKFLAG",
	"SOCKERR_SOCKSTATUS",
	"SOCKERR_ARG",
	"SOCKERR_PORTZERO",
	"SOCKERR_IPINVALID",
	"SOCKERR_TIMEOUT",
	"SOCKERR_DATALEN",
	"SOCKERR_BUFFER",
} ;

//--------------------------------------------------------------------------------
// Display history from flash

void	displayYesterdayHisto (uint32_t mode, uint32_t inRank)
{
	uint32_t	ii ;
	powerH_t	histo ;
	int			hh, mm ;
	static bool		bDdisplayYesterdayHistoEnable = false ;
	static uint32_t	rank ;

	if (mode == 0)
	{
		if (histoCheckRank (inRank, NULL))
		{
			bDdisplayYesterdayHistoEnable = true ;
			rank = inRank ;
		}
		else
		{
			aaPrintf ("Invalid history rank: %u\n", inRank) ;
		}
		return ;
	}

	if (bDdisplayYesterdayHistoEnable)
	{
		// Display yesterday history from flash
		bDdisplayYesterdayHistoEnable = false ;
		hh = mm = 0 ;

		// Read the 1st record that contain the date
		histoPowerRead (& histo, rank, 0, sizeof (powerH_t)) ;
		aaPrintf ("%4d/%02d/%02d\n",
					histo.exported >> 16,
					(histo.exported >> 8) & 0xFF,
					histo.exported & 0xFF) ;

		// Read the history skipping the 1st record (header)
		for (ii = 1 ; ii < POWER_HISTO_MAX_WHEADER ; ii++)
		{
			histoPowerRead (& histo, rank, ii * sizeof (powerH_t), sizeof (powerH_t)) ;
			aaPrintf (" %02d:%02d  %9d %9d %9d %9d"
#if (defined IX_I3)
					" %9d"
#endif
#if (defined IX_I4)
					" %9d"
#endif
					" %9d %9d\n",
				hh, mm,
				histo.imported, histo.exported,
				histo.diverted1, histo.power2,
#if (defined IX_I3)
				histo.power3,
#endif
#if (defined IX_I4)
				histo.power4,
#endif
				histo.powerPulse [0], histo.powerPulse [0]) ;
			mm += 15 ;
			if (mm == 60)
			{
				mm  = 0 ;
				hh++ ;
			}

			// Avoid Telnet TX buffer overflow
			if ((ii & 3) == 0)
			{
				telnetFlush () ;
			}
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Functions used by Wiznet ioLibrary to access the SPI
//	No function for critical section. The ioLibrary use of CR is rudimentary and not compatible with our application

void	wiz_cs_sel (void)
{
	csSel () ;		// CS LOW
}

//--------------------------------------------------------------------------------

void	wiz_cs_desel (void)
{
	csDesel () ;	// CS HIGH
}

//--------------------------------------------------------------------------------
// Get 1 byte from the SPI

uint8_t	wiz_spi_rbyte (void)
{
	while ((w5500Spi->SR & SPI_SR_TXE) == 0u)
	{
	}

	* ((__IO uint8_t *) & w5500Spi->DR) = (uint8_t) 0u ;

	while ((w5500Spi->SR & SPI_SR_RXNE) == 0u)
	{
	}

	return * ((__IO uint8_t *) & w5500Spi->DR) ;
}

//--------------------------------------------------------------------------------
// Send 1 byte to the SPI

void	wiz_spi_wbyte (uint8_t byte)
{
//	volatile uint8_t rd ;

	while ((w5500Spi->SR & SPI_SR_TXE) == 0u)
	{
	}

	* ((__IO uint8_t *) & w5500Spi->DR) = byte ;

	while ((w5500Spi->SR & SPI_SR_RXNE) == 0u)
	{
	}

	volatile uint8_t  rd = * ((__IO uint8_t *) & w5500Spi->DR) ;
	(void) rd ;
}

//--------------------------------------------------------------------------------
// Get a buffer from the SPI

void	wiz_spi_rburst (uint8_t * pBuf, uint16_t len)
{
	for (uint32_t ii = 0 ; ii < len ; ii++)
	{
		pBuf [ii] = wiz_spi_rbyte () ;
	}
}

//--------------------------------------------------------------------------------
// Send a buffer to the SPI

void	wiz_spi_wburst (uint8_t * pBuf, uint16_t len)
{
	if (len > 3)
	{
		spiW5500WriteDmaXfer (len, pBuf) ;
	}
	else
	{
		for (uint32_t ii = 0 ; ii < len ; ii++)
		{
			wiz_spi_wbyte (pBuf [ii]) ;
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

// Socket Buffer Size Register are values 2^n in kByte (total <= 16 kByte)
static const uint8_t		sockBufferSize [8] = { 2, 2, 2, 2, 2, 2, 2, 2 } ;

// DATA_BUF_SIZE defined in httpServer.h
// wizBuffers is also used by SerEL.c
static	uint8_t	wizBuffer [DATA_BUF_SIZE * 2] ;
static	uint8_t * const wizRxBuf = & wizBuffer [0] ;
static	uint8_t * const wizTxBuf = & wizBuffer [DATA_BUF_SIZE] ;

static	aaTimerId_t		wiz1secTimerHandle ;

extern	mfsCtx_t	wMfsCtx ;	// MFS file system context for http server

/*
// The default configuration when the EEPROM configuration is not available
static const wiz_NetInfo	netInfo =
{		.mac  = { 0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
		.ip   = { 192, 168,   1, 130 },		// IP address
		.sn   = { 255, 255, 255,   0 },		// Subnet mask
		.gw   = { 192, 168,   1, 254 },		// Gateway address
		.dns  = {   1,   1,   1,   1 },		// Domain Name Server
		.dhcp = NETINFO_STATIC				// NETINFO_STATIC or NETINFO_DHCP
} ;
*/

static wiz_NetInfo	netConfiguration ;

// --------------------------- Socket numbers
#define	TELNET_SOCK_NUM		0
#define	TELNET_PORT			23

#define	DNS_SOCK_NUM		1
#define	SNTP_SOCK_NUM		2

#define	HTTP_SOCK_MAX		4
static const uint8_t		httpSocknumlist [HTTP_SOCK_MAX] = { 4, 5, 6 ,7 } ;
// ---------------------------

// Sizes of telnet intermediate buffers: power of 2 is mandatory
#define SOCK_TX_SIZE		1024
#define	SOCK_RX_SIZE		512

#define	WLAN_TASK_PRIORITY	BSP_IRQPRIOMIN_PLUS(1)

//--------------------------------------------------------------------------------

void		wizSetCfg			(lanCfg_t * pCfg)
{
	memcpy (netConfiguration.ip,  & pCfg->ip,  sizeof (pCfg->ip)) ;
	memcpy (netConfiguration.sn,  & pCfg->sn,  sizeof (pCfg->sn)) ;
	memcpy (netConfiguration.gw,  & pCfg->gw,  sizeof (pCfg->gw)) ;
	memcpy (netConfiguration.dns, & pCfg->dns, sizeof (pCfg->dns)) ;
	netConfiguration.dhcp = pCfg->dhcp ;
}

//--------------------------------------------------------------------------------

void		wizGetCfg			(lanCfg_t * pCfg)
{
	memcpy (& pCfg->ip,  netConfiguration.ip,  sizeof (pCfg->ip)) ;
	memcpy (& pCfg->sn,  netConfiguration.sn,  sizeof (pCfg->sn)) ;
	memcpy (& pCfg->gw,  netConfiguration.gw,  sizeof (pCfg->gw)) ;
	memcpy (& pCfg->dns, netConfiguration.dns, sizeof (pCfg->dns)) ;
	pCfg->dhcp = netConfiguration.dhcp ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

enum
{
	TN_IAC		= 255,
	TN_SB		= 250,
	TN_SE		= 240
} ;

// The descriptor for the console "telnet" server

telnetDesc_t	telnetDesc =
{
	0,					// sn
	0,					// flags
	TN_ST_IDLE,

	{ 0 },				// txBuffer
	{ 0 },				// rxBuffer

	{ { NULL, NULL } },	// txWriteList
	{ { NULL, NULL } },	// rxReadList
	{ { NULL, NULL } },	// rxWriteList
} ;

STATIC	uint8_t		wcTxBuffer [SOCK_TX_SIZE] ;
STATIC	uint8_t		wcRxBuffer [SOCK_RX_SIZE] ;

// Descriptor to set the socket as standard I/O
static	aaDevIo_t	wSockIo ;

static	uint32_t	tnProtocol (telnetDesc_t * pTnDesc) ;

bool				bTelneInUse ;	// True if Telnet is in use
bool				bWifiTelnet	;	// True if WIFI Telnet in use, else W5500 Telnet

//--------------------------------------------------------------------------------

uint8_t *		getWizBuffer	(void)
{
	return wizBuffer ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Returns the connected state of the telnet socket

static	bool	wCheckConnected (telnetDesc_t * pTnDesc)
{
	uint8_t		tmp ;
	bool		bConnected = true ;

	// check for link down
	tmp = getPHYCFGR () ;
	if ((tmp & PHYCFGR_LNK_ON) == 0)
	{
		// Link down
		closesocket (pTnDesc->sn) ;
		bConnected = false ;
	}
	else
	{
		// Check for disconnection
		tmp = getSn_SR (pTnDesc->sn) ;
		if (tmp != SOCK_ESTABLISHED)
		{
			if (tmp == SOCK_CLOSE_WAIT)
			{
				// Close initiated by the client
				disconnect (pTnDesc->sn) ;	// This closes the socket
				bConnected = false ;
			}
			else
			{
				closesocket (pTnDesc->sn) ;
				bConnected = false ;
			}
		}
	}
	return bConnected ;	// True if connected
}

//--------------------------------------------------------------------------------
// Copy data from the W5500 socket buffer to the RX ring buffer
// Copy data using iRxWrite (write to the RX ring buffer)

static	uint32_t	wRecv (telnetDesc_t * pTnDesc)
{
	uint32_t	freeSize ;		// Free size in receive buffer
	uint32_t	dataSize ;		// Data length to read in the socket
	uint32_t	len ;
	uint32_t	chunkSize ;

	freeSize = rbGetWriteCount (& pTnDesc->rxBuffer) ;
	dataSize = getSn_RX_RSR (pTnDesc->sn) ;	// Amount of data available in the socket

	if (dataSize > freeSize)
	{
		dataSize = freeSize ;
	}
	if (dataSize == 0u)
	{
		return 0 ;	// Nothing to read
	}
	// Now dataSize is the length to read from the socket

	chunkSize = rbWriteChunkSize (& pTnDesc->rxBuffer) ;
	if (chunkSize >= dataSize)
	{
		// It can be read at once
		len = recv (pTnDesc->sn, ( uint8_t *) rbGetWritePtr (& pTnDesc->rxBuffer), dataSize) ;
		rbAddWrite (& pTnDesc->rxBuffer, dataSize) ;
if (len != dataSize)
{
	aaPuts ("wRecv error\n") ;
	aaTaskDelay (10) ;
	AA_ASSERT (0) ;
}
	}
	else
	{
		// Wrap at the buffer end: write 1st part until the buffer end
		len = recv (pTnDesc->sn, ( uint8_t *) rbGetWritePtr (& pTnDesc->rxBuffer), chunkSize) ;
		rbAddWrite (& pTnDesc->rxBuffer, chunkSize) ;

		// Write the 2nd part (at the beginning of the buffer)
		len += recv (pTnDesc->sn, ( uint8_t *) rbGetWritePtr (& pTnDesc->rxBuffer), dataSize - chunkSize) ;
		rbAddWrite (& pTnDesc->rxBuffer, dataSize - chunkSize) ;
if (len != dataSize)
{
	aaPuts ("wRecv error\n") ;
	aaTaskDelay (10) ;
	AA_ASSERT (0) ;
}
	}

	// If some task is waiting to receive awake it
	aaIoResumeWaitingTask (& pTnDesc->rxReadList) ;

	return dataSize ;
}

//--------------------------------------------------------------------------------
// Copy data from the TX ring buffer to the W5500 socket buffer
// Copy data using iTxRead (read from the TX ring buffer)

static	uint32_t	wSend (telnetDesc_t * pTnDesc)
{
	uint32_t	dataSize ;		// Data length to write in the socket
	uint32_t	toSendSize ;
	int32_t		sentSize ;
	int32_t		len ;
	uint32_t	chunkSize ;

	dataSize   = getSn_TX_FSR (pTnDesc->sn) ;	// free space in socket TX buffer
	toSendSize = rbGetReadCount (& pTnDesc->txBuffer) ;

	if (dataSize > toSendSize)
	{
		dataSize = toSendSize ;
	}
	if (dataSize == 0u)
	{
		return 0 ;	// Nothing to write
	}
	// Now dataSize is the length to write to the socket

	sentSize = 0 ;
	// We cannot have 2 send in a row: the second returns an emitted length of 0.
	while (dataSize > 0)
	{
		chunkSize = rbReadChunkSize (& pTnDesc->txBuffer) ;
		if (chunkSize >=  dataSize)
		{
			chunkSize = dataSize ;
		}
		len = send (pTnDesc->sn, (uint8_t *) rbGetReadPtr (& pTnDesc->txBuffer), chunkSize) ;
if (len < 0)
{
	AA_ASSERT (0) ;
}
		dataSize -= len ;
		sentSize += len ;
		rbAddRead (& pTnDesc->txBuffer, len) ;
	}

	// If some task is waiting to transmit awake it
	aaIoResumeWaitingTask (& pTnDesc->txWriteList) ;

	return sentSize ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	wLan standard IO
//--------------------------------------------------------------------------------
// To check a flag in the descriptor flags field

__STATIC_INLINE	uint32_t wCheckFlag (telnetDesc_t * pTnDesc, uint32_t flag)
{
	return ((pTnDesc->flags & flag) == 0u) ? 0u : 1u ;
}

//--------------------------------------------------------------------------------
//	Set a flag to telnet descriptor

void	telnetSetFlag		(telnetHandle_t hTelnet, uint32_t flag, uint32_t bSet)
{
	telnetDesc_t 	* pTnDesc = (telnetDesc_t *) hTelnet ;

	if (bSet == 0u)
	{
		pTnDesc->flags &= ~flag ;
	}
	else
	{
		pTnDesc->flags |= flag ;
	}
}

//-----------------------------------------------------------------------------
//	Get a flag from telnet descriptor

void	telnetGetFlag		(telnetHandle_t hTelnet, uint32_t * pFlag)
{
	telnetDesc_t 	* pTnDesc = (telnetDesc_t *) hTelnet ;

	* pFlag = pTnDesc->flags ;
}

//--------------------------------------------------------------------------------
//	To build a descriptor to use this telnet as standard input/output

void	wInitStdDev (aaDevIo_t * pDev, telnetHandle_t hTelnet)
{
	pDev->hDev				= hTelnet ;
	pDev->getCharPtr		= wGetChar ;
	pDev->putCharPtr		= wPutChar ;
	pDev->checkGetCharPtr	= wCheckGetChar ;
	pDev->checkPutCharPtr	= wCheckPutChar ;
}

//--------------------------------------------------------------------------------

void	telnetSwitchOn (void)
{
	aaPuts ("Switch to Telnet\n") ;
	telnetSetFlag ((telnetHandle_t) & telnetDesc, WSOCK_FLAG_LFCRLF, 1) ;	// Convert \n to \r\n
	rbReset (& telnetDesc.txBuffer) ;
	rbReset (& telnetDesc.rxBuffer) ;

	aaCriticalEnter () ;
	aaInitStdIn	 (& wSockIo) ;				// Use this devIo for input
	aaInitStdOut (& wSockIo) ;				// Use this devIo for output
	aaCriticalExit () ;

	// Request the client to not local echo (for MS Telnet)
	aaPuts ("\xff\xfd\x2d") ;	// IAC DO SUPPRESS-LOCAL-ECHO
	aaPuts ("Connected\n") ;
	bTelneInUse = true ;
}

//--------------------------------------------------------------------------------
// telnet connexion closed, switch to UART console

void	telnetSwitchOff (void)
{
// TODO: Flush waiting tasks ?
	aaDevIo_t	* hDevice = aaGetDefaultConsole () ;

	if (hDevice != NULL)
	{
		// Restore the default console
		aaInitStdIn	 (hDevice) ;
		aaInitStdOut (hDevice) ;
	}
	else
	{
		// No default console !!!!
		aaInitStdIn	 (NULL) ;
		aaInitStdOut (NULL) ;
	}
	aaPuts ("Switch to UART\n") ;
	bTelneInUse = false ;
}

//--------------------------------------------------------------------------------
// Get count of char free space to write to dev without blocking

uint32_t	wCheckPutChar		(telnetHandle_t hTelnet)
{
	telnetDesc_t 	* pTnDesc = (telnetDesc_t *) hTelnet ;

	return rbGetWriteCount (& pTnDesc->txBuffer) ;
}

//--------------------------------------------------------------------------------
// Put one char to device TX FIFO, blocking if FIFO full

void 		wPutChar			(telnetHandle_t hTelnet, char cc)
{
	telnetDesc_t 		* pTnDesc = (telnetDesc_t *) hTelnet ;
	uint32_t		flagLF = 0u ;

	// If cc is LF, then maybe send CR-LF
	if (wCheckFlag (pTnDesc, WSOCK_FLAG_LFCRLF) != 0u  &&  cc == '\n')
	{
		flagLF = 1u ;
		cc = '\r' ;
	}

	while (1)
	{
		while (1)
		{
			aaCriticalEnter () ;
			if (rbGetWriteCount (& pTnDesc->txBuffer) == 0)
			{
				// Buffer is full, wait. The critical section will be released by aaIoWait().
				(void) aaIoWait (& pTnDesc->txWriteList, 0u, 0u) ;	// Not ordered, no timeout
			}
			else
			{
				break ;		// Not full. Note: exit while inside the critical section
			}
		}

		rbPut (& pTnDesc->txBuffer, cc) ;
		aaCriticalExit () ;

		if (flagLF != 0u)
		{
			flagLF = 0u ;
			cc = '\n'  ;
			continue ;
		}
		break ;
	}
}

//-----------------------------------------------------------------------------
//	Returns the count of available characters in RX buffer
//	Actually if at least 1 char is available

uint32_t	wCheckGetChar (telnetHandle_t hTelnet)
{
	telnetDesc_t 	* pTnDesc = (telnetDesc_t *) hTelnet ;

	return tnProtocol (pTnDesc) ;
}

//--------------------------------------------------------------------------------
// Get one char from device RX FIFO, blocking if no char available

char 		wGetChar			(telnetHandle_t hTelnet)
{
	telnetDesc_t 	* pTnDesc = (telnetDesc_t *) hTelnet ;
	char		cc ;

	while (1)
	{
		aaCriticalEnter () ;
		if (rbGetReadCount (& pTnDesc->rxBuffer) == 0)
		{
			// Buffer is empty. aaIoWait() will release the critical section
			(void) aaIoWait (& pTnDesc->rxReadList, 0u, 0u) ;	// Not ordered, no timeout
		}
		else
		{
			if (0 != tnProtocol (pTnDesc))
			{
				break ;	// Not empty. Note: exit while inside critical section
			}
		}
	}

	rbGet (& pTnDesc->rxBuffer, & cc) ;
	aaCriticalExit () ;

	// If some task is waiting to write to the RX buffer awake it
	aaIoResumeWaitingTask (& pTnDesc->rxWriteList) ;

	return cc ;
}

//--------------------------------------------------------------------------------
//	Send a block of data

void	wPutString (telnetHandle_t hTelnet, char * pStr)
{
	while (* pStr != 0)
	{
		wPutChar (hTelnet, * pStr) ;
		pStr++ ;
	}

	// TODO ? To flush the buffer: awake the lan task
}

//-----------------------------------------------------------------------------
// Manage (eat) Telnet protocol. See http://www.tcpipguide.com/free/t_TelnetOptionsandOptionNegotiation-4.htm
// Returns the count or real char available

static	uint32_t tnProtocol (telnetDesc_t * pTnDesc)
{
	uint32_t	count ;
	bool		bEnd = false ;
	char		cc ;

	if (bWifiTelnet)
	{
		// No need of protocol for WIFI Telnet (it is managed by the ESP32)
		return rbGetReadCount (& pTnDesc->rxBuffer) ;
	}

	aaCriticalEnter () ;

	while (! bEnd)
	{
		if (rbGetReadCount (& pTnDesc->rxBuffer) == 0)
		{
			// No char available
			count = 0 ;
			break ;
		}
		else
		{
			rbPeek (& pTnDesc->rxBuffer, & cc) ;
			switch (pTnDesc->tnState)
			{
				case TN_ST_IDLE:	// Idle
					if (cc == TN_IAC)
					{
						rbGet (& pTnDesc->rxBuffer, & cc) ;
						pTnDesc->tnState = TN_ST_IAC1 ;
					}
					else
					{
						count = rbGetReadCount (& pTnDesc->rxBuffer) ;
						bEnd = true ;
					}
					break ;

				case TN_ST_IAC1:	// 1st IAC received, waiting for the command
					if (cc == TN_IAC)
					{
						// Double IAC => this is a standard char
						pTnDesc->tnState = TN_ST_IDLE ;
						count = rbGetReadCount (& pTnDesc->rxBuffer) ;
						bEnd = true ;
					}
					else
					{
						rbGet (& pTnDesc->rxBuffer, & cc) ;
						if (cc == TN_SB)
						{
							pTnDesc->tnState = TN_ST_SB ;
						}
						else
						{
							pTnDesc->tnState = TN_ST_CMD ;
						}
					}
					break ;

				case TN_ST_CMD:		// Command received, waiting for the value
					rbGet (& pTnDesc->rxBuffer, & cc) ;
					pTnDesc->tnState = TN_ST_IDLE ;	// End of command
					break ;

				case TN_ST_SB:		// SB received, waiting for IAC
					rbGet (& pTnDesc->rxBuffer, & cc) ;
					if (cc == TN_IAC)
					{
						pTnDesc->tnState = TN_ST_IAC2 ;
					}
					break ;

				case TN_ST_IAC2:	// 2nd IAC received, waiting for SE
					rbGet (& pTnDesc->rxBuffer, & cc) ;
					if (cc == TN_IAC)
					{
						// Double IAC => this is a sub negotiation data
						pTnDesc->tnState = TN_ST_SB ;
					}
					if (cc == TN_SE)
					{
						// End of sub negotiation data
						pTnDesc->tnState = TN_ST_IDLE ;
					}
					else
					{
						AA_ASSERT (0) ;		// Abnormal case
					}
					break ;

				default:
					AA_ASSERT (0) ;
					break ;
			}
		}
	}
	aaCriticalExit () ;
	return count ;
}

//--------------------------------------------------------------------------------
//	Telnet state machine for wired LAN interface (W5500)

typedef enum
{
	tnNotInit = 0,
	tnOpen,
	tnListen,
	tnConnect,
	tnXfer,

} tnW5500State_t ;

static	tnW5500State_t	telnetW5500State ;	// Default state tnNotInit

void	telnetNext ()
{
	switch (telnetW5500State)
	{
		case tnNotInit:

			telnetDesc.flags    = 0 ;
			rbInit (& telnetDesc.txBuffer, SOCK_TX_SIZE, wcTxBuffer) ;
			rbInit (& telnetDesc.rxBuffer, SOCK_RX_SIZE, wcRxBuffer) ;
			aaIoDriverDescInit (& telnetDesc.txWriteList) ;
			aaIoDriverDescInit (& telnetDesc.rxReadList) ;
			aaIoDriverDescInit (& telnetDesc.rxWriteList) ;

			// Initialize the wLan I/O descriptor
			wInitStdDev  (& wSockIo, (telnetHandle_t) & telnetDesc) ;	// Initialize the devIo with telnet functions
			telnetW5500State = tnOpen ;
			break ;

		case tnOpen:
			// Open socket
			telnetDesc.sn = socket (TELNET_SOCK_NUM, Sn_MR_TCP, TELNET_PORT, 0) ;
			if (telnetDesc.sn < 0)
			{
				aaPuts ("socket: " ) ;
				aaPuts (socketErrorMsg [-telnetDesc.sn]) ;
				aaPutChar('\n') ;
				aaTaskDelay (100) ;
				AA_ASSERT (0) ;
			}
			telnetW5500State = tnListen ;
			break ;

		case tnListen:
			// Set socket in listen state
			if (listen (telnetDesc.sn) == SOCK_OK)
			{
				telnetW5500State = tnConnect ;
			}
			break ;

		case tnConnect:
			// Waiting connection
			if (getSn_SR (telnetDesc.sn) == SOCK_ESTABLISHED)
			{
				if (bTelneInUse)
				{
					// WIFI Telnet already in use, close the connection
					closesocket (telnetDesc.sn) ;
					telnetW5500State = tnOpen ;
				}
				else
				{
					// Connection established: switch to telnet console
					telnetSwitchOn () ;
					bWifiTelnet = false ;
					telnetW5500State = tnXfer ;
				}
			}
			break ;

		case tnXfer:
			// Something to receive ?
			wRecv (& telnetDesc) ;

			// Something to send ?
			wSend (& telnetDesc) ;

			// Check socket connected state
			if (false == wCheckConnected (& telnetDesc))
			{
				// Socket disconnected and possibly closed
				// Switch to UART console then go to open the socket
				telnetSwitchOff () ;
				telnetW5500State = tnOpen ;
			}
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//--------------------------------------------------------------------------------
//	If telnet is in use then flush the TX buffer

static	void	telnetFlush (void)
{
	if (bWifiTelnet == true)
	{
		// WIFI
// TODO
	}
	else
	{
		// W5500 wired LAN
		if (telnetW5500State == tnXfer)
		{
			while (rbGetReadCount (& telnetDesc.txBuffer) > 0)
			{
				telnetNext () ;
			}
		}
	}
}

//--------------------------------------------------------------------------------

const uint8_t *	wGetMacAddress		(void)
{
	return (const uint8_t *) netConfiguration.mac ;
}

//--------------------------------------------------------------------------------
//	The lan task
//	It manages only one socket for telnet server

// Define the task stack, to avoid use of dynamic memory
#define	WLAN_STACK_SIZE		400u
static	bspStackType_t		wLanStack [WLAN_STACK_SIZE] BSP_ATTR_ALIGN(8) BSP_ATTR_NOINIT ;

//--------------------------------------------------------------------------------

#include	"w25q.h"		// Flash

// The user provided function to set in the MFS context

static	int mfsDevRead (void * userData, uint32_t address, void * pBuffer, uint32_t size)
{
	W25Q_Read (pBuffer, address + (uint32_t) userData, size) ;
	return MFS_ENONE ;
}

static	void mfsDevLock (void * userData)
{
	(void) userData ;
	W25Q_SpiTake () ;
}

static	void mfsDevUnlock (void * userData)
{
	(void) userData ;
	W25Q_SpiGive () ;
}

//--------------------------------------------------------------------------------

// Wiznet 1 sec timer callback
uint32_t wizTimerCb (uintptr_t arg)
{
	(void) arg ;
	httpServer_time_handler () ;
	DNS_time_handler () ;
	return 1 ;		// Restart
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	DNS management

#define	DNS_ST_IDLE		0
#define	DNS_ST_WAIT1	1	// Waiting response from net configuration DNS
#define	DNS_ST_WAIT2	2	// Waiting response from alternate DNS

static	const uint8_t 	dnsAlternate [4] = { 208, 67, 222, 222 } ;	// OpenDNS

static	uint8_t		dnsState ;
static	dnsCb_t		dnsCbtoCall ;
static	const char	* name ;

// Server is 0 (primary), or 1 (alternate)
// The domainName must remain valid until the end of the operation
// If domainName is NULL, then use the memorized domainName pointer

uint32_t	timeDnsRequest (uint32_t server, const char * domainName, dnsCb_t dnsCb)
{
aaPrintf ("timeDnsRequest %u ", server) ;
	if (dnsState != DNS_ST_IDLE)
	{
aaPutChar ('\n') ;
		return 0 ;	// DNS not in idle state
	}
	if (domainName != NULL)
	{
		name = domainName ;
	}
	if (name [0] == 0)
	{
		aaPutChar ('\n') ;
		return 0 ;	// No domain name
	}
aaPuts (name) ; aaPutChar ('\n') ;

	dnsCbtoCall = dnsCb ;
	DNS_start ((server == 0) ? netConfiguration.dns : dnsAlternate, (const uint8_t *) name) ;
	dnsState = (server == 0) ? DNS_ST_WAIT1 : DNS_ST_WAIT2 ;
	// BEWARE: If the link is off, the DNS request is pending until the link is on.
	// If the link is on but the server is not reachable, the DNS request will get a timeout
	return 1 ;	// Success
}

static	void	dnsNext (void)
{
	int8_t		res ;
	uint8_t		ip [4] ;	// Translated IP address

	if (dnsState != DNS_ST_IDLE)
	{
		res = DNS_next (ip) ;	// DNS_next manages a timeout of 3 seconds
		if (res == 1)
		{
			// Success
			dnsState = DNS_ST_IDLE ;
			(* dnsCbtoCall) (0, ip) ;	// 0 is OK
		}
		else if (res < 0)
		{
			// Fail: return the faulty DNS 1:primary or 2:alternate
			uint32_t	result = (dnsState == DNS_ST_WAIT1) ? 1 : 2 ;
			dnsState = DNS_ST_IDLE ;
			(* dnsCbtoCall) (result, ip) ;
		}
		else
		{
			// Continue to wait
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	SNTP management

static	sntpCb_t	sntpCbToCall ;
static	uint8_t		sntpRunning ;

uint32_t	sntpRequest (uint8_t * serverIp, uint8_t tz, sntpCb_t sntpCb)
{
	if (sntpRunning == 0u)
	{
		sntpCbToCall = sntpCb ;
		SNTP_init (SNTP_SOCK_NUM, serverIp, tz, wizTxBuf);
		sntpRunning = 1u ;
		return 1 ;
	}
	return 0 ;
}

void	sntpNext (void)
{
	int8_t		res ;
	datetime 	time ;

	if (sntpRunning == 1u)
	{
		res = SNTP_run (& time) ;
		if (res == 1)
		{
			// Success
			(* sntpCbToCall) (0, & time) ;
			sntpRunning = 0u ;
		}
		else if (res == -1)
		{
			// Timeout
			(* sntpCbToCall) (-1, & time) ;
			sntpRunning = 0u ;
		}
		else
		{
		}
	}
}

//--------------------------------------------------------------------------------
// When the console link is used for SerEl (web page download) it is necessary to get exclusive use of this link.
// Setting bLowProcessEnabled to false disable the low process task, so it can't generate outputs

static	bool bLowProcessEnabled ;

void	enableLowProcesses	(bool enable)
{
	bLowProcessEnabled = enable ;
}

//--------------------------------------------------------------------------------
// This task handles the local network and other low priority processes

void	lowProcessesTask (uintptr_t arg)
{
	int8_t		tmp ;
	int8_t		seqnum ;

	(void) arg ;

	// Configure the SPI hardware
	spiInit (w5500Spi) ;
	wiz_cs_desel ()  ;
	spiW5500DmaInit () ;
	rstOff () ;
	aaTaskDelay (10) ;	// More than 1ms (RSTn to internal PLOCK)
	spiSetBaudRate (w5500Spi, SPI_W5500_BRDIV) ;

	// Provides the SPI functions to Wiznet ioLibrary
	reg_wizchip_cs_cbfunc		(wiz_cs_sel,     wiz_cs_desel) ;
	reg_wizchip_spi_cbfunc		(wiz_spi_rbyte,  wiz_spi_wbyte) ;
	reg_wizchip_spiburst_cbfunc	(wiz_spi_rburst, wiz_spi_wburst) ;
//	reg_wizchip_cris_cbfunc		(aaCriticalEnter, aaCriticalExit) ;	// Guard only SPI access => useless and too long critical section

	// Set sockets internal buffer size
	wizchip_init (sockBufferSize, sockBufferSize) ;

	// Get the MAC address from MCU unique ID
	getMacAddress (netConfiguration.mac) ;

	wizchip_setnetinfo (& netConfiguration) ;

	// Check if wizchip W5500 is physically present: read version register
	ctlwizchip(CW_GET_VERSIONR, (void *) & tmp);
	if (tmp == 0x04)
	{
		statusWSet (STSW_W5500_EN) ;
		aaPrintf ("W5500 present %02X\n", tmp & 0xFF) ;
	}
	else
	{
		aaPrintf ("No W5500 %02X\n", tmp & 0xFF) ;
	}


	// LAN cable connected ?
	statusWSet (STSW_NET_LINK_OFF) ;

	// For http server
	// Initialize the MFS context
	// userData is used to provide the starting address of the file system in the FLASH
	wMfsCtx.userData = (void *) 0x100000 ;
	wMfsCtx.lock     = mfsDevLock ;
	wMfsCtx.unlock   = mfsDevUnlock ;
	wMfsCtx.read     = mfsDevRead ;
	mfsOk = mfsMount (& wMfsCtx) ;
	if (mfsOk != MFS_ENONE)
	{
		aaPrintf ("mfsMount error: %d\n", mfsOk) ;
	}
	else
	{
		aaPrintf ("mfsMount Ok  Size:%u, Crc:0x%08X\n", wMfsCtx.fsSize, wMfsCtx.fsCRC) ;
	}

	httpServer_init (wizTxBuf, wizRxBuf, HTTP_SOCK_MAX, httpSocknumlist) ;
	reg_httpServer_cbfunc(NVIC_SystemReset, NULL);

	// Initialize net 1sec time handler
	{
		aaError_t	res ;
		res = aaTimerCreate	(& wiz1secTimerHandle) ;
		if (res != AA_ENONE)
		{
			aaPrintf ("aaTimerCreate error %u\n", res) ;
			aaTaskDelay (50) ;
			AA_ASSERT (0) ;
		}
		aaTimerSet   (wiz1secTimerHandle, wizTimerCb, 0, bspGetTickRate ()) ;
		aaTimerStart (wiz1secTimerHandle) ;
	}
/*
// Fort test get back chip info
wizchip_getnetinfo(& netInfoBack) ;
aaPuts ("MAC ") ;
for (uint32_t ii = 0 ; ii < 6 ; ii++)
{
	aaPrintf (" %02X", netConfiguration.mac [ii]) ;
}
aaPuts ("\nIP  ") ;
for (uint32_t ii = 0 ; ii < 4 ; ii++)
{
	aaPrintf (" %3d", netConfiguration.ip [ii]) ;
}
aaPutChar('\n') ;
*/
	// Initialize state machines
	telnetW5500State = tnNotInit ;
	telnetNext () ;		// To initialize the Telnet  descriptor

	dnsState = DNS_ST_IDLE ;
	sntpRunning = 0u ;
	seqnum = 0 ;

	DNS_init (DNS_SOCK_NUM, wizTxBuf) ;

	// Initialize communication with ESP32 that handle WIFI
	wifiInit () ;

	// This loop manage the Telnet connections (wired and WIFI), HTTP server, DNS request, SNTP, temperature sensors, etc
	while (1)
	{
		aaTaskDelay (3) ;
		if (bLowProcessEnabled == false)
		{
			continue ;
		}

		tmp = 0 ;
		ctlwizchip (CW_GET_PHYLINK, (void *) & tmp) ;
		if (tmp == PHY_LINK_ON)
		{
if (statusWTest (STSW_NET_LINK_OFF)) aaPrintf ("STSW_NET_LINK_ON\n") ;	// Print only once
			statusWClear (STSW_NET_LINK_OFF) ;

			telnetNext () ;
			dnsNext () ;
			sntpNext () ;

			// If the file system is mounted, then manage HTTP
			if (mfsOk == MFS_ENONE)
			{
				httpServer_run (seqnum++) ;
				if (seqnum == HTTP_SOCK_MAX)
				{
					seqnum = 0 ;
				}
			}
		}
		else
		{
if (! statusWTest (STSW_NET_LINK_OFF)) aaPrintf ("STSW_NET_LINK_OFF\n") ;	// Print only once
			statusWSet (STSW_NET_LINK_OFF) ;
		}

		wifiNext () ;

		tempSensorNext () ;

		displayYesterdayHisto (1, 0) ;
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Create the task for local network and other low priority processes
// With a low priority: mandatory lower than the AASun task

void	lowProcessesInit (void)
{
	bLowProcessEnabled = true ;

	aaTaskCreate (
		WLAN_TASK_PRIORITY,			// Task priority
		"tLan",						// Task name
		lowProcessesTask,			// Entry point,
		0,							// Entry point parameter
		wLanStack,					// Stack pointer
		WLAN_STACK_SIZE,			// Stack size
		AA_FLAG_STACKCHECK,			// Flags
		NULL) ;						// Created task id

	aaTaskDelay (50) ;	// Allows the task to start
}

//--------------------------------------------------------------------------------
