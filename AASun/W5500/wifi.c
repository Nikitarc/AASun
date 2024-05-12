/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	wifi.c		Communication with ESP32 (WIFI)

	When		Who	What
	20/03/24	ac	Creation

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aaprintf.h"

#include	"stdbool.h"
#include	"string.h"

#include	"AASun.h"

#include	"gpiobasic.h"
#include	"rccbasic.h"
#include	"dmabasic.h"

#include	"stm32g0xx_ll_bus.h"

#include	"wifiMsg.h"
#include	"wifi.h"
#include	"w25q.h"
#include	"httpUtil.h"

#include	"aautils.h"	// For aaDump

extern		mfsCtx_t		wMfsCtx ;		// MFS file system context for HTTP server

//--------------------------------------------------------------------------------

#define		WIFIUART		USART1			// USART clock must be PCLK, so BBR = PCLK/baud_rate
											// The USART clock is configured in BSP SystemClock_Config.c
#define		IRQNUM			USART1_IRQn
#define		WIFIRTOR		20				// Receiver timeout, bit time count

static const gpioPinDesc_t	pinTx = {	'A',	9,	AA_GPIO_AF_1,	AA_GPIO_MODE_ALT_PP_UP | AA_GPIO_SPEED_LOW } ;
static const gpioPinDesc_t	pinRx = {	'A',	10,	AA_GPIO_AF_1,	AA_GPIO_MODE_ALT_PP_UP | AA_GPIO_SPEED_LOW } ;

#define		TX_DMA_CHANNEL	LL_DMA_CHANNEL_2	// Channels 2 and 3 shares the same interrupt vector
#define		RX_DMA_CHANNEL	LL_DMA_CHANNEL_3

static		char			txBuff [WBUF_SIZE] ;
static		char			rxBuff [WBUF_SIZE] ;

static		uint32_t		rxReadOffset ;

// To synchronize AASun/ESP32 UART exchanges
#define		SYNC_TMO		3000				// millisecond
static		uint32_t		syncTmoStartTime ;

// Time out for receiving message data
#define		RX_TMO			200
static		uint32_t		rxTmoStartTime ;

// To manage the date/time request timeout
static		bool			bWifiTimeoutOn ;
static		uint32_t		wifiDateTimeout ;
#define		WIFI_DATE_TMO	3000

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Handling of requests from AASun to WIFI interface
// Request are message that AAsun want to send to WIFI interface
// These messages can only be sent in response to the REQ message sent by the WIFI interface

// Messages for the requests
static	uint8_t		dateRequestMsg [sizeof (wifiMsgHdr_t) + sizeof (uint64_t)] ;

// Indexes of the requests in the arrays
#define		REQ_IX_DATE		0

// Array of request messages addresses
static	uint8_t	*	requestMsg [] =
{
	dateRequestMsg,
};

static	const uint32_t	requestCount = sizeof (requestMsg) / sizeof (uint8_t *) ;

// Array of waiting requests
static	bool		requestWaiting [sizeof (requestMsg) / sizeof (uint8_t *)] ;

// The active request
#define	REQUEST_NONE	99
static	uint32_t	requestActive ;

//--------------------------------------------------------------------------------

static	void	requestInit (void)
{
	memset (requestWaiting, 0 , sizeof (requestWaiting)) ;
	requestActive = REQUEST_NONE ;
}

//--------------------------------------------------------------------------------
//	Build the date message then request the send

bool	wifiDateRequest (void)
{
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) dateRequestMsg ;

	if (requestWaiting [REQ_IX_DATE] == true)
	{
		// Already waiting
		return false ;
	}
	pHdr->magic = WIFIMSG_MAGIC ;
	pHdr->msgId = WM_ID_REQ_DATE ;
	pHdr->dataLength = 0 ;		// No data for the request message

	requestWaiting [REQ_IX_DATE] = true ;

	// Start the receive timeout
	wifiDateTimeout = aaGetTickCount () ;
	bWifiTimeoutOn  = true ;
	return true ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

