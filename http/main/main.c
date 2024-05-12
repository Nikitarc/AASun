/*
----------------------------------------------------------------------

	Energy monitor and diverter: WIFI interface

	Alain Chebrou

	main.c		Configuration and user commands

	When		Who	What
	20/03/24	ac	Creation

	ESP32 Web Server with ESP-IDF
	https://esp32tutorials.com/esp32-web-server-esp-idf/
	Console for USB/JTAG port: set CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y and no secondary console in sdkconfig

----------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 		// Required for memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <esp_http_server.h>

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#include <esp_clk_tree.h>

#define	 MAIN_GLOBAL	// Globals are instantiated here
#include "global.h"

//----------------------------------------------------------------------

static const char * TAG = "main" ; // TAG for debug

#define DEFAULT_ESP_WIFI_SSID	"No SSID"
#define DEFAULT_ESP_WIFI_PASS	"No Password"
#define ESP_MAXIMUM_RETRY		5

// Credential for AP mode
#define ESP_WIFI_AP_SSID      	"AASun"
#define ESP_WIFI_AP_PASS      	"AASun***"
#define ESP_WIFI_AP_CHANNEL  	1
#define MAX_WIFI_AP_STA_CONN  	4

#define BACKUP_DNS_SERVER       "208.67.222.222"	// OpenDns

// FreeRTOS event group to signal when we are connected (after boot only)
static EventGroupHandle_t s_wifi_event_group ;

// The event group allows multiple bits for each event, but we only care about two events:
// - we are connected to the AP with an IP
// - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT	BIT0
#define WIFI_FAIL_BIT		BIT1

static int s_retry_num = 0 ;

//----------------------------------------------------------------------
// For WIFI automatic reconnection

typedef enum
{
	WST_WAIT_IP		= 0,
	WST_CONNECTED,
	WST_DELAY,

} wifiState_t ;

static	wifiState_t	wifiState ;		// Initial state: WST_WAIT_IP
static	uint32_t	wifiStartTimeout ;
static	bool 		bWifiGotIp ;
static	bool		bWifiDisconnected ;

#define		WIFI_CONNECT_TIMEOUT	(5000 / portTICK_PERIOD_MS)

#define		LOOP_DELAY		20		// ms, main loop period

//----------------------------------------------------------------------
// WIFI credential management

#define			WIFI_NAMESPACE		"WIFI_NVS"

static	char	wifiSSID   [64] ;
static	char	wifiPSWD   [64] ;
static	char	wifiPSWDAp [64] ;
static	char	bWifiCredentialOk ;

//----------------------------------------------------------------------

#define	MAIN_TASK_PRIOITY		5

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Register a DNS server IP address

static esp_err_t set_dns_server (esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE))
    {
        esp_netif_dns_info_t    dns ;
        dns.ip.u_addr.ip4.addr = addr ;
        dns.ip.type            = IPADDR_TYPE_V4 ;
        ESP_ERROR_CHECK (esp_netif_set_dns_info (netif, type, & dns)) ;
    }
    return ESP_OK ;
}

//----------------------------------------------------------------------
// Set a static IP for STA ou AP mode

static void set_static_ip (esp_netif_t * netif)
{
    if (esp_netif_dhcpc_stop (netif) != ESP_OK)
    {
        ESP_LOGE (TAG, "Failed to stop DHCP client") ;
        return ;
    }

    esp_netif_ip_info_t ip ;
	// Use the addresses provided by AASun for the WIFI AP
    memset (& ip, 0 , sizeof(esp_netif_ip_info_t)) ;
    ip.ip.addr      = aaSunInfo.ipAddress + (bSoftAP ? 0x00010000 : 0) ;
    ip.gw.addr      = aaSunInfo.ipGw      + (bSoftAP ? 0x00010000 : 0) ;
    ip.netmask.addr = aaSunInfo.ipMask ;

    // How to use ipaddr_addr:  ip.ip.addr = ipaddr_addr ("192.168.1.140") ;

    if (esp_netif_set_ip_info (netif, & ip) != ESP_OK)
    {
        ESP_LOGE (TAG, "Failed to set IP info") ;
        return ;
    }

    if (bSoftAP)
    {
		if (esp_netif_dhcpc_start (netif) != ESP_OK)
		{
			ESP_LOGE (TAG, "Failed to start DHCP client") ;
			return ;
		}
    }
    else
    {
		// DNS1 address provided by AASun, backup DNS address defined localy
		ESP_ERROR_CHECK (set_dns_server (netif, aaSunInfo.dns1, ESP_NETIF_DNS_MAIN)) ;
		ESP_ERROR_CHECK (set_dns_server (netif, ipaddr_addr (BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP)) ;
    }
}

//----------------------------------------------------------------------
// WIFI driver events handler
// arg is a esp_netif_t *

static void wifi_event_handler (void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
	// Events for WIFI SoftAP
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data ;
        ESP_LOGW(TAG, "station "MACSTR" join, AID=%d", MAC2STR (event->mac), event->aid) ;
    }

    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t * event = (wifi_event_ap_stadisconnected_t *) event_data ;
        ESP_LOGW(TAG, "station "MACSTR" leave, AID=%d", MAC2STR (event->mac), event->aid) ;
    }


    // Events for WIFI_STA
    else if (event_base == WIFI_EVENT  &&  event_id == WIFI_EVENT_STA_START)
    {
    	ESP_LOGW(TAG, "WIFI_EVENT_STA_START: esp_wifi_connect") ;
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT  &&  event_id == WIFI_EVENT_STA_CONNECTED)
	{
    	ESP_LOGW(TAG, "WIFI_EVENT_STA_START: set_static_ip") ;
		set_static_ip (arg) ;
	}

    else if (event_base == WIFI_EVENT  &&  event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
    	ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED") ;

        if (s_wifi_event_group != NULL)
        {
        	// On system boot do our best to connect
			if (s_retry_num < ESP_MAXIMUM_RETRY)
			{
				esp_wifi_connect () ;
				s_retry_num++ ;
				ESP_LOGI (TAG, "Retry to connect to the AP") ;
			}
			else
			{
	    		bWifiDisconnected = true ;
				xEventGroupSetBits (s_wifi_event_group, WIFI_FAIL_BIT) ;
				ESP_LOGE (TAG, "Connect to the AP fail") ;
			}
        }
        else
        {
    		bWifiDisconnected = true ;
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
    	ESP_LOGW(TAG, "IP_EVENT_STA_GOT_IP") ;
		bWifiGotIp = true ;

        ip_event_got_ip_t * event = (ip_event_got_ip_t *) event_data ;
        ESP_LOGI (TAG, "got_ip:" IPSTR, IP2STR (& event->ip_info.ip)) ;
        s_retry_num = 0;
        if (s_wifi_event_group != NULL)
        {
        	xEventGroupSetBits (s_wifi_event_group, WIFI_CONNECTED_BIT) ;
        }
    }
}

//----------------------------------------------------------------------
// Initialize the WIFI module as Access Point

// https://esp32.com/viewtopic.php?t=9687
// For static IP example: https://esp32.com/viewtopic.php?t=13371

// Default IP address for AP: 192.168.4.1

// AP + STA: https://github.com/espressif/esp-idf/issues/6352

// switch AP and STA modes: https://esp32.com/viewtopic.php?t=22486

// example of HTTP server in AP mode : https://esp32.com/viewtopic.php?t=9687

void wifi_init_softap (void)
{
    ESP_ERROR_CHECK (esp_netif_init ()) ;
    ESP_ERROR_CHECK (esp_event_loop_create_default ()) ;

    esp_netif_t * ap_netif = esp_netif_create_default_wifi_ap () ;
//  set_static_ip (ap_netif) ;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT () ;
    ESP_ERROR_CHECK (esp_wifi_init (& cfg)) ;

    ESP_ERROR_CHECK (esp_event_handler_instance_register (WIFI_EVENT,
    		                                              WIFI_EVENT_AP_STACONNECTED,	// ESP_EVENT_ANY_ID,
                                                          & wifi_event_handler,
                                                          NULL,
                                                          NULL)) ;
    ESP_ERROR_CHECK (esp_event_handler_instance_register (WIFI_EVENT,
    		                                              WIFI_EVENT_AP_STADISCONNECTED,
                                                          & wifi_event_handler,
                                                          NULL,
                                                          NULL)) ;
    wifi_config_t wifi_config =
    {
        .ap =
        {
            .ssid           = ESP_WIFI_AP_SSID,
            .ssid_len       = strlen (ESP_WIFI_AP_SSID),
            .channel        = ESP_WIFI_AP_CHANNEL,
            .password       = ESP_WIFI_AP_PASS,
            .max_connection = MAX_WIFI_AP_STA_CONN,
            .authmode       = WIFI_AUTH_WPA2_PSK,		// No CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .pmf_cfg =
            {
                .required   = true,
            },
        },
    } ;
    strcpy ((char *) wifi_config.ap.password, wifiPSWDAp) ;
    if (strlen ((char *) wifi_config.ap.password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN ;
    }

    ESP_ERROR_CHECK (esp_wifi_set_mode (WIFI_MODE_AP)) ;
    ESP_ERROR_CHECK (esp_wifi_set_config (WIFI_IF_AP, & wifi_config)) ;
    ESP_ERROR_CHECK (esp_wifi_start ()) ;

    // Print the IP address of this AP:
    esp_netif_ip_info_t		ip_info ;
    esp_netif_get_ip_info (ap_netif, & ip_info) ;
    ESP_LOGI (TAG, "Access Point IP: " IPSTR "\n", IP2STR (& ip_info.ip)) ;

    // For security reasons do not display the password
//	ESP_LOGI (TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
//	ESP_WIFI_AP_SSID, wifiPSWDAp, ESP_WIFI_AP_CHANNEL) ;
    ESP_LOGI (TAG, "wifi_init_softap finished. SSID:%s channel:%d", ESP_WIFI_AP_SSID, ESP_WIFI_AP_CHANNEL) ;
}

//----------------------------------------------------------------------
// Initialize the WIFI module as Station

static	void	wifi_init_sta (void)
{
    s_wifi_event_group = xEventGroupCreate () ;

    ESP_ERROR_CHECK(esp_netif_init ()) ;
    ESP_ERROR_CHECK(esp_event_loop_create_default ()) ;

    esp_netif_t * sta_netif = esp_netif_create_default_wifi_sta () ;
    assert (sta_netif) ;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT () ;
    ESP_ERROR_CHECK (esp_wifi_init (& cfg)) ;

    esp_event_handler_instance_t    instance_any_id ;
    esp_event_handler_instance_t    instance_got_ip ;
    ESP_ERROR_CHECK (esp_event_handler_instance_register (WIFI_EVENT,
                                                          WIFI_EVENT_STA_START,
                                                          & wifi_event_handler,
                                                          sta_netif,
                                                          & instance_any_id)) ;
    ESP_ERROR_CHECK (esp_event_handler_instance_register (WIFI_EVENT,
    		                                              WIFI_EVENT_STA_CONNECTED,
                                                          & wifi_event_handler,
                                                          sta_netif,
                                                          & instance_any_id)) ;
    ESP_ERROR_CHECK (esp_event_handler_instance_register (WIFI_EVENT,
    		                                              WIFI_EVENT_STA_DISCONNECTED,
                                                          & wifi_event_handler,
                                                          sta_netif,
                                                          & instance_any_id)) ;
    ESP_ERROR_CHECK (esp_event_handler_instance_register (IP_EVENT,
                                                          IP_EVENT_STA_GOT_IP,
                                                          & wifi_event_handler,
                                                          sta_netif,
                                                          & instance_got_ip)) ;
    wifi_config_t wifi_config =
    {
        .sta =
        {
//            .ssid     = DEFAULT_ESP_WIFI_SSID,
//            .password = DEFAULT_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    } ;
	strcpy ((char *) wifi_config.sta.ssid,     wifiSSID) ;
	strcpy ((char *) wifi_config.sta.password, wifiPSWD) ;

    ESP_ERROR_CHECK (esp_wifi_set_mode   (WIFI_MODE_STA)) ;
    ESP_ERROR_CHECK (esp_wifi_set_config (WIFI_IF_STA, & wifi_config)) ;
    ESP_ERROR_CHECK (esp_wifi_start      ()) ;

    ESP_LOGI (TAG, "wifi_init_sta finished.") ;

    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened
    // For security reasons do not display the password
    if (bits & WIFI_CONNECTED_BIT)
    {
//      ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifiSSID, wifiPSWD) ;
        ESP_LOGI(TAG, "connected to ap SSID:%s", wifiSSID) ;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
//      ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", wifiSSID, wifiPSWD) ;
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", wifiSSID) ;
    }
    else
    {
        ESP_LOGE (TAG, "s_wifi_event_group UNEXPECTED EVENT") ;
    }
    vEventGroupDelete (s_wifi_event_group) ;
    s_wifi_event_group = NULL ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	SNTP

#include "esp_netif_sntp.h"
#include "esp_sntp.h"

/* https://www.di-mgt.com.au/wclock/help/wclo_tzexplain.html
[Europe/Paris]
TZ=CET-1CEST,M3.5.0/2,M10.5.0/3
CET     = designation for standard time when daylight saving is not in force
-1      = offset in hours = negative so 1 hour east of Greenwich meridian
CEST    = designation when daylight saving is in force ("Central European Summer Time")
,       = no offset number between code and comma, so default to one hour ahead for daylight saving
M3.5.0  = when daylight saving starts = the last Sunday in March (the "5th" week means the last in the month)
/2,     = the local time when the switch occurs = 2 a.m. in this case
M10.5.0 = when daylight saving ends = the last Sunday in October.
/3,     = the local time when the switch occurs = 3 a.m. in this case
*/

