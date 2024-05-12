/*
----------------------------------------------------------------------

	Energy monitor and diverter: WIFI interface

	Alain Chebrou

	global.h	Global definitions and variables in only 1 place

	When		Who	What
	09/04/24	ac	Creation

----------------------------------------------------------------------
*/

#ifndef GLOBAL_H_
#define GLOBAL_H_

#if defined MAIN_GLOBAL
#define		EXTERN
#else
#define		EXTERN	extern
#endif

#include "driver/uart.h"
#include "freertos/semphr.h"
#include <esp_http_server.h>

#include "wifiMsg.h"

// The software version
#define		VERSION		((1 << 16) | 0)

//----------------------------------------------------------------------

#define LED_PIN 		8
#define	LED_ON			0
#define	LED_OFF			1

#define	WIFIMODE_PIN	2		// There is a pull up resistor on the board

//----------------------------------------------------------------------
//	USART to communicate with AASun

#define	UART_NUM			UART_NUM_1

#define UART_TX_PIN			GPIO_NUM_0	//	GPIO_NUM_21
#define UART_RX_PIN			GPIO_NUM_1	//	GPIO_NUM_20

#define	UART_RX_BUF_SIZE	2048
#define	UART_TX_BUF_SIZE	1024

//----------------------------------------------------------------------

// In main.c
EXTERN	int led_state ;

#define	lenMax				4048
EXTERN	char				uartBuffer [lenMax] ;		// The use of uartBuffer must be protected by uartMutex
EXTERN	SemaphoreHandle_t	uartMutex ;

EXTERN	wifiInfoMsg_t		aaSunInfo ;

EXTERN	bool				bSoftAP ;		// True if WIFI mode is AP
EXTERN	bool				bWifiOk ;		// True when the station is connected to an AP

//----------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
// in main.c
bool			wifiGetCredential	(void) ;
bool			wifiWritePSWD		(char * pPSWD) ;
bool			wifiWriteSSID		(char * pSSID) ;
bool			wifiWritePSWDAp		(char * pPSWD) ;
bool			wifiErasePSWDAp		(void) ;


// In http.c
httpd_handle_t	setup_server		(void) ;
void			wifiRequest			(void) ;
void			wifiUartInit		(void) ;
void			telnetOn			(bool bTelnetOn) ;
bool			message_exchange	(wifiMsgHdr_t * pHdr) ;

// In telnet.c
void			telnetNext			(void) ;
uint32_t		telnetGetState		(void) ;
bool			telnetSend			(char * pData, uint32_t length) ;

#ifdef __cplusplus
}
#endif//----------------------------------------------------------------------
#endif // GLOBAL_H_