void	wifiTxStart (void * address, uint32_t size)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [TX_DMA_CHANNEL] ;

	// Set DMA data parameter
	pStream->CNDTR = size ;
	pStream->CMAR  = (uint32_t) address ;
	pStream->CPAR  = (uint32_t) & WIFIUART->TDR ;

	// Clear DMA channel flags
	pDma->IFCR = DMA_FLAG_ALLIF << (TX_DMA_CHANNEL << 2u) ;

	WIFIUART->ICR = USART_ICR_TCCF ;	// Clear TC of USART

	// Set UART DMAT bit
	WIFIUART->CR3 |=  USART_CR3_DMAT ;

	// Enable the DMA transfer
	pStream->CCR |= DMA_CCR_EN ;
}

//--------------------------------------------------------------------------------
// Returns true if TX ended

bool	wifiTxCheckEnd (void)
{
	if ((WIFIUART->ISR & USART_ISR_TC) == 0)
	{
		return false ;
	}

	// Stop TX
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [TX_DMA_CHANNEL] ;

	// Clear UART DMAT bit
	WIFIUART->CR3 &=  ~USART_CR3_DMAT ;

	// Disable the DMA transfer
	pStream->CCR &= ~DMA_CCR_EN ;

	return true ;
}

//--------------------------------------------------------------------------------
// Start RX in rxBuff, for WBUF_SIZE bytes

void	wifiRxStart (void)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [RX_DMA_CHANNEL] ;

	rxReadOffset = 0 ;

	// Set DMA data parameter
	pStream->CNDTR = WBUF_SIZE ;
	pStream->CMAR  = (uint32_t) rxBuff ;
	pStream->CPAR  = (uint32_t) & WIFIUART->RDR ;

	// Clear DMA channel flags
	pDma->IFCR = DMA_FLAG_ALLIF << (RX_DMA_CHANNEL << 2u) ;

	// Set UART DMAR bit
	WIFIUART->CR3 |=  USART_CR3_DMAR ;

	// Clear RTOF and enable RX
	WIFIUART->ICR  = USART_ICR_RTOCF ;
	WIFIUART->CR1 |= USART_CR1_RE ;

	// Enable the DMA transfer
	pStream->CCR |= DMA_CCR_EN ;
}

//--------------------------------------------------------------------------------
//	Test if the RX TimeOut Flag is set (end of message)

uint32_t	wifiRxCheck (void)
{
	return (WIFIUART->ISR & USART_ISR_RTOF) != 0 ;
}

//--------------------------------------------------------------------------------
//	Stops the RX

void	wifiRxStop (void)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [RX_DMA_CHANNEL] ;

	// Clear UART DMAT bit
	WIFIUART->CR3 &=  ~USART_CR3_DMAR ;

	// Disable the DMA transfer
	pStream->CCR &= ~DMA_CCR_EN ;
}

//--------------------------------------------------------------------------------
//	Return the count of available bytes in RX buffer

uint32_t	wifiRxlength (void)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [RX_DMA_CHANNEL] ;

	return ((WBUF_SIZE - pStream->CNDTR) - rxReadOffset) & WBUF_MASK ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Copy data from the UART buffer to the RX ring buffer
// Copy data using iWrite (write to the RX ring buffer)