#define		TZ		"CET-1CEST,M3.5.0/2,M10.5.0/3"		// Europe/Paris

#define		SNTP_TIME_SERVER		"pool.ntp.org"

bool	sntpInit	(void) ;
bool	sntpGet		(void) ;

static	void time_sync_notification_cb (struct timeval *tv)
{
	time_t    now ;
	struct tm timeinfo ;
	char      strftime_buf[64] ;

	ESP_LOGI (TAG, "Notification of a time synchronization event") ;
	time (& now) ;
	localtime_r (& now, & timeinfo) ;
	strftime (strftime_buf, sizeof (strftime_buf), "%c", & timeinfo) ;
	ESP_LOGI (TAG, "The current date/time is: %s", strftime_buf) ;
}

// This sets the SNTP module, then start the system clock synchronization
// To send a SNTP request use: sntp_restart ()

// To do only once
bool	sntpInit (void)
{
    ESP_LOGI (TAG, "sntpInit()") ;
	setenv ("TZ", TZ, 1);
	tzset () ;

	// This is the basic default config with one server and starting the service
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG (SNTP_TIME_SERVER);
	config.sync_cb = time_sync_notification_cb ;     // Note: This is only needed if we want
    esp_netif_sntp_init (& config) ;

    return true ;
}

// Return true if a SNTP answer is received and the system time is updated
bool	sntpGet (void)
{
    int retry = 0 ;
    const int retry_count = 15 ;

    // Wait for time to be set (If registered the callback will also be called)
	// This will break the synchronization with AASun
    while (esp_netif_sntp_sync_wait (2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count)
    {
        ESP_LOGI (TAG, "Waiting SNTP answer... (%d/%d)", retry, retry_count) ;
    }
	return retry != retry_count ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

static	void main_init (void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init () ;
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK (nvs_flash_erase ()) ;
        ret = nvs_flash_init () ;
    }
    ESP_ERROR_CHECK (ret) ;

    // Get WIFI mode selected by GPIO strap
    bSoftAP = gpio_get_level (WIFIMODE_PIN) == 0 ;

    bWifiCredentialOk = wifiGetCredential () ;
    if (! bWifiCredentialOk)
    {
        ESP_LOGE (TAG, "No WIFI credential.") ;
		strcpy (wifiSSID, DEFAULT_ESP_WIFI_SSID) ;
		strcpy (wifiPSWD, DEFAULT_ESP_WIFI_PASS) ;
    }

    // Initialize the UART to communicate with AAsun
    // Get STA IP addresses
    wifiUartInit () ;

    // Start WIFI
    if (bSoftAP)
    {
		ESP_LOGI (TAG, "ESP_WIFI_MODE_AP") ;
		wifi_init_softap () ;
    }
    else
    {
		ESP_LOGI (TAG, "ESP_WIFI_MODE_STA") ;
		wifi_init_sta () ;
    }

    // Disable any WiFi power save mode, this allows best throughput
	esp_wifi_set_ps (WIFI_PS_NONE);

	if (! bSoftAP)	// SNTP not available in Access Point mode
	{
		sntpInit () ;	// Start system time update
	}

	setup_server () ;	// Start HTTP server

    ESP_LOGI (TAG, "AASun WIFI interface is running %s mode...\n", bSoftAP ? "AP" : "STA") ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	Non blocking get string
//	Reads in at most one less than size characters from the console
//	and stores them into the buffer.
//	Reading stops after a \r, it is not stored into the buffer.
//	Characters exceeding size-1 are discarded.
//	A terminating null byte (\0) is stored after the last character in the buffer.
//	The return value is 1 when the \r is encountered, else 0.

//	Limited cooking of ANSI/xterm escape sequences : left/right arrow, backspace, SUP, home, end
//	Tested with putty as terminal
#if (1)
#define	DEL_CHAR	0x7Fu	// Sent by backspace key of PC keyboard
#define	BS_CHAR		0x08u	// ASCII backspace

// Copy src to the right (char insertion)
// count must be > 0
static inline  void	copyToRight (char * pSrc, uint32_t count)
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
static inline void	copyToLeft (char * pSrc, uint32_t count)
{
	memcpy (& pSrc [-1], pSrc, count) ;
}


uint32_t	aaCheckGetChar (void)
{
	int	cc ;

	if (-1 == (cc = fgetc (stdin)))
	{
		return 0 ;
	}
	ungetc (cc, stdin) ;
	return 1 ;
}

static inline int	aaGetChar (void)
{
	return fgetc (stdin) ;
}

static inline void	aaPutChar	(int cc)
{
	// Avoid stdout buffering
	write (fileno (stdout), & cc, 1) ;
}

uint32_t	aaGetsNonBlock		(char * pBuffer, uint32_t size)
{
	static	uint32_t 	nn ;
	static	uint32_t 	pos ;
	static	char		* pStr ;
	static	uint32_t	state = 0 ;

	uint32_t	ii  ;
	char		cc ;
	uint32_t	result = 0 ;	// 1 at the end of the input

	while (result == 0  &&  aaCheckGetChar () != 0)
	{
		switch (state)
		{
			case 0:			// Idle state, this is the beginning of an input
				nn = 0 ;
				pos = 0 ;
				pStr = pBuffer ;
				state = 1 ;
				break ;

			case 1:			// Waiting for standard char
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
//					if (cc == '\r')
					if (cc == '\n')
					{
						aaPutChar (cc) ;
//						aaPutChar ('\n') ;
						pBuffer [nn] = 0 ;
						result = 1u ;
						state = 0 ;
						break ;		// End of input
					}
					if (cc == 0x1B)			// ESC
					{
						state = 2 ;
						break ;		// Wait for ESC parameter
					}
					else if (cc == BS_CHAR)
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
/*
						// Backspace: move one char left
						if (pos > 0)
						{
							pos-- ;
							pStr-- ;
							aaPutChar (BS_CHAR) ;
						}
*/
					}
					else if (cc == DEL_CHAR)
					{
						// DEL: delete char under the cursor
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
					}
					else if (nn < (size - 1))
					{
						// Standard char, and buffer not full
						// If the 1st char is \n or \0 ignore
						if (nn != 0u  ||  (cc != '\n' && cc != 0u))
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
					}
					else
					{
						// Buffer overflow, ignore the char
					}
				}
				break ;

			case 2:			// Waiting for ESC parameter
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x5B)		// [
					{
						state = 3 ;
					}
					else
					{
						// Ignore NEXT char
						state = 6 ;
					}
				}
				break ;

			case 3:			// Waiting for ESC 0x5B parameter
				if (aaCheckGetChar () != 0)
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

						case 'H':		// Home	CTRL-H
							while (pos > 0)
							{
								pos-- ;
								pStr-- ;
								aaPutChar (BS_CHAR) ;
							}
							state = 1 ;
							break ;

						case '4':	// END	of PuTTY
							if (pos != nn)
							{
								for ( ; pos < nn ; pos++)
								{
									aaPutChar (* pStr++) ;
								}
							}
							break ;

						case 'F':		// END	of IDF monitor
							if (pos != nn)
							{
								for ( ; pos < nn ; pos++)
								{
									aaPutChar (* pStr++) ;
								}
							}
							state = 1 ;
							break ;

						case 'D':		// LEFT
							if (pos > 0)
							{
								pos-- ;
								pStr-- ;
								aaPutChar (BS_CHAR) ;
							}
							state = 1 ;
							break ;

						case 'C':		// RIGHT
							if (pos != nn)
							{
								pos++ ;
								aaPutChar (* pStr++) ;
							}
							state = 1 ;
							break ;

						case 'B':		// DOWN
						case 'A':		// UP
						default:		// ???
							state = 1 ;
							break ;
					}
					if (cc >= '0'  && cc <= '9')
					{
						// HOME is ESC [ 1 0x7E, but other sequences are ESC [ 1 X 0x7E
						if (cc == '1')
						{
							state = 4 ;		// Special case for HOME
						}
						else
						{
							state = 5 ;		// Ignore char until 7E
						}
					}
				}
				break ;

			case 4:			// Waiting for the 0x7E of ESC [ x 0x7E
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x7E)
					{
						// This is home command: ESC [ 1 7E
						while (pos > 0)
						{
							aaPutChar (BS_CHAR) ;
							pos-- ;
							pStr-- ;
						}
						state = 1 ;
					}
					else
					{
						state = 5 ;		// Other non managed commands
					}
				}
				break ;

			case 5:			// Ignore chars until 0x7E
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					if (cc == 0x7E)
					{
						state = 1 ;		// End of ESC sequence
					}
				}
				break ;

			case 6:			// Ignore next char
				if (aaCheckGetChar () != 0)
				{
					cc = aaGetChar () ;
					state = 1 ;
				}
				break ;

			default:
				break ;
		}
	}
	fsync (fileno (stdout)) ;
	return result ;
}
#endif

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//	WIFI STA/AP credentials management

