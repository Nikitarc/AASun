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
#include	"util.h"		// For getMacAddres()

#include	"mfs.h"
#include	"w25q.h"
#include	"temperature.h"

#include	"stdbool.h"
#include	"string.h"


static void	telnetFlush (void) ;

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
static	uint8_t wizRxBuf[DATA_BUF_SIZE] ;
static	uint8_t wizTxBuf[DATA_BUF_SIZE] ;

aaTimerId_t		wiz1secTimerHandle ;

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
// The states of the telnet protocol state machine

typedef enum
{
	TN_ST_IDLE		= 0,	// Idle
	TN_ST_IAC1,				// 1st IAC received
	TN_ST_CMD,				// Command received, only 1 byte to receive
	TN_ST_SB,				// SB received, waiting for IAC
	TN_ST_IAC2				// 2nd IAC received, waiting for SE

} tnProtoState_t ;

enum
{
	TN_IAC		= 255,
	TN_SB		= 250,
	TN_SE		= 240
} ;

typedef struct
{
	int8_t			sn ;	// Socket number
	uint32_t		flags ;
	tnProtoState_t	tnState ;

	// Buffers management
	uint16_t		txSize, txMask ;
	uint16_t		iTxRead, iTxWrite, txCount ;
	uint16_t		rxSize, rxMask ;
	uint16_t		iRxRead, iRxWrite, rxCount ;
	uint8_t			* pTxBuffer ;
	uint8_t			* pRxBuffer ;

	aaDriverDesc_t	txList ;		// The list of tasks waiting for Tx
	aaDriverDesc_t	rxList ;		// The list of tasks waiting for Rx

} wSock_t ;

// The socket  descriptor for the console "telnet" server

STATIC	uint8_t		wcTxBuffer [SOCK_TX_SIZE] ;
STATIC	uint8_t		wcRxBuffer [SOCK_RX_SIZE] ;

STATIC	wSock_t		wConsole =
{
	0,
	0,
	TN_ST_IDLE,

	SOCK_TX_SIZE, SOCK_TX_SIZE-1,
	0, 0, 0,
	SOCK_RX_SIZE, SOCK_RX_SIZE-1,
	0, 0, 0,
	wcTxBuffer,
	wcRxBuffer,

	{ { NULL, NULL } },	// txList
	{ { NULL, NULL } },	// rxList
} ;

// Descriptor to set the socket as standard I/O
static	aaDevIo_t	wSockIo ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Returns the connected state of the telnet socket

static	bool	wCheckConnected (wSock_t * pSock)
{
	uint8_t		tmp ;
	bool		bConnected = true ;

	// check for link down
	tmp = getPHYCFGR () ;
	if ((tmp & PHYCFGR_LNK_ON) == 0)
	{
		// Link down
		closesocket (pSock->sn) ;
		bConnected = false ;
	}
	else
	{
		// Check for disconnection
		tmp = getSn_SR (pSock->sn) ;
		if (tmp != SOCK_ESTABLISHED)
		{
			if (tmp == SOCK_CLOSE_WAIT)
			{
				// Close initiated by the client
				disconnect (pSock->sn) ;	// This closes the socket
				bConnected = false ;
			}
			else
			{
				closesocket (pSock->sn) ;
				bConnected = false ;
			}
		}
	}
	return bConnected ;	// True if connected
}

//--------------------------------------------------------------------------------
// Copy data from the W5500 socket buffer to the RX circular buffer
// Copy data using iRxWrite (write to the RX circular buffer)

