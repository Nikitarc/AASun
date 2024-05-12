/*
----------------------------------------------------------------------

	Energy monitor and diverter: WIFI interface

	Alain Chebrou

	http.c		HTTP server

	When		Who	What
	20/03/24	ac	Creation

	https://github.com/espressif/esp-idf/blob/master/examples/protocols/sockets/tcp_server/main/tcp_server.c

----------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Required for memset

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_partition.h"

#include <time.h>
#include <lwip/sockets.h>

#include "global.h"
#include "mfs.h"

//----------------------------------------------------------------------

static const	char 			* TAG = "http" ; // TAG for debug

static	void	uartSynchronize (void) ;

//----------------------------------------------------------------------
// To access the HTTP data in SPI flash

#define		MFS_PARTITION_TYPE		((esp_partition_type_t)    0x80)
#define		MFS_PATITION_SUBTYPE	((esp_partition_subtype_t) 0x80)

static	mfsCtx_t	mfsCtx ;
static	mfsFile_t	mfsFile ;

// The user provided functions to set in the MFS context

static	int mfsDevRead (void * userData, uint32_t address, void * pBuffer, uint32_t size)
{
	esp_err_t res = esp_partition_read ((esp_partition_t *) userData, address, pBuffer, size) ;
	return (res == ESP_OK) ? MFS_ENONE : MFS_EIO ;
}

static	void mfsDevLock (void * userData)
{
	(void) userData ;
}

static	void mfsDevUnlock (void * userData)
{
	(void) userData ;
}

//----------------------------------------------------------------------

#define	REQUEST_TMO_FAST	(100 / portTICK_PERIOD_MS)	// While Telnet is on
#define	REQUEST_TMO_SLOW	(500 / portTICK_PERIOD_MS)	// While Telnet is off
static	uint32_t			requestStartTime ;
static	uint32_t			requestCounter ;		// To slow the LED pace
static	uint32_t			requestTimeout ;		// Set to REQUEST_TMO_FAST or REQUEST_TMO_SLOW

#define	SYNC_TMO			(1500 / portTICK_PERIOD_MS)

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Send a file as request response

static esp_err_t http_response_file (httpd_req_t * req)
{
	int32_t			size, len ;
	int32_t			fileLen ;
	const char *	path ;
	char *			pExt ;

ESP_LOGI (TAG, "FILE: %s", req->uri) ;
    if (strcmp (req->uri, "/")  == 0)
    {
    	path = "/index.html" ;
    }
    else if (strcmp (req->uri, "/id") == 0)
    {
    	path = "/identify.html" ;
    }
    else
    {
    	path = req->uri ;
    	if (path [0] == '.')
    	{
    		path++ ;		// skip '.'
    	}
    }

    // Set the response content type
    pExt = strrchr (path, '.') ;
    if (pExt != NULL)
    {
		if (strcmp (pExt, ".js") == 0)
		{
			httpd_resp_set_type (req, "application/javascript") ;
		}
		else if (strcmp (pExt, ".ico") == 0)
		{
			httpd_resp_set_type (req, "image/x-icon") ;
		}
		else if (strcmp (pExt, ".css") == 0)
		{
		    httpd_resp_set_type (req, "text/css") ;
		}
		else if (strcmp (pExt, ".png") == 0)
		{
			httpd_resp_set_type (req, "image/png") ;
		}
		else
		{
			// Default value is "text/html"
		}
    }

    if (MFS_ENONE == mfsOpen (& mfsCtx, & mfsFile, path))
	{
		fileLen = mfsSize (& mfsFile) ;
	    ESP_LOGI (TAG, "FILE found: %ld bytes", fileLen) ;

		len = 0 ;
		while (len < fileLen)
		{
			size = MIN (sizeof (uartBuffer), fileLen - len) ;
			mfsRead (& mfsFile, uartBuffer, size) ;
			len += size ;

			httpd_resp_send_chunk (req, uartBuffer, size) ;
		}
		// An empty chunk to signal HTTP response completion
//		httpd_resp_set_hdr(req, "Connection", "close") ;
		httpd_resp_send_chunk (req, NULL, 0) ;
		mfsClose (& mfsFile) ;
		return ESP_OK ;
	}

	// File not found
	httpd_resp_send_err (req, HTTPD_404_NOT_FOUND, "File does not exist") ;
	return ESP_FAIL ;
}

//----------------------------------------------------------------------

static	esp_err_t get_index_handler (httpd_req_t *req)
{
	http_response_file (req) ;
    return ESP_OK ;
}

static	esp_err_t get_file_handler (httpd_req_t *req)
{
	http_response_file (req) ;
    return ESP_OK ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

// Extract the URI name: from the beginning until the 1st space, the 1st '?' or the end of string
// Don't modify the original URI string

bool get_http_uri_name (const char * uri, char * uri_buf)
{
	char		* uri_ptr ;
	uint32_t	len ;

	if (! uri)
		return false ;

	// Exclude the beginning /, except if the string is only /
	uri_ptr = (char *) uri ;
	if (strcmp (uri_ptr, "/"))
	{
		uri_ptr++ ;
	}

	len = strcspn (uri_ptr, " ?") ;
	// Exclude / only if it is at the end
	if (len != 1  &&  uri_ptr[len-1] == '/')
	{
		len-- ;		// exclude '/?' from the name
	}
	strncpy ((char *) uri_buf, uri_ptr, len) ;
	uri_buf [len] = 0 ;

	return true ;
}

//----------------------------------------------------------------------
// Send a message to AASun then wait for the response message
// The message to send is built in uartBuffet and pointed to by pHdr

static	esp_err_t cgi_message_exchange (httpd_req_t * req, wifiMsgHdr_t * pHdr)
{
	wifiRespMsgCgi_t	* pResp ;
	size_t				size ;

	// Send the message to AASun
	uart_write_bytes (UART_NUM, pHdr, wifiHdrSize + pHdr->dataLength) ;

	// Wait for response header
// TODO: Timeout
	while (1)
	{
		uart_get_buffered_data_len (UART_NUM, & size) ;
		if (size >= wifiHdrSize)
		{
			uart_read_bytes (UART_NUM, pHdr, wifiHdrSize, 10 / portTICK_PERIOD_MS) ;
			break ;
		}
		vTaskDelay (2) ;
	}

    // Check response header
    if (pHdr->magic != WIFIMSG_MAGIC)
    {
    	//  Badly formated message: communication not synchronized
		httpd_resp_send_err (req, HTTPD_404_NOT_FOUND, "Get error") ;
		uartSynchronize () ;
		return ESP_FAIL ;
    }

	// Get message data
	pResp = (wifiRespMsgCgi_t *) pHdr->message ;
	if (pHdr->dataLength != 0)
	{
		while (1)
		{
			uart_get_buffered_data_len (UART_NUM, & size) ;
			if (size >= pHdr->dataLength)
			{
				uart_read_bytes (UART_NUM, pResp, pHdr->dataLength, 10 / portTICK_PERIOD_MS) ;
				pResp->resp [pResp->respSize] = 0 ;
				break ;
			}
			vTaskDelay (2) ;
		}
	}

	// Message handling
	if (pHdr->msgId != WM_ID_CGI_RESP)
	{
		httpd_resp_send_err (req, HTTPD_404_NOT_FOUND, "Get error") ;
	}
	else
	{
		httpd_resp_set_type (req, pResp->contentType) ;
		httpd_resp_send     (req, pResp->resp, pResp->respSize) ;
	}

	return ESP_OK ;
}

//----------------------------------------------------------------------
// Manage HTTP GET CGI request

static	esp_err_t get_cgi_handler (httpd_req_t * req)
{
	// Take the semaphore to protect access to uartBuffer
    xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;
    gpio_set_level (LED_PIN, LED_ON) ;

	wifiMsgHdr_t		* pHdr  = (wifiMsgHdr_t *) uartBuffer ;
	wifiReqMsgCgi_t		* pMess = (wifiReqMsgCgi_t *) pHdr->message ;
	size_t				size ;
	esp_err_t			status ;

	ESP_LOGI (TAG, "GET CGI: %s", req->uri) ;

    // Build the message to send to AASun in uartBuffer
    // Get the request URI name and URI query string
    get_http_uri_name (req->uri, pMess->uriName) ;
    size = httpd_req_get_url_query_len (req) ;
    if (size != 0)
    {
    	httpd_req_get_url_query_str (req, pMess->uri, lenMax - wifiHdrSize - wifiReqMsgCgiSize) ;
    }
	pMess->uri [size] = 0 ;
	pMess->uriSize    = size ;
    pMess->type       = WM_TYPE_GET ;

    pHdr->magic      = WIFIMSG_MAGIC ;
    pHdr->msgId      = WM_ID_CGI ;
    pHdr->dataLength = wifiReqMsgCgiSize + pMess->uriSize ;

	status = cgi_message_exchange (req, pHdr) ;
    gpio_set_level (LED_PIN, LED_OFF) ;
	xSemaphoreGiveRecursive (uartMutex) ;
	return status ;
}

//----------------------------------------------------------------------
// Manage HTTP POST CGI request

static	esp_err_t post_cgi_handler (httpd_req_t * req)
{
	wifiMsgHdr_t		* pHdr  = (wifiMsgHdr_t *) uartBuffer ;
	wifiReqMsgCgi_t		* pMess = (wifiReqMsgCgi_t *) pHdr->message ;
	size_t				size, nn ;
	int					ret ;
	esp_err_t			status ;

    ESP_LOGI (TAG, "POST CGI: %s", req->uri) ;

	// Take the semaphore to protect access to uartBuffer
	xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;
    gpio_set_level (LED_PIN, LED_ON) ;

	// Get the request URI name
    get_http_uri_name (req->uri, pMess->uriName) ;

    // Get the request body length
    size = req->content_len ;	// Length of the body
    if (size >= lenMax - wifiHdrSize - wifiReqMsgCgiSize - 1)
    {
        ESP_LOGE (TAG, "POST CGI URI too large: %d / %lu", size, lenMax - wifiHdrSize - wifiReqMsgCgiSize) ;
    	httpd_resp_send_err (req, HTTPD_400_BAD_REQUEST, "Body too large");
        gpio_set_level (LED_PIN, LED_OFF) ;
    	xSemaphoreGiveRecursive (uartMutex) ;
        return ESP_OK ;
    }

    // Get the body data, like: {"mid":"17","V1":"1"}
    nn = 0 ;
    while (nn != size)
    {
		ret = httpd_req_recv (req, pMess->uri + nn, lenMax - wifiHdrSize - wifiReqMsgCgiSize - 1 - nn) ;
		if (ret <= 0)	// 0 return value indicates connection closed
		{
			if (ret == HTTPD_SOCK_ERR_TIMEOUT)
			{
				// httpd_resp_send_408 (req) ;	// HTTP 408 (Request Timeout) error
				// Retry receiving if timeout occurred
				continue ;
			}
		    gpio_set_level (LED_PIN, LED_OFF) ;
			xSemaphoreGiveRecursive (uartMutex) ;
			return ESP_FAIL ; // Return ESP_FAIL to close underlying socket
		}
		nn += ret ;
    }

    // Test if the POST is for local use (credentials)
ESP_LOGI (TAG, "uriName %s", pMess->uriName) ;
    if (strncmp (pMess->uriName, "wifiCredential", 14)== 0)
    {
ESP_LOGI (TAG, "wifiCredential") ;
    	pMess->uri [req->content_len] = 0 ;
    	status = ESP_OK ;

    	// This is a POST to the WIFI interface
    	if (strcmp (pMess->uriName, "wifiCredentialAp.cgi") == 0)
    	{
ESP_LOGI (TAG, "AP PSWD '%s'", pMess->uri) ;

    		if (! wifiWritePSWDAp (pMess->uri))
    		{
    	       	status = ESP_FAIL ;
    	  	}
    	}
    	else if (strcmp (pMess->uriName, "wifiCredentialSta.cgi") == 0)
    	{
    		// Station mode credentials
    		char	* pPSWD ;

			pPSWD = strchr (pMess->uri, ' ') ;
    		* pPSWD = 0 ;
    		pPSWD++ ;

ESP_LOGI (TAG, "STA SSID '%s'", pMess->uri) ;
ESP_LOGI (TAG, "STA PSWD '%s'", pPSWD) ;
    		if (! wifiWriteSSID (pMess->uri))
    		{
            	status = ESP_FAIL ;
    		}
    		else if (! wifiWritePSWD (pPSWD))
        	{
               	status = ESP_FAIL ;
        	}
    	}
    	else if (strcmp (pMess->uriName, "wifiCredentialApReset.cgi") == 0)
    	{
ESP_LOGI (TAG, "AP Reset PSWD '%s'", pMess->uri) ;

    		if (! wifiErasePSWDAp ())
    		{
    	       	status = ESP_FAIL ;
    	  	}
    	}
    	else
    	{
           	status = ESP_FAIL ;	// Unknown URI
    	}

    	if (status == ESP_OK)
    	{
    		httpd_resp_send (req, "OK 0", 4) ;
    	}
    	else
    	{
        	httpd_resp_send_err (req, HTTPD_400_BAD_REQUEST, "Error 0 0");
        	status = ESP_OK ;
    	}
    }
    else
    {
		// To send to AASun: Build the UART message
		pMess->uri [size] = 0 ;
		pMess->uriSize    = size ;
		pMess->type       = WM_TYPE_POST ;

		pHdr->magic      = WIFIMSG_MAGIC ;
		pHdr->msgId      = WM_ID_CGI ;
		pHdr->dataLength = wifiReqMsgCgiSize + pMess->uriSize ;

		status = cgi_message_exchange (req, pHdr) ;
    }
	gpio_set_level (LED_PIN, LED_OFF) ;
	xSemaphoreGiveRecursive (uartMutex) ;
	return status ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	URI declaration, compatible with the function my_uri_match()

static const httpd_uri_t uriList [] =
{
	{
		.uri      = "/*.cgi",
		.method   = HTTP_GET,
		.handler  = get_cgi_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.cgi",
		.method   = HTTP_POST,
		.handler  = post_cgi_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/",
		.method   = HTTP_GET,
		.handler  = get_index_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.html",
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.js",
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.css",
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.ico",
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/*.png",
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	},
	{
		.uri      = "/id",		// Special case to get identify.html
		.method   = HTTP_GET,
		.handler  = get_file_handler,
		.user_ctx = NULL
	}
} ;

static const int32_t uriListCount = sizeof (uriList) / sizeof (httpd_uri_t) ;

//----------------------------------------------------------------------

static	bool my_uri_match (const char * template, const char * uri, size_t len)
{
	const char	* pTempExt ; 	// Template last extension
	const char	* pUriExt ; 	// URI last extension
	int			uriExtLen ;
	bool		result ;

	if (template [0] != '/'  ||  template [1] != '*')
	{
		// The template doesn't contain * wildcard, so use strcmp
		result = 0 == strcmp (template, uri) ;
//ESP_LOGI (TAG, "match1 '%s'  '%s'  %d  %d", template, uri, len, result) ;
		return result ;
	}
//ESP_LOGI (TAG, "match2 '%s'  '%s'  %d", template, uri, len) ;

	// Search for the template extension
	pTempExt = strrchr (template, '.') ;

	// Search for the last extension in the URI
	pUriExt = uri + len  ;
	if (* pUriExt != 0)
	{
		// The URI has parameter like: /temperature.cgi/?mid=All', whose len is 17
		pUriExt-- ;
	}
	uriExtLen = 0 ;
	do
	{
		pUriExt-- ;
		uriExtLen++ ;
	} while (* pUriExt != '.'  &&  pUriExt != uri) ;

	if (pUriExt == uri)
	{
		pUriExt = NULL ;
	}
	else
	{
//ESP_LOGI (TAG, "match3  uri:'%s'  %d", pUriExt, uriExtLen) ;
	}

	if (pTempExt == NULL  ||  pUriExt == NULL)
	{
		// One of the string doesn't contain extension, so use strcmp
		result = 0 == strcmp (template, uri) ;
//		ESP_LOGI (TAG, "match4 '%s'  '%s'  %d  %d", template, uri, len, result) ;
		return result ;
	}

	// Both template and uri contain extension, compare the extension
	result = 0 == strncmp (pTempExt, pUriExt, uriExtLen) ;

//	ESP_LOGI (TAG, "match5 '%s'  '%s'  %d  %d", template, uri, len, result) ;
	return result ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Synchronize UART communication

static	void		uartSynchronize (void)
{
	size_t			size ;
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) uartBuffer ;
	uint32_t		tmoStartTime ;

	uart_flush_input (UART_NUM) ;

	tmoStartTime = xTaskGetTickCount () ;
	while (1)
	{
    	uart_get_buffered_data_len (UART_NUM, & size) ;
    	if (size >= wifiHdrSize)
    	{
    		uart_read_bytes (UART_NUM, pHdr, wifiHdrSize, 10 / portTICK_PERIOD_MS) ;
    		if (pHdr->magic == WIFIMSG_MAGIC  &&  pHdr->msgId == WM_ID_SYNC)
    		{
    			// Synchronized
    			break ;
    		}
    		uart_flush_input (UART_NUM) ;
    	}
    	if ((xTaskGetTickCount () - tmoStartTime) > SYNC_TMO)
    	{
    		// Time out: Reset RX buffer
    		ESP_LOGI (TAG, "UART sync tmo, flush input") ;
    		uart_flush_input (UART_NUM) ;
    		tmoStartTime = xTaskGetTickCount () ;
    	}
    	vTaskDelay (2) ;
	}

	// Sync message received. Send the received SYNC message as acknowledge
	uart_write_bytes (UART_NUM, pHdr, wifiHdrSize) ;

	// Now ESP32/AAsun are synchronized, and AASun go passive mode and will no longer take the initiative to send a message
	// Clean the reception buffer after being sure to have received the possible last message from AASun
	vTaskDelay (100 / portTICK_PERIOD_MS) ;
	uart_flush_input (UART_NUM) ;
	ESP_LOGI (TAG, "UART sync done") ;
}

//----------------------------------------------------------------------
//	The message to send is built in a buffer pointed to by pHdr
//	The answer is returned in the same buffer
//	Return true on success, else false (timeout)
//  To call inside uartMutex protection (except on initialization)

bool message_exchange (wifiMsgHdr_t * pHdr)
{
	size_t				size ;

	// Send the message to AASun
	uart_write_bytes (UART_NUM, pHdr, wifiHdrSize + pHdr->dataLength) ;

	// Wait for response header
// TODO Timeout
	while (1)
	{
		uart_get_buffered_data_len (UART_NUM, & size) ;
		if (size >= wifiHdrSize)
		{
			uart_read_bytes (UART_NUM, pHdr, wifiHdrSize, 10 / portTICK_PERIOD_MS) ;
			break ;
		}
		vTaskDelay (2) ;
	}

    // Check response header
    if (pHdr->magic != WIFIMSG_MAGIC)
    {
    	//  Badly formated message: communication not synchronized
    	ESP_LOGE (TAG, "message_exchange error") ;
		return false ;
    }

	// Get message data
	if (pHdr->dataLength != 0)
	{
		while (1)
		{
			uart_get_buffered_data_len (UART_NUM, & size) ;
			if (size >= pHdr->dataLength)
			{
				uart_read_bytes (UART_NUM, pHdr->message, pHdr->dataLength, 10 / portTICK_PERIOD_MS) ;
				pHdr->message [pHdr->dataLength] = 0 ;
				break ;
			}
			vTaskDelay (2) ;
		}
	}

	return true ;
}

//----------------------------------------------------------------------
// Initialize the UART to communicate with AAsun
// Then get some configuration information

void	wifiUartInit (void)
{
	wifiMsgHdr_t		* pHdr ;

	const uart_config_t uart_config =
    {
        .baud_rate  = WIFIMSG_BBR,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    } ;
    ESP_ERROR_CHECK(uart_driver_install (UART_NUM, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, ESP_INTR_FLAG_IRAM)) ;
    ESP_ERROR_CHECK(uart_param_config   (UART_NUM, & uart_config)) ;
    ESP_ERROR_CHECK(uart_set_pin        (UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) ;
	vTaskDelay (2) ;
	uart_flush_input (UART_NUM) ;	// Flush parasitic char received at UART initialization

	uartMutex = xSemaphoreCreateRecursiveMutex () ;

	// Synchronize communication with AASun
	xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;
	uartSynchronize () ;
	xSemaphoreGiveRecursive (uartMutex) ;

	// Get information about web page version and WIFI addresses
	// Provides to AASun the WIFI application version and mode (STA or AP)
	pHdr  = (wifiMsgHdr_t *) uartBuffer ;
    pHdr->magic      = WIFIMSG_MAGIC ;
    pHdr->msgId      = WM_ID_GET_INFO ;
    pHdr->dataLength = wifiGetInfoMsgSize ;
    ((wifiGetInfoMsg_t *) pHdr->message)->version = VERSION ;
    ((wifiGetInfoMsg_t *) pHdr->message)->softAP  = bSoftAP ;

    if (message_exchange (pHdr)  &&  pHdr->msgId == WM_ID_INFO)
    {
    	memcpy (& aaSunInfo, pHdr->message, sizeof aaSunInfo) ;

    	ESP_LOGI (TAG, "AASun INFO  size:%lu  crc:0x%08lX", aaSunInfo.fsSize, aaSunInfo.fsCRC) ;
    	ESP_LOGI (TAG, " Ip         0x%08lX %lu.%lu.%lu.%lu", aaSunInfo.ipAddress,
    					 aaSunInfo.ipAddress & 0xFF,
						(aaSunInfo.ipAddress >>  8) & 0xFF,
						(aaSunInfo.ipAddress >> 16) & 0xFF,
						(aaSunInfo.ipAddress >> 24) & 0xFF) ;
    	ESP_LOGI (TAG, " Mask       0x%08lX %lu.%lu.%lu.%lu", aaSunInfo.ipMask,
    					 aaSunInfo.ipMask & 0xFF,
						(aaSunInfo.ipMask >>  8) & 0xFF,
						(aaSunInfo.ipMask >> 16) & 0xFF,
						(aaSunInfo.ipMask >> 24) & 0xFF) ;
    	ESP_LOGI (TAG, " Gw         0x%08lX %lu.%lu.%lu.%lu", aaSunInfo.ipGw,
    					 aaSunInfo.ipGw & 0xFF,
						(aaSunInfo.ipGw >>  8) & 0xFF,
						(aaSunInfo.ipGw >> 16) & 0xFF,
						(aaSunInfo.ipGw >> 24) & 0xFF) ;
    	ESP_LOGI (TAG, " DNS1       0x%08lX %lu.%lu.%lu.%lu", aaSunInfo.dns1,
    					 aaSunInfo.dns1 & 0xFF,
						(aaSunInfo.dns1 >>  8) & 0xFF,
						(aaSunInfo.dns1 >> 16) & 0xFF,
						(aaSunInfo.dns1 >> 24) & 0xFF) ;
    }
    else
    {
    	ESP_LOGE (TAG, "GET INFO error, msg ID %u", pHdr->msgId) ;
    }

	requestStartTime = xTaskGetTickCount () ;

	return ;	// Ready to start HTTP server
}

//----------------------------------------------------------------------
//	Debug Only
//	Erase 1st sector of MFS partition

bool	eraseMfs (void)
{
	const esp_partition_t	* pPartition ;

	pPartition = esp_partition_find_first (MFS_PARTITION_TYPE,  MFS_PATITION_SUBTYPE, "http_mfs") ;
	if (pPartition == NULL)
	{
		ESP_LOGE (TAG, "http_mfs partition not found") ;
		return false ;
	}
	if (ESP_OK != esp_partition_erase_range (pPartition, 0, pPartition->erase_size))
	{
		ESP_LOGI (TAG, "Flash Erase error") ;
		return false ;
	}
	ESP_LOGI (TAG, "Flash Erase OK") ;
	return true ;
}

//----------------------------------------------------------------------
// Update the web pages file system from AASun

bool	web_page_update (const esp_partition_t	* pPartition)
{
	wifiMsgHdr_t		* pHdr = (wifiMsgHdr_t *) uartBuffer ;
	wifiPageMsg_t		* pMsg = (wifiPageMsg_t *) pHdr->message ;
	uint32_t			size, offset ;

	// Erase flash partition
	size = (aaSunInfo.fsSize + (pPartition->erase_size - 1)) & ~(pPartition->erase_size - 1)  ; // round up the size to next 4k
	ESP_LOGI (TAG, "Flash Erase %lu %lu", aaSunInfo.fsSize, size) ;
	if (ESP_OK != esp_partition_erase_range (pPartition, 0, size))
	{
		ESP_LOGE (TAG, "Flash Erase error") ;
		return false ;
	}
	ESP_LOGI (TAG, "Flash Erase end") ;

	size = (aaSunInfo.fsSize + (WIFIMSG_PAGE_CHUNK - 1)) & ~(WIFIMSG_PAGE_CHUNK - 1) ;
	offset = 0 ;
	while (size != 0)
	{
	    pHdr->magic      = WIFIMSG_MAGIC ;
	    pHdr->msgId      = WM_ID_GET_FS ;
	    pHdr->dataLength = wifiPageMsgSize ;

	    pMsg->offset = offset ;
	    pMsg->size   = WIFIMSG_PAGE_CHUNK ;
	    message_exchange (pHdr) ;

	    esp_partition_write (pPartition, offset, pMsg, WIFIMSG_PAGE_CHUNK) ;
	    ESP_LOGI (TAG, "PartWrite %lu %lu", size, offset) ;

		offset += WIFIMSG_PAGE_CHUNK ;
		size   -= WIFIMSG_PAGE_CHUNK ;
	}

	return true ;
}

//----------------------------------------------------------------------

static	void close_fd_cb (httpd_handle_t hd, int sockfd)
{
   struct linger so_linger;
   so_linger.l_onoff = true;
   so_linger.l_linger = 0;
   setsockopt (sockfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
   close(sockfd);
   ESP_LOGE (TAG, "close_fd_cb %d", sockfd) ;
}

// Setup of the HTTP server and SPI page file system

httpd_handle_t setup_server (void)
{
	esp_err_t				err ;
    httpd_handle_t			server = NULL ;
    httpd_config_t			config   = HTTPD_DEFAULT_CONFIG () ;
	const esp_partition_t	* pPartition ;
	mfsError_t				mfsErr ;

	// Force UART synchronization: Initializations take a long time, and synchronization can be lost
	uartSynchronize () ;

	// Find HTTP data SPI partition and initialize the MFS context

	// Initialize access to the MFS
	mfsCtx.lock     = mfsDevLock ;
	mfsCtx.unlock   = mfsDevUnlock ;
	mfsCtx.read     = mfsDevRead ;

	pPartition = esp_partition_find_first (MFS_PARTITION_TYPE,  MFS_PATITION_SUBTYPE, "http_mfs") ;
	if (pPartition == NULL)
	{
		ESP_LOGE (TAG, "http_mfs partition not found") ;
		mfsErr = MFS_EIO ;
	}
	else
	{
		// Partition found then mount the MFS
		mfsCtx.userData = (void *) pPartition ;
		mfsErr = mfsMount (& mfsCtx) ;
		if (mfsErr != MFS_ENONE)
		{
			ESP_LOGE (TAG, "mfsMount error: %d", mfsErr) ;
		}
		else
		{
			// MFS mounted and available
			uint32_t	size, crc ;

			mfsGetCrc (& mfsCtx, & crc, & size) ;
			ESP_LOGI (TAG, "crc:0x%08lX / 0x%08lX    size:%lu / %lu", crc, aaSunInfo.fsCRC, size, aaSunInfo.fsSize) ;
			if (crc != aaSunInfo.fsCRC  ||  size != aaSunInfo.fsSize)
			{
				ESP_LOGE (TAG, "Flash MFS not up to date") ;
				mfsErr = MFS_ECORRUPT ;
			}
		}

		// Update web pages ?
		if (mfsErr != MFS_ENONE)
		{
			// Need to update the file system: copy the FS from AAsun to local SPI flash
			if (web_page_update (pPartition))
			{
				// Mount the new files system
				mfsErr = mfsMount (& mfsCtx) ;
				if (mfsErr != MFS_ENONE)
				{
					ESP_LOGE (TAG, "mfsMount error: %d", mfsErr) ;
				}
			}
			else
			{
				mfsErr = MFS_ECORRUPT ;
			}
		}
	}

	// Start HTTP server
	if (mfsErr == MFS_ENONE)
	{
		// MFS file system OK
		ESP_LOGI (TAG, "MFS Ok") ;

		// Start HTTP server
		config.max_uri_handlers  = uriListCount ;
//		config.uri_match_fn      = httpd_uri_match_wildcard ;	// Use the URI wildcard matching function
		config.uri_match_fn      = my_uri_match ;	// Use this URI wildcard matching function
		config.close_fn          = close_fd_cb ;

		if (httpd_start (& server, & config) == ESP_OK)
		{
			for (int32_t ii = 0 ; ii < uriListCount ; ii++)
			{
				err = httpd_register_uri_handler (server, & uriList [ii]) ;
				if (err != ESP_OK)
				{
					ESP_LOGE (TAG, "httpd_register_uri_handler error 0x%X", err) ;
					httpd_stop (server) ;
					server = NULL ;
					break ;
				}
			}
		}
	}

	telnetOn (false) ;		// Default value on start, no Telnet running

	ESP_LOGI (TAG, "Server start %s", (server == NULL) ? "failed" : "OK") ;
    return server ;
}

//----------------------------------------------------------------------
// Set a fast request pace if Telnet is on

void			telnetOn		(bool bTelnetOn)
{
	requestTimeout = bTelnetOn ? REQUEST_TMO_FAST : REQUEST_TMO_SLOW ;
}

//----------------------------------------------------------------------
//  Ask AASun if he has a request to make to the ESP32
//	This is also a kind of keep-alive: periodical transmission to maintain UART synchronization

void	wifiRequest (void)
{
	wifiMsgHdr_t	* pHdr = (wifiMsgHdr_t *) uartBuffer ;

	// Time out elapsed ?
	if ((xTaskGetTickCount () - requestStartTime) < requestTimeout)
	{
		return ;	// No
	}
	requestStartTime = xTaskGetTickCount () ;	// Reset time out

	// Yes: It's time to send a request

	xSemaphoreTakeRecursive (uartMutex, portMAX_DELAY) ;

	// Put on the LED from time to time
	if (requestTimeout == REQUEST_TMO_SLOW)
	{
		if ((requestCounter & 0x1) == 0)
		{
			gpio_set_level(LED_PIN, LED_ON) ;
		}
	}
	else
	{
		if ((requestCounter & 0x7) == 0)
		{
			gpio_set_level(LED_PIN, LED_ON) ;
		}
	}
	requestCounter++ ;

    pHdr->magic      = WIFIMSG_MAGIC ;
    pHdr->msgId      = WM_ID_REQ ;
    pHdr->dataLength = 0 ;
    if (! message_exchange (pHdr))
	{
    	xSemaphoreGiveRecursive (uartMutex) ;
    	return ;
	}

	// Handle the message
	switch (pHdr->msgId)
	{
		case WM_ID_NACK:
			// Nothing to do
			break ;

		case WM_ID_TELNET:
			// Some data to transmit to telnet client
			telnetSend ((char *) pHdr->message, pHdr->dataLength) ;
			break ;

		case WM_ID_REQ_DATE:
			{
				// AASun need to know the date
				time_t    now ;

ESP_LOGI (TAG, "WM_ID_REQ_DATE") ;
				// Get the date and build the message
				time (& now) ;
				localtime_r (& now, (struct tm *) pHdr->message) ;
/*
char      strftime_buf[64] ;
strftime (strftime_buf, sizeof (strftime_buf), "%c", (struct tm *) pHdr->message) ;
ESP_LOGI (TAG, "The current date/time is: %s", strftime_buf) ;
*/
				pHdr->magic      = WIFIMSG_MAGIC ;
				pHdr->msgId      = WM_ID_REQ_DATE ;
				pHdr->dataLength = sizeof (struct tm) ;
				message_exchange (pHdr) ;
			}
			break ;

		case WM_ID_SYNC:
			// Communication is not synchronized any more, then AASUn send WM_ID_SYNC messages
ESP_LOGI (TAG, "WM_ID_SYNC") ;
			uartSynchronize () ;
			break ;

		default:
			ESP_LOGE (TAG, "Unknown request: %d", pHdr->msgId) ;
			break ;
	}

	// At this point all transactions with the UART should be completed
    gpio_set_level(LED_PIN, LED_OFF) ;
	xSemaphoreGiveRecursive (uartMutex) ;
}

//----------------------------------------------------------------------