// https://github.com/espressif/esp-idf/blob/9c99a385adaf6dfc49138f83f97069bac63eef2a/examples/storage/nvs_rw_blob/main/nvs_blob_example_main.c

static	bool	wifiGetCredential_ (nvs_handle_t nvsHandle)
{
    esp_err_t		err ;
    size_t			size = 0 ;  // Value will default to 0, if not set yet in NVS

    // Read STA SSID
    // Obtain required memory space to store blob being read from NVS
    err = nvs_get_blob (nvsHandle, "SSID", NULL, & size) ;
    if (err != ESP_OK  &&  err != ESP_ERR_NVS_NOT_FOUND)
    {
    	ESP_LOGE (TAG, "wifiGetCredentials SSID size error") ;
    	return false ;
    }
    err = nvs_get_blob (nvsHandle, "SSID", wifiSSID, & size) ;
    if (err != ESP_OK)
    {
    	ESP_LOGE (TAG, "wifiGetCredentials SSID read error") ;
      	return false ;
    }

    // Read STA password
    err = nvs_get_blob (nvsHandle, "PSWD", NULL, & size) ;
    if (err != ESP_OK  &&  err != ESP_ERR_NVS_NOT_FOUND)
    {
    	ESP_LOGE (TAG, "wifiGetCredentials PSWD size error") ;
    	return false ;
    }
    err = nvs_get_blob (nvsHandle, "PSWD", wifiPSWD, & size) ;
    if (err != ESP_OK)
    {
    	ESP_LOGE (TAG, "wifiGetCredentials PSWD read error") ;
      	return false ;
    }

    // Read AP password
    // If not found: revert to default value and return true
    err = nvs_get_blob (nvsHandle, "PSWDAP", NULL, & size) ;
    if (err != ESP_OK  &&  err != ESP_ERR_NVS_NOT_FOUND)
    {
    	ESP_LOGW (TAG, "wifiGetCredentials PSWDAP size error") ;
    	return true ;
    }
    err = nvs_get_blob (nvsHandle, "PSWDAP", wifiPSWDAp, & size) ;
    if (err != ESP_OK)
    {
    	ESP_LOGW (TAG, "wifiGetCredentials PSWDAP read error") ;
      	return true ;
    }

    return true ;
}