static	uint32_t	wRecv (wSock_t * pSock)
{
	uint32_t	freeSize ;		// Free size in receive buffer
	uint32_t	dataSize ;		// Data length to read in the socket
	uint32_t	len ;
	uint32_t	iWrite ;		// Write index in RX buffer

	freeSize = pSock->rxSize - pSock->rxCount ;
	dataSize = getSn_RX_RSR (pSock->sn) ;	// Amount of data available in the socket

	if (dataSize > freeSize)
	{
		dataSize = freeSize ;
	}
	if (dataSize == 0u)
	{
		return 0 ;	// Nothing to read
	}
	// Now dataSize is the length to read from the socket

	iWrite = pSock->iRxWrite ;
	if ((iWrite + dataSize) <= pSock->rxSize)
	{
		// It can be written at once
		len = recv (pSock->sn, & pSock->pRxBuffer [iWrite], dataSize) ;
if (len != dataSize)
{
	aaPuts ("wRecv error\n") ;
	aaTaskDelay (10) ;
	AA_ASSERT (0) ;
}
	}
	else
	{
		// Wrap at the buffer end
		len = pSock->rxSize - iWrite ; // Write from iRxWrite to the end of the buffer
		len = recv (pSock->sn, & pSock->pRxBuffer [iWrite], len) ;

		// Write the second part (at the beginning of the buffer)
		len += recv (pSock->sn, pSock->pRxBuffer, dataSize - len) ;
if (len != dataSize)
{
	aaPuts ("wRecv error\n") ;
	aaTaskDelay (10) ;
	AA_ASSERT (0) ;
}
	}

	aaCriticalEnter () ;
	pSock->rxCount += dataSize ;
	pSock->iRxWrite = (iWrite + dataSize) & pSock->rxMask ;
	aaCriticalExit () ;

	// If some task is waiting to receive awake it
	aaCriticalEnter () ;
	if (aaIsIoWaitingTask (& pSock->rxList) != 0u)
	{
		// A thread is waiting for RX char, awake it
		(void) aaIoResume (& pSock->rxList) ;
	}
	aaCriticalExit () ;

	return dataSize ;
}

//--------------------------------------------------------------------------------
// Copy data from the TX circular buffer to the W5500 socket buffer
// Copy data using iTxRead (read from the TX circular buffer)