static	void	telnetRecv (telnetDesc_t * pTnDesc)
{
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) rxBuff ;
	uint32_t		freeSize ;		// Free size in receive ring buffer
	uint32_t		dataSize ;		// Data length to read in the UART buffer
	uint32_t		written ;
	uint32_t		chunkSize ;

	written = 0 ;

	while (written != pHdr->dataLength)
	{
		// Wait for some space in the ring buffer
		while (1)
		{
			aaCriticalEnter () ;
			if (rbGetWriteCount (& pTnDesc->rxBuffer) == 0)
			{
				// Buffer is full. aaIoWait() will release the critical section
				if (AA_ETIMEOUT == aaIoWait (& pTnDesc->rxWriteList, 0u, 100u))		// 100ms timeout
				{
					// Timeout: Cancel the write
					return ;
				}
			}
			else
			{
				break ;	// Not full. Note: exit while inside critical section
			}
		}
		aaCriticalExit () ;

		// Only 1 writer so no need of critical section
		freeSize = rbGetWriteCount (& pTnDesc->rxBuffer) ;
		dataSize = pHdr->dataLength - written ;	// Amount of data available in the UART

		if (dataSize > freeSize)
		{
			dataSize = freeSize ;
		}
		if (dataSize == 0)
		{
			continue ;
		}
		// Now dataSize is the length to read from the UART

		chunkSize = rbWriteChunkSize (& pTnDesc->rxBuffer) ;
		if (chunkSize >= dataSize)
		{
			// It can be read at once
			memcpy (rbGetWritePtr (& pTnDesc->rxBuffer), pHdr->message, dataSize) ;
			rbAddWrite (& pTnDesc->rxBuffer, dataSize) ;
		}
		else
		{
			// Wrap at the buffer end: write 1st part until the buffer end
			memcpy (rbGetWritePtr (& pTnDesc->rxBuffer), pHdr->message, chunkSize) ;
			rbAddWrite (& pTnDesc->rxBuffer, chunkSize) ;

			// Write the 2nd part (at the beginning of the buffer)
			memcpy (rbGetWritePtr (& pTnDesc->rxBuffer), pHdr->message + chunkSize, dataSize - chunkSize) ;
			rbAddWrite (& pTnDesc->rxBuffer, dataSize - chunkSize) ;
		}
		written += dataSize ;

		// If some task is waiting to read from the RX buffer awake it
		(void) aaIoResumeWaitingTask (& pTnDesc->rxReadList) ;
	}
}

//--------------------------------------------------------------------------------
// Copy data from the TX ring buffer to the UART TX buffer
// Copy data using iRead index (read from the TX ring buffer)
// Return true if there is something to send, so there is a message in UART txBuff

static	bool	telnetSend (telnetDesc_t * pTnDesc)
{
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) txBuff ;
	uint32_t		dataSize ;		// Data length to write in the socket
	uint32_t		toSendSize ;
	uint32_t		chunkSize ;

	dataSize   = dataMessageMax ;	// Space available in UART TX buffer for message data
	toSendSize = rbGetReadCount (& pTnDesc->txBuffer) ;

	if (dataSize > toSendSize)
	{
		dataSize = toSendSize ;
	}
	if (dataSize == 0u)
	{
		return false ;	// Nothing to write
	}
	// Now dataSize is the length to write to the UART buffer

	chunkSize = rbReadChunkSize (& pTnDesc->txBuffer) ;
	if (chunkSize >= dataSize)
	{
		// It can be read at once
		memcpy (pHdr->message, rbGetReadPtr (& pTnDesc->txBuffer), dataSize) ;
		rbAddRead (& pTnDesc->txBuffer, dataSize) ;
	}
	else
	{
		// Wrap at the buffer end: read 1st part until the buffer end
		memcpy (pHdr->message, rbGetReadPtr (& pTnDesc->txBuffer), chunkSize) ;
		rbAddRead (& pTnDesc->txBuffer, chunkSize) ;

		// Read the 2nd part, at the beginning of the buffer
		memcpy (pHdr->message + chunkSize, rbGetReadPtr (& pTnDesc->txBuffer), dataSize - chunkSize) ;
		rbAddRead (& pTnDesc->txBuffer, dataSize - chunkSize) ;
	}

	// Build the UART message header
	pHdr->magic      = WIFIMSG_MAGIC ;
	pHdr->msgId      = WM_ID_TELNET ;
	pHdr->dataLength = dataSize ;

	// If some task is waiting to transmit awake it
	(void) aaIoResumeWaitingTask (& pTnDesc->txWriteList) ;

	return true ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	WIFI over UART state machine

#define	WST_IDLE			0
#define	WST_WAIT_RX_HDR		1
#define	WST_WAIT_RX_DATA	2
#define	WST_WAIT_TX			3
#define	WST_SYNC_SEND		4
#define	WST_SYNC_TX			5
#define	WST_SYNC_WAIT		6

static	uint32_t	wifiState ;		// Initialized to WST_IDLE=0 by BSS

// Build the header of a message in txBuff, then start TX
static	void builHdrAndSend (uint32_t id, uint32_t size)
{
	wifiMsgHdr_t	* pHdr ;

	pHdr = (wifiMsgHdr_t *) txBuff ;
	pHdr->magic      = WIFIMSG_MAGIC ;
	pHdr->msgId      = id ;
	pHdr->dataLength = size ;
	wifiRxStart () ;	// Enable RX before the end of TX
	wifiTxStart (txBuff, wifiHdrSize + size) ;
	wifiState = WST_WAIT_TX ;
}