bool	wifiGetCredential (void)
{
    nvs_handle_t	nvsHandle ;
    esp_err_t		err ;
    bool			ret ;

    strcpy (wifiPSWDAp, ESP_WIFI_AP_PASS) ;		// Default value for AP password

    err = nvs_open (WIFI_NAMESPACE, NVS_READWRITE, & nvsHandle) ;
    if (err != ESP_OK)
    {
    	ESP_LOGI (TAG, "wifiGetCredentials open error") ;
    	return false ;
    }
    ret = wifiGetCredential_ (nvsHandle) ;
    nvs_close (nvsHandle) ;
    return ret ;
}

//----------------------------------------------------------------------

bool	wifiWriteSSID (char * pSSID)
{
    nvs_handle_t	nvsHandle ;
    esp_err_t		err ;
    bool			ret = false ;

ESP_LOGI (TAG, "wifiWriteSSID '%s'", pSSID) ;

    err = nvs_open (WIFI_NAMESPACE, NVS_READWRITE, & nvsHandle) ;
    if (err == ESP_OK)
    {
        err = nvs_set_blob (nvsHandle, "SSID", pSSID, strlen (pSSID)+1) ;
        if (err == ESP_OK)
        {
            nvs_commit (nvsHandle) ;
        	ret = true ;
        }
    }

    nvs_close (nvsHandle) ;
    return ret ;
}