static	uint32_t	wSend (wSock_t * pSock)
{
	int32_t		dataSize ;		// Data length to write in the socket
	int32_t		sentSize ;
	int32_t		len ;
	uint32_t	iRead ;

	dataSize = getSn_TX_FSR (pSock->sn) ;	// free space in socket TX buffer

	if (dataSize > pSock->txCount)
	{
		dataSize = pSock->txCount ;
	}
	if (dataSize == 0u)
	{
		return 0 ;	// Nothing to write
	}
	// Now dataSize is the length to write to the socket

	sentSize = 0 ;
	iRead = pSock->iTxRead ;
	// We cannot have 2 send in a row: the second returns an emitted length of 0.
	while (dataSize > 0)
	{
		if ((iRead + dataSize)  <= pSock->txSize)
		{
			// It can be read at once.
			len = dataSize ;
		}
		else
		{
			// Wrap at the buffer end
			// Copy the 1st part from iTxRead to the end of the buffer
			len = pSock->txSize - iRead ;
		}
		len = send (pSock->sn, & pSock->pTxBuffer [iRead], len) ;
if (len < 0)
{
	AA_ASSERT (0) ;
}
		dataSize -= len ;
		sentSize += len ;
		iRead = (iRead + len) & pSock->txMask ;
		if (len == 0)
		{
			// Not  sent, we have to wait a bit.
			aaTaskDelay (1) ;
		}
	}

	if (sentSize > 0)
	{
		aaCriticalEnter () ;
		pSock->txCount -= sentSize ;
		pSock->iTxRead = iRead ;
		aaCriticalExit () ;
	}

	// If some task is waiting to transmit awake it
	aaCriticalEnter () ;
	if (aaIsIoWaitingTask (& pSock->txList) != 0u)
	{
		// A thread is waiting for RTX char, awake it
		(void) aaIoResume (& pSock->txList) ;
	}
	aaCriticalExit () ;

	return sentSize ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	wLan standard IO
//--------------------------------------------------------------------------------
// To check a flag in the descriptor flags field

__STATIC_INLINE	uint32_t wCheckFlag (wSock_t * pSock, uint32_t flag)
{
	return ((pSock->flags & flag) == 0u) ? 0u : 1u ;
}

//--------------------------------------------------------------------------------
//	Set a flag to socket descriptor

void	wSetFlag		(sockHandle_t hSock, uint32_t flag, uint32_t bSet)
{
	wSock_t 	* pSock = (wSock_t *) hSock ;

	if (bSet == 0u)
	{
		pSock->flags &= ~flag ;
	}
	else
	{
		pSock->flags |= flag ;
	}
}

//-----------------------------------------------------------------------------
//	Get a flag from socket descriptor

void	wGetFlag		(sockHandle_t hSock, uint32_t * pFlag)
{
	wSock_t 	* pSock = (wSock_t *) hSock ;

	* pFlag = pSock->flags ;
}

//--------------------------------------------------------------------------------
//	To build a socked descriptor to use this socket as standard input/output

void	wInitStdDev (aaDevIo_t * pDev, sockHandle_t hSock)
{
	pDev->hDev				= hSock ;
	pDev->getCharPtr		= wGetChar ;
	pDev->putCharPtr		= wPutChar ;
	pDev->checkGetCharPtr	= wCheckGetChar ;
	pDev->checkPutCharPtr	= wCheckPutChar ;
}

//--------------------------------------------------------------------------------
// Get count of char free space to write to dev without blocking

uint32_t	wCheckPutChar		(sockHandle_t hSock)
{
	wSock_t 	* pSock = (wSock_t *) hSock ;

	return (uint32_t) (pSock->txSize - pSock->txCount) ;
}

//--------------------------------------------------------------------------------
// Put one char to device TX FIFO, blocking if FIFO full

void 		wPutChar			(sockHandle_t hSock, char cc)
{
	wSock_t 		* pSock = (wSock_t *) hSock ;
	uint32_t		flagLF = 0u ;

	// If cc is LF, then maybe send CR-LF
	if (wCheckFlag (pSock, WSOCK_FLAG_LFCRLF) != 0u  &&  cc == '\n')
	{
		flagLF = 1u ;
		cc = '\r' ;
	}

	while (1)
	{
		while (1)
		{
			aaCriticalEnter () ;
			if (pSock->txCount == pSock->txSize)
			{
				// Buffer is full, wait. The critical section will be released by aaIoWait().
				(void) aaIoWait (& pSock->txList, 0u, 0u) ;	// Not ordered, no timeout
			}
			else
			{
				break ;		// Not full. Note: exit while inside the critical section
			}
		}

		pSock->pTxBuffer [pSock->iTxWrite] = (uint8_t) cc ;
		pSock->iTxWrite = (pSock->iTxWrite + 1u) & pSock->txMask ;
		pSock->txCount++ ;
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
// Manage (eat) Telnet protocol. See http://www.tcpipguide.com/free/t_TelnetOptionsandOptionNegotiation-4.htm
// Returns the count or real char available

static	uint32_t tnProtocol (wSock_t * pSock)
{
	uint32_t	count ;
	bool		bEnd = false ;
	uint8_t		cc ;

	aaCriticalEnter () ;

	while (! bEnd)
	{
		if (pSock->rxCount == 0)
		{
			// No char available
			count = 0 ;
			break ;
		}
		else
		{
			cc = (char) pSock->pRxBuffer [pSock->iRxRead] ;
			switch (pSock->tnState)
			{
				case TN_ST_IDLE:	// Idle
					if (cc == TN_IAC)
					{
						pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
						pSock->rxCount-- ;
						pSock->tnState = TN_ST_IAC1 ;
					}
					else
					{
						count = pSock->rxCount ;
						bEnd = true ;
					}
					break ;

				case TN_ST_IAC1:	// 1st IAC received, waiting for the command
					if (cc == TN_IAC)
					{
						// Double IAC => this is a standard char
						pSock->tnState = TN_ST_IDLE ;
						count = pSock->rxCount ;
						bEnd = true ;
					}
					else
					{
						pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
						pSock->rxCount-- ;
						if (cc == TN_SB)
						{
							pSock->tnState = TN_ST_SB ;
						}
						else
						{
							pSock->tnState = TN_ST_CMD ;
						}
					}
					break ;

				case TN_ST_CMD:		// Command received, waiting for the value
					pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
					pSock->rxCount-- ;
					pSock->tnState = TN_ST_IDLE ;	// End of command
					break ;

				case TN_ST_SB:		// SB received, waiting for IAC
					pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
					pSock->rxCount-- ;
					if (cc == TN_IAC)
					{
						pSock->tnState = TN_ST_IAC2 ;
					}
					break ;

				case TN_ST_IAC2:	// 2nd IAC received, waiting for SE
					pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
					pSock->rxCount-- ;
					if (cc == TN_IAC)
					{
						// Double IAC => this is a sub negotiation data
						pSock->tnState = TN_ST_SB ;
					}
					if (cc == TN_SE)
					{
						// End of sub negotiation data
						pSock->tnState = TN_ST_IDLE ;
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

//-----------------------------------------------------------------------------
//	Returns the count of available characters in RX buffer
//	Actually if at least 1 char is available

uint32_t	wCheckGetChar (sockHandle_t hSock)
{
	wSock_t 	* pSock = (wSock_t *) hSock ;

//	return pSock->rxCount  ;
	return tnProtocol (pSock) ;
}

//--------------------------------------------------------------------------------
// Get one char from device RX FIFO, blocking if no char available

char 		wGetChar			(sockHandle_t hSock)
{
	wSock_t 	* pSock = (wSock_t *) hSock ;
	char		cc ;

	while (1)
	{
		aaCriticalEnter () ;
		if (pSock->rxCount == 0u)
		{
			// Buffer is empty. aaIoWait() will release the critical section
			(void) aaIoWait (& pSock->rxList, 0u, 0u) ;	// Not ordered, no timeout
		}
		else
		{
			if (0 != tnProtocol (pSock))
			{
				break ;	// Not empty. Note: exit while inside critical section
			}
		}
	}

	cc = (char) pSock->pRxBuffer [pSock->iRxRead] ;
	pSock->iRxRead = (pSock->iRxRead + 1) & pSock->rxMask ;
	pSock->rxCount-- ;
	aaCriticalExit () ;

	return cc ;
}

//--------------------------------------------------------------------------------
//	Send a block of data

void	wPutString (sockHandle_t hSock, char * pStr)
{
	while (* pStr != 0)
	{
		wPutChar (hSock, * pStr) ;
		pStr++ ;
	}

	// TODO ? To flush the buffer: awake the lan task
}

//--------------------------------------------------------------------------------

const uint8_t *	wGetMacAddress		(void)
{
	return (const uint8_t *) netConfiguration.mac ;
}

//--------------------------------------------------------------------------------
//	Telnet state machine

typedef enum
{
	tnNotInit = 0,
	tnOpen,
	tnListen,
	tnConnect,
	tnXfer,

} tnConState_t ;

static	tnConState_t	telnetState ;	// Default state tnNotInit

void	telnet ()
{
	switch (telnetState)
	{
		case tnNotInit:
			// Telnet: Initialize the wLan descriptor
			wConsole.flags    = 0 ;
			wConsole.iTxRead  = 0 ;
			wConsole.iTxWrite = 0 ;
			wConsole.txCount  = 0 ;
			wConsole.iRxRead  = 0 ;
			wConsole.iRxWrite = 0 ;
			wConsole.rxCount  = 0 ;
			aaIoDriverDescInit (& wConsole.txList) ;
			aaIoDriverDescInit (& wConsole.rxList) ;

			// Initialize the wLan I/O descriptor
			wInitStdDev (& wSockIo, (sockHandle_t) & wConsole) ;
			telnetState = tnOpen ;
			break ;

		case tnOpen:
			// Open socket
			wConsole.sn = socket (TELNET_SOCK_NUM, Sn_MR_TCP, TELNET_PORT, 0) ;
			if (wConsole.sn < 0)
			{
				aaPuts ("socket: " ) ;
				aaPuts (socketErrorMsg [-wConsole.sn]) ;
				aaPutChar('\n') ;
				aaTaskDelay (100) ;
				AA_ASSERT (0) ;
			}
			telnetState = tnListen ;
			break ;

		case tnListen:
			// Set socket in listen state
			if (listen (wConsole.sn) == SOCK_OK)
			{
				telnetState = tnConnect ;
			}
			break ;

		case tnConnect:
			// Waiting connection
			if (getSn_SR (wConsole.sn) == SOCK_ESTABLISHED)
			{
				// Connection established: switch to wLan console
				aaPuts ("Switch to Telnet\n") ;
				wSetFlag ((sockHandle_t) & wConsole, WSOCK_FLAG_LFCRLF, 1) ;	// Convert \n to \r\n
				wInitStdDev  (& wSockIo, (sockHandle_t) & wConsole) ;			// Initialize the devIo with wLan functions
				aaInitStdIn	 (& wSockIo) ;				// Use this devIo for input
				aaInitStdOut (& wSockIo) ;				// Use this devIo for output

				// Request the client to not local echo (for MS Telnet)
				aaPuts ("\xff\xfd\x2d") ;	// IAC DO SUPPRESS-LOCAL-ECHO
				aaPuts ("Connected\n") ;
				telnetState = tnXfer ;
			}
			break ;

		case tnXfer:
			// Something to receive ?
			wRecv (& wConsole) ;

			// Something to send ?
			wSend (& wConsole) ;

			// Check socket connected state
			if (false == wCheckConnected (& wConsole))
			{
				// Socket disconnected and possibly closed
				// Switch to UART console then go to open the socket
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
				telnetState = tnOpen ;
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
	if (telnetState == tnXfer)
	{

		while (wConsole.txCount > 0)
		{
			telnet () ;
		}
	}
}

//--------------------------------------------------------------------------------

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
// If domainName is NULL, then use memorized domainName pointer

uint32_t	dnsRequest (uint32_t server, const char * domainName, dnsCb_t dnsCb)
{
aaPrintf ("dnsRequest %u\n", server) ;
	if (dnsState != DNS_ST_IDLE)
	{
		return 0 ;	// DNS not in idle state
	}
	if (domainName != NULL)
	{
		name = domainName ;
	}
	if (name [0] == 0)
	{
		return 0 ;	// No domain name
	}
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

uint32_t	sntpDateToSecond	(localTime_t * pTime)
{
	// Because localTime_t and datetime have the same content
	return changedatetime_to_seconds ((datetime *) pTime) ;
}

uint32_t	sntpSecondToWd		(uint32_t seconds)
{
	return ((seconds / SECS_PERDAY) + 6u) % 7u ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// This task handles the local network and other low priority processes

void	lowProcessesTask (uintptr_t arg)
{
	int8_t		tmp ;
	int8_t		seqnum ;
	int8_t		mfsOk ;

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
	telnetState = tnNotInit ;
	dnsState = DNS_ST_IDLE ;
	sntpRunning = 0u ;
	seqnum = 0 ;

	DNS_init (DNS_SOCK_NUM, wizTxBuf) ;

	// This loop manage the telnet connection, http server, DNS request, SNTP, temperature sensors, etc
	while (1)
	{
		aaTaskDelay (3) ;

		tmp = 0 ;
		ctlwizchip (CW_GET_PHYLINK, (void *) & tmp) ;
		if (tmp == PHY_LINK_ON)
		{
if (statusWTest (STSW_NET_LINK_OFF)) aaPrintf ("STSW_NET_LINK_ON\n") ;	// Print only once
			statusWClear (STSW_NET_LINK_OFF) ;

			telnet () ;
			dnsNext () ;
			sntpNext () ;

			// If the file system is mounted, then manage http
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

		tempSensorNext () ;

		displayYesterdayHisto (1, 0) ;
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Create the lan task
// With a low priority: mandatory lower than the AASun task

void	lowProcessesInit (void)
{
	// Create the wLan task
	aaTaskCreate (
		WLAN_TASK_PRIORITY,			// Task priority
		"tLan",						// Task name
		lowProcessesTask,			// Entry point,
		0,							// Entry point parameter
		wLanStack,					// Stack pointer
		WLAN_STACK_SIZE,			// Stack size
		AA_FLAG_STACKCHECK,			// Flags
		NULL) ;						// Created task id

	aaTaskDelay (50) ;	// Allows the lan task to start
}

//--------------------------------------------------------------------------------