void	wifiNext (void)
{
	wifiMsgHdr_t	* pHdr ;

	if (bWifiTimeoutOn)
	{
		if ((aaGetTickCount () - wifiDateTimeout) >= WIFI_DATE_TMO)
		{
aaPuts ("WIFI_DATE_TMO\n") ;
			// After sending a date request, we don't receive a WM_ID_REQ_DATE message
			// Maybe because we were unable to emit the WM_ID_REQ_DATE message
			bWifiTimeoutOn = false ;
			requestWaiting [REQ_IX_DATE] = false ;
			timeUpdateWifi (NULL) ;		// Warns the date/time module of the timeout
		}
	}

	switch (wifiState)
	{
		case WST_IDLE:
			// Nothing to do, not started
			break ;

		case WST_WAIT_RX_HDR:			// Waiting for a message header
			if (wifiRxlength() >= wifiHdrSize)
			{
				pHdr = (wifiMsgHdr_t *) rxBuff ;
				if (pHdr->magic != WIFIMSG_MAGIC)
				{
					// Communication desynchronization
					wifiState = WST_SYNC_SEND ;
				}
				else
				{
					syncTmoStartTime = aaGetTickCount () ;	// Header received, so the UART link is active
					rxTmoStartTime   = syncTmoStartTime ;
					wifiState = WST_WAIT_RX_DATA ;			// Go to wait for the message data (its size is in the header)
				}
			}
			else
			{
				// Check if time out elapsed
				if ((aaGetTickCount () - syncTmoStartTime) > SYNC_TMO)
				{
					// Time is up, send SYNC message
					wifiState = WST_SYNC_SEND ;
aaPuts ("WST_WAIT_RX_HDR tmo\n") ;
				}
			}
			break ;

		case WST_WAIT_RX_DATA:			// waiting for message data
			{
				if ((aaGetTickCount () - rxTmoStartTime) >= RX_TMO)
				{
					// Time out elapsed: reset RX then wait for next header
					wifiRxStop () ;
					wifiRxStart () ;
					wifiState = WST_WAIT_RX_HDR ;
aaPuts ("WST_WAIT_RX_DATA tmo\n") ;
				}

				pHdr = (wifiMsgHdr_t *) rxBuff ;
				if (wifiRxlength() >= (wifiHdrSize + pHdr->dataLength))
				{
					// Message received, handle this message
					wifiRxStop () ;
//aaPrintf ("MSG %u  rxl %u  msgl %u  ", pHdr->msgId, wifiRxlength(), wifiHdrSize + pHdr->dataLength) ;
//aaDump ((uint8_t *) pHdr, wifiHdrSize) ;
//aaDump (pHdr->message, pHdr->dataLength) ;

					if (pHdr->msgId == WM_ID_CGI)
					{
						wifiReqMsgCgi_t	* pCgiMess = (wifiReqMsgCgi_t *) pHdr->message  ;
						uint8_t			result ;
//aaPuts ("CGI ") ; aaPuts (pCgiMess->uriName) ; aaPutChar ('\n') ;

						// Preset error response message
						pHdr = (wifiMsgHdr_t *) txBuff ;
						pHdr->magic = WIFIMSG_MAGIC ;
						pHdr->msgId = WM_ID_ERROR_404 ;
						pHdr->dataLength = 0 ;

						if (pCgiMess->type == WM_TYPE_GET)
						{
							// GET
							uint32_t	size ;

							result = http_get_cgi_handler_common  ( (uint8_t *) pCgiMess->uriName,
																	pCgiMess->uri,
																	(uint8_t *) (txBuff + wifiHdrSize + wifiRespMsgCgiSize),
																	WBUF_SIZE - wifiHdrSize - wifiRespMsgCgiSize,
																	& size) ;
							if (result == HTTP_OK)
							{
								// Get CGI found
								pHdr = (wifiMsgHdr_t *) txBuff ;
								pHdr->msgId      = WM_ID_CGI_RESP ;
								pHdr->dataLength = wifiRespMsgCgiSize + size ;
//aaPrintf ("CGI resp %u %u\n", pHdr->dataLength, size) ;
								wifiRespMsgCgi_t	* pMess = (wifiRespMsgCgi_t *) (pHdr->message) ;
								pMess->respSize = size ;
								strcpy (pMess->contentType, contentTypeJson) ;
							}
						}
						else
						{
							// POST
							uint32_t		size ;
							postCgiParam_t	param ;

							param.contentSize = pCgiMess->uriSize ;
							param.data        = pCgiMess->uri ;
							param.respBuffer  = txBuff + wifiHdrSize + wifiRespMsgCgiSize ;
							param.respLen	  = & size  ;

							result = http_post_cgi_handler_common (pCgiMess->uriName, & param) ;

							if (result == HTTP_OK)
							{
								// Post CGI found
								pHdr = (wifiMsgHdr_t *) txBuff ;
								pHdr->msgId      = WM_ID_CGI_RESP ;
								pHdr->dataLength = wifiRespMsgCgiSize + size ;

								wifiRespMsgCgi_t	* pMess = (wifiRespMsgCgi_t *) pHdr->message ;
								pMess->respSize = size ;
								strcpy (pMess->contentType, contentTypeJson) ;
							}
						}

						// There is a message to send in txBuffer
						wifiRxStart () ;	// Enable RX before the end of TX
						wifiTxStart (txBuff, wifiHdrSize + ((wifiMsgHdr_t *) txBuff)->dataLength) ;
//aaPrintf ("CGI resp ID %u %u\n", pHdr->msgId, pHdr->dataLength) ;
						wifiState = WST_WAIT_TX ;
					}

					else if (pHdr->msgId == WM_ID_SYNC)			//-----------------------------------------
					{
						// Do not answer to SYNC message, nothing to do
						wifiRxStart () ;
						wifiState = WST_WAIT_RX_HDR ;
aaPuts ("WM_ID_SYNC received!\n") ;
					}

					else if (pHdr->msgId == WM_ID_REQ)			//-----------------------------------------
					{
						// The ESP32 is asking if we have something to send
						if (telnetSend (& telnetDesc))
						{
							// There is telnet data to transmit in TX buffer
							wifiRxStart () ;	// Enable RX before the end of TX
							wifiTxStart (txBuff, wifiHdrSize + ((wifiMsgHdr_t *) txBuff)->dataLength) ;
						}
						else
						{
							// Search for a waiting request
							uint32_t		ii ;

							for (ii = 0 ; ii < requestCount ; ii++)
							{
								if (requestWaiting [ii] == true)
								{
									// This is a waiting request
									requestActive = ii ;
									wifiRxStart () ;	// Enable RX before the end of TX
									wifiTxStart (requestMsg [ii], wifiHdrSize + ((wifiMsgHdr_t *) requestMsg [ii])->dataLength) ;
									break ;
								}
							}
							if (ii == requestCount)
							{
								// Nothing to send, answer NAK
								builHdrAndSend (WM_ID_NACK, 0) ;
//aaPuts ("WM_ID_REQ received\n") ;
							}
						}
						wifiState = WST_WAIT_TX ;
					}

					else if (pHdr->msgId == WM_ID_GET_INFO)		//-----------------------------------------
					{
						// The ESP32 is asking for configuration informations: HTTP page version, IP addresses...
aaPuts ("WM_ID_GET_INFO received\n") ;
						// Retrieve WIFI application version (1st data word)
						pHdr = (wifiMsgHdr_t *) rxBuff ;
						wifiSoftwareVersion = ((wifiGetInfoMsg_t *) pHdr->message)->version ;
						wifiModeAP          = ((wifiGetInfoMsg_t *) pHdr->message)->softAP != 0 ;

						// Build the answer message
						pHdr = (wifiMsgHdr_t *) txBuff ;
						wifiInfoMsg_t	* pMess = (wifiInfoMsg_t *) pHdr->message  ;

						mfsGetCrc (& wMfsCtx, & pMess->fsCRC, & pMess->fsSize) ;
//aaPrintf ("crc:0x%08X  size:%08X\n", pMess->fsCRC, pMess->fsSize) ;

						// The WIFI IP address is the AASun IP Address + 1
						pMess->ipAddress = (* ((uint32_t *) & aaSunCfg.lanCfg.ip [0])) + 0x01000000 ;
						pMess->ipMask    =  * ((uint32_t *) & aaSunCfg.lanCfg.sn  [0]) ;
						pMess->ipGw      =  * ((uint32_t *) & aaSunCfg.lanCfg.gw  [0]) ;
						pMess->dns1      =  * ((uint32_t *) & aaSunCfg.lanCfg.dns [0]) ;
						pMess->dns2      = 0 ;	// Not used
//aaPrintf ("IP:%08X  SN:%08X  GW:%08X\n", pMess->ipAddress, pMess->ipMask, pMess->ipGw) ;

						builHdrAndSend (WM_ID_INFO, sizeof (wifiInfoMsg_t)) ;
					}

					else if (pHdr->msgId == WM_ID_GET_FS)		//-----------------------------------------
					{
						// The ESP32 need to update its copy of the HTTP file system
						wifiPageMsg_t	* pReq = (wifiPageMsg_t *) pHdr->message ;

						pHdr = (wifiMsgHdr_t *) txBuff ;

						// Read the flash
						W25Q_SpiTake () ;
						W25Q_Read (pHdr->message, (uint32_t) wMfsCtx.userData + pReq->offset, pReq->size) ;
						W25Q_SpiGive () ;
//if (pReq->offset == 0) aaDumpEx (pHdr->message, 128, NULL) ;
						// Send the message
						builHdrAndSend (WM_ID_FS, pReq->size) ;
					}

					else if (pHdr->msgId == WM_ID_REQ_DATE)		//-----------------------------------------
					{
						// We receive a date to update the local date
						bWifiTimeoutOn = false ;				// Clear the timeout
						timeUpdateWifi ((struct tm *) pHdr->message) ;
						builHdrAndSend (WM_ID_ACK, 0) ;			// Acknowledge the message
					}

					else if (pHdr->msgId == WM_ID_TELNET)		//-----------------------------------------
					{
						// WIFI Telnet data received
						telnetRecv (& telnetDesc) ;				// Copy the data to the RX ring buffer
						builHdrAndSend (WM_ID_ACK, 0) ;			// Acknowledge the message
					}

					else if (pHdr->msgId == WM_ID_TELNET_START)	//-----------------------------------------
					{
						// WIFI Telnet connection opened
aaPuts ("WM_ID_TELNET_START\n") ;
						if (bTelneInUse)
						{
							builHdrAndSend (bWifiTelnet ?
											WM_ID_ACK :		// Accepted: already WIFI Telnet
											WM_ID_NACK,		// Rejected: Telnet already in use by wired LAN
											0) ;
						}
						else
						{
							telnetSwitchOn () ;					// Accepted
							bWifiTelnet = true ;
							builHdrAndSend (WM_ID_ACK, 0) ;
						}
					}

					else if (pHdr->msgId == WM_ID_TELNET_STOP)	//-----------------------------------------
					{
						// WIFI Telnet connection closed
						if (bTelneInUse  &&  bWifiTelnet)
						{
							telnetSwitchOff () ;
						}
aaPuts ("WM_ID_TELNET_STOP\n") ;
						builHdrAndSend (WM_ID_ACK, 0) ;
					}

					else if (pHdr->msgId == WM_ID_ACK)	//-----------------------------------------
					{
						// It's just a heartbeat so as not to lose synchronization
aaPuts ("WM_ID_ACK\n") ;
						builHdrAndSend (WM_ID_ACK, 0) ;
					}

					else										//-----------------------------------------
					{
						// Unknown message, answer NAK to not break the synchronization
						builHdrAndSend (WM_ID_NACK, 0) ;
aaPrintf ("WIFI Unknown Id: %u\n", pHdr->msgId) ;
					}
				}
			}
			break ;

		case WST_WAIT_TX:
			if (wifiTxCheckEnd ())
			{
				// End of transmit
				// If a request is pending, then this is the end of the request transmit
				if (requestActive != REQUEST_NONE)
				{
					requestWaiting [requestActive] = false ;	// Free for a new request
					requestActive = REQUEST_NONE ;
				}
				wifiState = WST_WAIT_RX_HDR ;
			}
			break ;

		// The following states are specific to the synchronization of AASun and WIFI interface
		case WST_SYNC_SEND:
			{
				statusWClear (STSW_WIFI_EN) ;	// WIFI not available

				// Reset RX DMA
				wifiRxStop () ;
				wifiRxStart () ;

				// Send SYNC message
				pHdr = (wifiMsgHdr_t *) txBuff ;
				pHdr->magic      = WIFIMSG_MAGIC ;
				pHdr->msgId      = WM_ID_SYNC ;
				pHdr->dataLength = 0 ;
				wifiTxStart (txBuff, wifiHdrSize) ;

				wifiState = WST_SYNC_TX ;
//aaPuts ("WST_SYNC_SEND\n") ;
			}
			break ;

		case WST_SYNC_TX:
			if (wifiTxCheckEnd ())
			{
				// SYNC message transmission ended
				syncTmoStartTime = aaGetTickCount () ;	// Reset timeout
				wifiState = WST_SYNC_WAIT ;
			}
			break ;

		case WST_SYNC_WAIT:
			if (wifiRxlength() >= wifiHdrSize)
			{
				// Something received: end of sync
				if (requestActive != REQUEST_NONE)
				{
					// This request is silently aborted
					requestWaiting [requestActive] = false ;
					requestActive = REQUEST_NONE ;
				}

				statusWSet (STSW_WIFI_EN) ;		// WIFI is detected and running
				wifiState = WST_WAIT_RX_HDR ;
aaPuts ("WST_SYNC_WAIT Ok\n") ;
			}
			else
			{
				if ((aaGetTickCount () - syncTmoStartTime) > SYNC_TMO)
				{
					// Time elapsed without receiving a header, send SYNC message again
					wifiState = WST_SYNC_SEND ;
				}
			}
			break ;

		default:
			AA_ASSERT (0) ;
				break ;
	}
}