//----------------------------------------------------------------------

bool	wifiWritePSWD (char * pPSWD)
{
    nvs_handle_t	nvsHandle ;
    esp_err_t		err ;
    bool			ret = false ;
ESP_LOGI (TAG, "wifiWritePSWD '%s'", pPSWD) ;

    err = nvs_open (WIFI_NAMESPACE, NVS_READWRITE, & nvsHandle) ;
    if (err == ESP_OK)
    {
        err = nvs_set_blob (nvsHandle, "PSWD", pPSWD, strlen (pPSWD)+1) ;
        if (err == ESP_OK)
        {
            nvs_commit (nvsHandle) ;
        	ret = true ;
        }
    }

    nvs_close (nvsHandle) ;
    return ret ;
}

//----------------------------------------------------------------------

bool	wifiWritePSWDAp (char * pPSWD)
{
    nvs_handle_t	nvsHandle ;
    esp_err_t		err ;
    bool			ret = false ;
ESP_LOGI (TAG, "wifiWritePSWDAp '%s'", pPSWD) ;

    err = nvs_open (WIFI_NAMESPACE, NVS_READWRITE, & nvsHandle) ;
    if (err == ESP_OK)
    {
        err = nvs_set_blob (nvsHandle, "PSWDAP", pPSWD, strlen (pPSWD)+1) ;
        if (err == ESP_OK)
        {
            nvs_commit (nvsHandle) ;
        	ret = true ;
        }
    }

    nvs_close (nvsHandle) ;
    return ret ;
}

