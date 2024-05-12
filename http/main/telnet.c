/*
----------------------------------------------------------------------

	Energy monitor and diverter: WIFI interface

	Alain Chebrou

	telnet.c	Telnet bridge: communication with AAsun trough ESP32 WIFI

	When		Who	What
	09/04/24	ac	Creation

----------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "global.h"
#include "wifiMsg.h"

#define	TAG	"Telnet"

//-----------------------------------------------------------------------------

#define TELNET_PORT                    23		// Telnet port

//-----------------------------------------------------------------------------

// Telnet states
typedef enum
{
	TS_IDLE		= 0,
	TS_LISTEN,
	TS_RUNNING

} tnState_t ;

// Telnet protocol state
typedef enum
{
	TN_ST_IDLE		= 0,	// Idle
	TN_ST_IAC1,				// 1st IAC received
	TN_ST_CMD,				// Command received, only 1 byte to receive
	TN_ST_SB,				// SB received, waiting for IAC
	TN_ST_IAC2				// 2nd IAC received, waiting for SE

} tnProtoState_t ;

// Special chars of telnet protocol
enum
{
	TN_IAC		= 255,
	TN_SB		= 250,
	TN_SE		= 240
} ;

static	tnState_t				telnetState ;
static	tnProtoState_t			tnProtoState ;

static	struct sockaddr_storage	dest_addr;
static	int						listenSock ;
static	int						clientSock ;

static	char					data [WBUF_SIZE] ;
static	char *					dataMessage = & data [wifiHdrSize] ;

//-----------------------------------------------------------------------------
// Returns the string representation of IP address

static char * getIpStr (struct sockaddr_storage * ipAddr)
{
	static char addressStr [128] ;
	char * res = NULL ;

	res = inet_ntoa_r (((struct sockaddr_in *) ipAddr)->sin_addr, addressStr, sizeof(addressStr) - 1);
	if (res == NULL)
	{
		addressStr [0] = '\0' ;
	}
	return addressStr ;
}

//-----------------------------------------------------------------------------
// Manage (eat) Telnet protocol. See http://www.tcpipguide.com/free/t_TelnetOptionsandOptionNegotiation-4.htm
// Returns true if the parameter is a true char (not part of the protocol)

static	bool tnProtocol (int cc)
{
	bool	 bReturn ;

	switch (tnProtoState)
	{
		case TN_ST_IDLE:	// Idle
			if (cc == TN_IAC)
			{
				tnProtoState = TN_ST_IAC1 ;
				bReturn = false ;
			}
			else
			{
				bReturn = true ;
			}
			break ;

		case TN_ST_IAC1:	// 1st IAC received, waiting for the command
			if (cc == TN_IAC)
			{
				// Double IAC => this is a standard char
				bReturn = true ;
			}
			else
			{
				if (cc == TN_SB)
				{
					tnProtoState = TN_ST_SB ;
				}
				else
				{
					tnProtoState = TN_ST_CMD ;
				}
				bReturn = false ;
			}
			break ;

		case TN_ST_CMD:		// Command received, waiting for the value
			tnProtoState = TN_ST_IDLE ;	// End of command
			bReturn = false ;
			break ;

		case TN_ST_SB:		// SB received, waiting for IAC
			if (cc == TN_IAC)
			{
				tnProtoState = TN_ST_IAC2 ;
			}
			bReturn = false ;
			break ;

		case TN_ST_IAC2:	// 2nd IAC received, waiting for SE
			if (cc == TN_IAC)
			{
				// Double IAC => this is a sub negotiation data
				tnProtoState = TN_ST_SB ;
			}
			else if (cc == TN_SE)
			{
				// End of sub negotiation data
				tnProtoState = TN_ST_IDLE ;
			}
			else
			{
	            ESP_LOGI (TAG, "tnProtocol TN_ST_IAC2 unknown char %d", cc) ;
			}
			bReturn = false ;
			break ;

		default:
			bReturn = false ;
            ESP_LOGI (TAG, "tnProtocol Unknown state %d", tnProtoState) ;
			break ;
	}
	return bReturn ;
}

//-----------------------------------------------------------------------------

uint32_t		telnetGetState	(void)
{
	return telnetState ;
}

//-----------------------------------------------------------------------------
// Send a message to AASun: WM_ID_TELNET_START or WM_ID_TELNET_STOP
// Return true if the answer is WM_ID_ACK

uint32_t		telnetSendEvent	(uint32_t id)
{
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) uartBuffer ;
	bool			bResult ;

	xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;

    pHdr->magic      = WIFIMSG_MAGIC ;
    pHdr->msgId      = id ;
    pHdr->dataLength = 0 ;
    if (! message_exchange (pHdr))
	{
    	ESP_LOGE (TAG, "telnetSendEvent message_exchange false");
    	xSemaphoreGiveRecursive (uartMutex) ;
    	telnetOn (false) ;		// Telnet OFF
    	return false ;
	}
    bResult = pHdr->msgId == WM_ID_ACK ;

	xSemaphoreGiveRecursive (uartMutex) ;
	telnetOn (id == WM_ID_TELNET_START) ;
	ESP_LOGI (TAG, "telnetSendEvent %ld %s", id, bResult ? "Ok" : "error");
	return bResult ;
}

//-----------------------------------------------------------------------------
// Send data to telnet client

bool	telnetSend	(char * pData, uint32_t length)
{
    int	written = 0 ;
    int sent ;

    while (written < length)
    {
        sent = send (clientSock, pData + written, length - written, 0);
        if (sent < 0)
        {
        	if (errno == EAGAIN)
        	{
        		vTaskDelay (1) ;
        		continue ;
        	}
            // Failed to retransmit
            ESP_LOGE (TAG, "Error occurred during sending: errno %d", errno);
			shutdown (clientSock, 0) ;
			close (clientSock) ;
			telnetSendEvent (WM_ID_TELNET_STOP) ;
			telnetState = TS_LISTEN ;
            return false ;
        }
        written += sent ;
    }
    return true ;
}

//-----------------------------------------------------------------------------
// Remove telnet protocol character from the received data

static	uint32_t	telnetConvertReceive (char * pData, uint32_t length)
{
	uint32_t	ii ;
	uint32_t	count = 0 ;
	char		* pDst = pData ;

	for (ii = 0 ; ii < length ; ii++)
	{
		if (tnProtocol (* pData))
		{
			* pDst++ = * pData ;
			count++ ;
		}
		pData++ ;
	}
	return count ;
}

//-----------------------------------------------------------------------------
//	The Telnet state machine

void	telnetNext (void)
{
	switch (telnetState)
	{
		case	TS_IDLE:
		    {
		    	struct sockaddr_in * dest_addr_ip4 = (struct sockaddr_in *) & dest_addr ;
				dest_addr_ip4->sin_addr.s_addr = htonl (INADDR_ANY) ;
				dest_addr_ip4->sin_family      = AF_INET ;
				dest_addr_ip4->sin_port        = htons (TELNET_PORT) ;

				listenSock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
				if (listenSock < 0)
				{
					ESP_LOGI (TAG, "Unable to create listen socket: errno %d", errno);
					break ;
				}

			    // Marking the socket as non-blocking
			    int flags = fcntl (listenSock, F_GETFL) ;
			    if (fcntl (listenSock, F_SETFL, flags | O_NONBLOCK) == -1)
			    {
			        ESP_LOGE (TAG, "Unable to set listen socket non blocking: %d", errno) ;
			        close (listenSock) ;
			        break ;
			    }

			    // Binding socket to the given address
			    int err = bind (listenSock, (struct sockaddr *) & dest_addr, sizeof (dest_addr)) ;
			    if (err != 0)
			    {
			    	ESP_LOGE (TAG, "Socket unable to bind: %d", errno);
			        close (listenSock) ;
			        break ;
			    }

			    // Set queue (backlog) of pending connections to one
			    err = listen (listenSock, 1);
			    if (err != 0)
			    {
			    	ESP_LOGE (TAG, "Error occurred during listen: %d", errno);
			        close (listenSock) ;
			        break ;
			    }
			    ESP_LOGI (TAG, "Socket listening") ;
			    telnetState = TS_LISTEN ;
		    }
		    break ;

		case	TS_LISTEN:
		   	{
		   	    struct sockaddr_storage		sourceAddr ; // Large enough for both IPv4 or IPv6
		   	    socklen_t addrLen = sizeof (sourceAddr) ;

		   	    clientSock = accept (listenSock, (struct sockaddr *) & sourceAddr, & addrLen) ;
	   	        if (clientSock < 0)
	   	        {
	                if (errno == EWOULDBLOCK)
	                {
	                	break ;	// The listener socket did not accepts any connection, retry later
	                }
	                else
	                {
	                	ESP_LOGE (TAG, "Error when accepting connection: %d", errno);
	                	close (listenSock) ;
	                	telnetState = TS_IDLE ;
	                	break ;
	                }
	   	        }
	   	        else
	   	        {
	                // We have a new client connected -> print it's address
	                ESP_LOGI (TAG, "Connection accepted from IP:%s", getIpStr (& sourceAddr)) ;

	                // ...and set the client's socket non-blocking
	   	        	int flags = fcntl (clientSock, F_GETFL);
	                if (fcntl (clientSock, F_SETFL, flags | O_NONBLOCK) == -1)
	                {
	                	ESP_LOGE (TAG, "Unable to set client socket non blocking:%d ", errno) ;
   		    			shutdown (clientSock, SHUT_RDWR) ;
	                	close (clientSock) ;
	                	break ;
	                }

   		    		// Send a message to AASun to switch console to WIFI Telnet
   		    		if (telnetSendEvent (WM_ID_TELNET_START))
   		    		{
   		    			// Accepted by AASun
   		    			telnetState = TS_RUNNING ;
   		    		}
   		    		else
   		    		{
   		    			// AASun rejected the connection
   		    			send (clientSock, "Closed\r\n", 8, 0) ;
   		    			shutdown (clientSock, SHUT_RDWR) ;
						close (clientSock) ;
	                	telnetState = TS_LISTEN ;
   		    		}
	   	        }
	   		}
	   		break ;

		case	TS_RUNNING:
	   		{
	   			// Try to receive. recv returns:
	   			// -1 : nothing to read and should check errno
	   			// >0 : data available
	   			//  0 : ?
	   		    int len = recv (clientSock, dataMessage, dataMessageMax, 0) ;
	   		    if (len < 0)
	   		    {
	   		        if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK)
	   		        {
	   		            // Not an error, nothing to receive
	   		        }
	   		        else
	   		        {
						if (errno == ENOTCONN)
						{
							ESP_LOGE (TAG, "Connection closed") ;	// Socket has been disconnected
						}
						else
						{
							ESP_LOGE (TAG, "Error occurred during receiving: %d", errno) ;
						}
						// Send a massage to AASun to quit WIFI Telnet
						telnetSendEvent (WM_ID_TELNET_STOP) ;
						shutdown (clientSock, SHUT_RDWR) ;
						close (clientSock) ;
	                	telnetState = TS_LISTEN ;
	   		        }
	   		    }
	   		    else if (len > 0)
	   		    {
	   		    	// Something received
	   		    	len = telnetConvertReceive (dataMessage, len) ;	// Remove Telnet protocol data
	   		    	if (len != 0)
	   		    	{
						// Send the data to AASun
						wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) data ;

						xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;

						pHdr->magic      = WIFIMSG_MAGIC ;
						pHdr->msgId      = WM_ID_TELNET ;
						pHdr->dataLength = len ;
						if (! message_exchange (pHdr))
						{
							// UART synchronization error: release the Telnet socket
							shutdown (clientSock, SHUT_RDWR) ;
							close (clientSock) ;
							telnetState = TS_LISTEN ;
						}
						xSemaphoreGiveRecursive (uartMutex) ;
	   		    	}
	   		    }
	   		}
	   		break ;
	}
}

//-----------------------------------------------------------------------------