//--------------------------------------------------------------------------------
//	Initialize the UART for DMA transfer

void	wifiInit (void)
{
	// Initialize UART
	gpioConfigurePin (& pinTx) ;
	gpioConfigurePin (& pinRx) ;

	rccEnableUartClock	(WIFIUART) ;
	rccResetUart		(WIFIUART) ;

	// USART default:
	//		8 bits, over sampling 16, parity none, 1 stop bit, LSB first,
	WIFIUART->BRR  = rccGetPCLK1ClockFreq () / WIFIMSG_BBR ;  // Set baud rate
	WIFIUART->CR1 |= USART_CR1_TE ;    		// TX is always enabled

	WIFIUART->CR1 |= USART_CR1_UE ;         // USART enable

	// Initialize TX DMA
	LL_AHB1_GRP1_EnableClock (LL_AHB1_GRP1_PERIPH_DMA1) ;

	LL_DMA_ConfigTransfer  (DMA1, TX_DMA_CHANNEL,
							LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
							LL_DMA_MODE_NORMAL                |
							LL_DMA_PERIPH_NOINCREMENT         |
							LL_DMA_MEMORY_INCREMENT           |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_PRIORITY_MEDIUM) ;

	// Select UART_TX as DMA transfer request
	// Only 1 DMA on this MCU so DMA channel number is also MUX channel number
	((dmaMux_t *) DMAMUX1)->CCR [TX_DMA_CHANNEL] = LL_DMAMUX_REQ_USART1_TX ;


	// Initialize RX DMA
	LL_DMA_ConfigTransfer  (DMA1, RX_DMA_CHANNEL,
							LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
							LL_DMA_MODE_CIRCULAR                |
							LL_DMA_PERIPH_NOINCREMENT         |
							LL_DMA_MEMORY_INCREMENT           |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_PRIORITY_MEDIUM) ;

	// Select UART_RX as DMA transfer request
	// Only 1 DMA on this MCU so DMA channel number is also MUX channel number
	((dmaMux_t *) DMAMUX1)->CCR [RX_DMA_CHANNEL] = LL_DMAMUX_REQ_USART1_RX ;

	requestInit () ;

	// Start the WIFI state machine
	wifiState = WST_SYNC_SEND ;
}

//--------------------------------------------------------------------------------