//----------------------------------------------------------------------

bool	wifiErasePSWDAp (void)
{
    nvs_handle_t	nvsHandle ;
    esp_err_t		err ;
    bool			ret = false ;
ESP_LOGI (TAG, "wifiErasePSWDAp") ;

    err = nvs_open (WIFI_NAMESPACE, NVS_READWRITE, & nvsHandle) ;
    if (err == ESP_OK)
    {
        strcpy (wifiPSWDAp, ESP_WIFI_AP_PASS) ;		// Default value
        err = nvs_erase_key (nvsHandle, "PSWDAP") ;
        if (err == ESP_OK  ||  err == ESP_ERR_NVS_NOT_FOUND)
        {
            nvs_commit (nvsHandle) ;
        	ret = true ;
        }
    }

    nvs_close (nvsHandle) ;
    return ret ;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

static	char	cmdBuffer [128] ;

bool	eraseMfs (void) ;	// For test

// Background loop

void app_main()
{
	char		* pCmd ;
	char		* savePtr ;
	char		* pArg1 ;
	char		* pArg2 ;
	char		* pArg3 ;
 	int32_t		arg1 ;
 	int32_t		arg2 ;
 	int32_t		arg3 ;

 	(void) arg1 ;
 	(void) arg2 ;
 	(void) arg3 ;

    vTaskPrioritySet (NULL, MAIN_TASK_PRIOITY) ;

    // LED GPIO initialization
    gpio_reset_pin     (LED_PIN) ;
    gpio_set_direction (LED_PIN, GPIO_MODE_OUTPUT) ;
    gpio_set_level     (LED_PIN, LED_ON) ;

    // Station/AP mode selector
    gpio_reset_pin     (WIFIMODE_PIN) ;
    gpio_set_direction (WIFIMODE_PIN, GPIO_MODE_INPUT) ;

    ESP_LOGI (TAG, "AAsun WIFI interface") ;

    main_init () ;
    gpio_set_level(LED_PIN, LED_OFF) ;

    // Background loop
    while (1)
    {
		if (0 == aaGetsNonBlock (cmdBuffer, sizeof (cmdBuffer)))
		{
			// No user command, do other stuff:

			// The automatic WIFI reconnection state machine (if WIFI mode is STA)
			if (! bSoftAP)
			{
				switch (wifiState)
				{
					case WST_WAIT_IP:
						if (bWifiGotIp)
						{
							bWifiOk    = true ;
							wifiState  = WST_CONNECTED ;
						}
						if (bWifiDisconnected)
						{
							wifiState  = WST_CONNECTED ;
						}
						break ;

					case WST_CONNECTED:
						if (bWifiDisconnected)
						{
							bWifiOk          = false ;
							wifiStartTimeout = xTaskGetTickCount () ;	// Allow some time before attempting to reconnect
							wifiState        = WST_DELAY ;
						}
						break ;

					case WST_DELAY:
						if ((xTaskGetTickCount () - wifiStartTimeout) >= WIFI_CONNECT_TIMEOUT)
						{
							bWifiDisconnected = false ;
							bWifiGotIp        = false ;
							wifiState         = WST_WAIT_IP ;
							esp_wifi_connect () ;
						}
						break ;
				}
			}

		    telnetNext () ;

			wifiRequest () ;	// Time to ask if AASun has a request to do?

		    vTaskDelay (LOOP_DELAY / portTICK_PERIOD_MS) ;
			continue ;
		}

		// User command received: extract command word and parameters
		pCmd = strtok_r (cmdBuffer, " \t", & savePtr) ;
		if (pCmd == NULL)
		{
			continue ;
		}
		arg1 = 0 ;
		arg2 = 0 ;
		arg3 = 0 ;
		pArg1 = NULL ;
		pArg3 = NULL ;
		pArg2 = NULL ;

		// Some commands doesn't want to get tokens
		if (0 != strcmp (pCmd, "ssid")  &&  0 != strcmp (pCmd, "pswd"))
		{
			pArg1 = strtok_r (NULL, " \t", & savePtr) ;
			if (pArg1 != NULL)
			{
				arg1 = strtol (pArg1, (char **) NULL, 0) ;

				pArg2 = strtok_r (NULL, " \t", & savePtr) ;
				if (pArg2 != NULL)
				{
					arg2 = strtol (pArg2, (char **) NULL, 0) ;

					pArg3 = strtok_r (NULL, " \t", & savePtr) ;
					if (pArg3 != NULL)
					{
						arg3 = strtol (pArg3, (char **) NULL, 0) ;
					}
				}
			}
		}

		// Execute the command
		if (0 == strcmp ("?", pCmd))
		{
		 	printf ("AASun WIFI interface %u.%u\n", VERSION >> 16, VERSION & 0xFFFF) ;

			printf ("ssid         Set WIFI STA SSID (spaces not allowed)\n") ;
			printf ("pswd         Set WIFI STA Password (spaces not allowed)\n") ;
			printf ("w?           Display WIFI credential\n") ;
			printf ("sntp         Get SNTP date\n") ;
			printf ("emfs         Erase 1st sector of MFS (test)\n") ;
			printf ("dis          Disconnect WIFI station (test)\n") ;
		}

		else if (0 == strcmp ("w?", pCmd))
		{
			if (wifiGetCredential ())
			{
			    // For security reasons do not display the password
				printf ("STA SSID: '%s'\n", wifiSSID) ;
//				printf ("STA PSWD: '%s'\n", wifiPSWD) ;
				printf ("AP  SSID: '%s'\n", ESP_WIFI_AP_SSID) ;
//				printf ("AP  PSWD: '%s'\n", wifiPSWDAp) ;
			}
			else
			{
				puts ("Not found") ;
			}
		}

		else if (0 == strcmp ("ssid", pCmd))
		{
			char	* pStr ;

			pStr = strtok_r (NULL, "", & savePtr) ;	// Get the entire string
			if (pStr == NULL  || strlen (pStr) == 0  ||  strlen (pStr) >= sizeof (wifiSSID))
			{
				puts ("Bad size") ;
			}
			else
			{
				if (wifiWriteSSID (pStr) == true)
				{
					puts ("SSID recorded") ;
				}
			}
		}

		else if (0 == strcmp ("pswd", pCmd))
		{
			char	* pStr ;

			pStr = strtok_r (NULL, "", & savePtr) ;	// Get the entire string
			if (pStr == NULL  || strlen (pStr) == 0  ||  strlen (pStr) >= sizeof (wifiSSID))
			{
				puts ("Bad size") ;
			}
			else
			{
				if (wifiWritePSWD (pStr) == true)
				{
					puts ("PSWD recorded") ;
				}
			}
		}

		else if (0 == strcmp ("sntp", pCmd))
		{
			sntp_restart () ;
		}

		else if (0 == strcmp ("emfs", pCmd))	// To test file system copy to local flash
		{
			eraseMfs () ;
		}

		else if (0 == strcmp ("in", pCmd))		// Get required WIFI mode
		{
			int level = gpio_get_level (WIFIMODE_PIN) ;
			printf ("WIFI Mode: %d %s\n", level, level == 1 ? "STA" : "AP") ;
			eraseMfs () ;
		}

		else if (0 == strcmp ("dis", pCmd)  &&  bSoftAP == false)	// Disconnect WIFI: to test automatic reconnection
		{
			if (ESP_OK != esp_wifi_disconnect ())
			{
			    ESP_LOGE (TAG, "esp_wifi_disconnect error") ;
			}
		}

		else if (0 == strcmp ("con", pCmd)  &&  bSoftAP == false)	// Connect WIFI
		{
			if (ESP_OK != esp_wifi_connect ())
			{
			    ESP_LOGE (TAG, "esp_wifi_connect error") ;
			}
		}
    }
}

//----------------------------------------------------------------------
