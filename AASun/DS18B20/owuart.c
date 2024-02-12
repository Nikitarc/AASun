
#include	"aa.h"

#include	"gpiobasic.h"
#include	"rccbasic.h"

#include	"owuart.h"

/*
	When		Who	What
	25/09/23	ac	Add pull up on UART TX pin (helps not to block when there is no temperature sensor interface)
*/

//--------------------------------------------------------------------------------
// The USART and pins to use:
// The temperature code for UART doesn't use aaBasic uartbasic.c functions
// So OWUART doesn't need to be enabled in uartbasic.h

#define		OWUART		USART3		// USART clock must be PCLK, so BBR = PCLK/baud_rate
									// The USART clock is configured in BSP SystemClock_Config.c

// Open drain GPIO is mandatory for TX
// On this pin use low speed to lower the slew rate to avoid ringing
static const gpioPinDesc_t	pinTx = {	'B',	8,	AA_GPIO_AF_4,	AA_GPIO_MODE_ALT_OD_UP | AA_GPIO_SPEED_LOW } ;

static	uint16_t	bbrLow ;	// For easy and fast baud rate change
static	uint16_t	bbrHigh ;

uint32_t	owuartTimeout ;

//--------------------------------------------------------------------------------

int ow_uart_init (char *dev_path)
{
	(void) dev_path ;
	uint32_t	pclk ;

	gpioConfigurePin (& pinTx) ;

	rccEnableUartClock	(OWUART) ;
	rccResetUart		(OWUART) ;

	pclk = rccGetPCLK1ClockFreq () ;
	bbrLow  = pclk / OW_BAUD_LOW ;
	bbrHigh = pclk / OW_BAUD_HIGH ;

	// USART default:
	//		8 bits, over sampling 16, parity none, 1 stop bit, LSB first,
	OWUART->BRR  = bbrLow ;                         // Set baud rate
	OWUART->CR3 |= USART_CR3_HDSEL ;    			// Half duplex mode: RX and TX internally connected, use only TX pin
	OWUART->CR1 |= USART_CR1_RE | USART_CR1_TE ;    // RX, TX enable
	OWUART->CR1 |= USART_CR1_UE ;                   // USART enable

	return UART_SUCCESS;
}

//--------------------------------------------------------------------------------

void ow_uart_finit(void)
{
	OWUART->CR1 &= ~USART_CR1_UE ;		// Disable USART
}

//--------------------------------------------------------------------------------
// Change the baud rate
// To change the baud rate, the UART must be in disabled state

void ow_uart_setb(uint32_t baud)
{
	OWUART->CR1 &= ~USART_CR1_UE ;
	if (baud == OW_BAUD_HIGH)
	{
		OWUART->BRR  = bbrHigh ;
	}
	else
	{
		OWUART->BRR  = bbrLow ;
	}
	OWUART->CR1 |= USART_CR1_UE ;
}

//--------------------------------------------------------------------------------
// Sends a char, then returns the received char

#define	OWUART_TMO		50		// 3ms is > to 1 char at 9600 bauds
								// High value because this is a low priority task, and higher busy task can trigger this timeout

unsigned char ow_uart_putc (unsigned char c)
{
	uint32_t	startTime ;

	while ((OWUART->ISR & USART_ISR_TXE_TXFNF) == 0u)
	{
	}
	OWUART->TDR = (c & 0xFF) ;

	startTime = aaGetTickCount () ;
	while ((OWUART->ISR & USART_ISR_RXNE_RXFNE) == 0u)
	{
		if ((aaGetTickCount () - startTime) >= OWUART_TMO)
		{
			// Timeout
//aaPuts ("owuartTimeout\n") ;		// TODO remove
			owuartTimeout = 1u ;
			return 0xFF ;
		}
	}
	return ((unsigned char)(OWUART->RDR & 0xFF)) ;
}

//--------------------------------------------------------------------------------
