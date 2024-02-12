/*
----------------------------------------------------------------------

	AASun - External loader for web pages

	Alain Chebrou

	main.c		Entry point for tests

	When		Who	What
	21/05/23	ac	Creation V1.0
	30/06/23	ac	Clock source is HSI (was HSE). Some boards don't have HSE.

----------------------------------------------------------------------
*/

#include	"global.h"
#include	"gpiobasic.h"
#include	"w25q.h"
#include	"Dev_inf.h"
#include	"stm32g0xx_ll_usart.h"
#include	"stm32g0xx_ll_rcc.h"
#include	"stm32g0xx_ll_bus.h"
#include	"stm32g0xx_ll_utils.h"

#include	"string.h"	// For memset, memcmp

// The GPIO descriptor for the LED
const gpioPinDesc_t	ledDesc = { 'B',	12,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW } ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// aaTaskDelay() is used by w25q.c, assert_failed(), main()

void	initTick (void)
{
	LL_InitTick (__LL_RCC_CALC_HCLK_FREQ (SystemCoreClock, LL_RCC_GetAHBPrescaler()), 1000) ;	// For delays
}

void aaTaskDelay (uint32_t ms)
{
	LL_mDelay (ms) ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// main() is not used when compiled as external loader: WITH_MAIN 0
// main() is only used when compiled for test:          WITH_MAIN 1

// BEWARE: When you change WITH_MAIN value
//         don't forget to set the according linker script (in project linker properties)

#if (WITH_MAIN == 0)

	int main (void)
	{
		while (1)
		{
		}
	}

#else

static	uint8_t		buffer_test   [W25Q_SECTOR_SIZE] ;
static	uint8_t		sectorBuffer  [W25Q_SECTOR_SIZE] ;

// When compiled as standard application, main() allows to exercise
// all the API functions of the external loader

int main (void)
{
	uint32_t	var ;
	uint32_t	sectorAddress ;

	__disable_irq () ;
	Init () ;

	// This is a test of the Loader_Src functions and the Flash driver

	// Initialize the flash sector data
	for (var = 0 ; var < W25Q_SECTOR_SIZE ; var++)
	{
		buffer_test [var] = var & 0xff ;
	}
/*
	// SectorErase() test, erase all sectors
	uartPuts    ("Sector erase ") ;
	uartPrintX  (StorageInfo.DeviceStartAddress, 9) ;
	uartPrintX  (StorageInfo.DeviceStartAddress+StorageInfo.DeviceSize-1, 9) ;
	uartPutChar ('\n') ;

	SectorErase (StorageInfo.DeviceStartAddress, StorageInfo.DeviceStartAddress+StorageInfo.DeviceSize-1) ;
	uartPuts ("Erase OK\n") ;
*/
	// Mass erase test
	MassErase () ;
	uartPuts  ("\nOK\n") ;

	// Write sectors test
	sectorAddress = StorageInfo.DeviceStartAddress ;
	for (var = 0 ; var < W25Q_FLASH_SIZE / W25Q_SECTOR_SIZE ; var++)
	{
		if (Write (sectorAddress, W25Q_SECTOR_SIZE, buffer_test) != LOADER_OK)
		{
			uartPuts ("Write error\n") ;
			AA_ASSERT (0) ; // Error detected, LED blink fast
		}
		sectorAddress += W25Q_SECTOR_SIZE ;
		gpioPinToggle (& ledDesc) ;
	}
	uartPutChar ('\n') ;

	// Verify Flash data
	sectorAddress = StorageInfo.DeviceStartAddress ;
  	for (var = 0 ; var < W25Q_FLASH_SIZE / W25Q_SECTOR_SIZE ; var++)
  	{
  		W25Q_Read (sectorBuffer, sectorAddress, W25Q_SECTOR_SIZE) ;
  		if (memcmp (buffer_test, sectorBuffer, W25Q_SECTOR_SIZE) != 0)
  		{
			uartPuts ("Verify  error\n") ;
  			AA_ASSERT (0) ;		// Error detected, LED blink fast
  		}
  		gpioPinToggle (& ledDesc) ;
  		uartPutChar ('-') ;
	}
  	uartPutChar ('\n') ;
	uartPuts ("Test OK\n") ;
  	// End of test


  	// If test OK then the LED blinks slowly
	while (1)
	{
		aaTaskDelay (500) ;
		gpioPinToggle (& ledDesc) ;
	}
}
#endif

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Simple polling driver for USART2
//  Used only when WITH_MAIN is 1


#if (WITH_MAIN == 0)

void uartInit (void)
{
}

void uartPutChar (char cc)
{
	(void) cc;
}

void uartPuts (char * pStr)
{
	(void) pStr;
}

void	uartPrintI (int32_t value)
{
	(void) value;
}

void	uartPrintX (uint32_t value, uint32_t width)
{
	(void) value;
	(void) width;
}

#else

#define	BAUD_RATE	57600

static const gpioPinDesc_t	uartTxDesc = { 'A', 2, 1, AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL | AA_GPIO_SPEED_LOW } ;
static const gpioPinDesc_t	uartRxDesc = { 'A', 3, 1, AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL | AA_GPIO_SPEED_LOW } ;

//--------------------------------------------------------------------------------
// The LL_USART_Init function is huge, 600 bytes, but easy to use...

void uartInit (void)
{
	LL_USART_InitTypeDef USART_InitStruct = { 0 } ;

	LL_APB1_GRP1_EnableClock  (LL_APB1_GRP1_PERIPH_USART2) ;
	LL_APB1_GRP1_ForceReset   (LL_APB1_GRP1_PERIPH_USART2) ;
	LL_APB1_GRP1_ReleaseReset (LL_APB1_GRP1_PERIPH_USART2) ;

	gpioConfigurePin (& uartTxDesc) ;
	gpioConfigurePin (& uartRxDesc) ;

	USART_InitStruct.PrescalerValue			= LL_USART_PRESCALER_DIV1 ;
	USART_InitStruct.BaudRate				= BAUD_RATE ;
	USART_InitStruct.DataWidth				= LL_USART_DATAWIDTH_8B ;
	USART_InitStruct.StopBits				= LL_USART_STOPBITS_1 ;
	USART_InitStruct.Parity					= LL_USART_PARITY_NONE ;
	USART_InitStruct.TransferDirection		= LL_USART_DIRECTION_TX_RX ;
	USART_InitStruct.HardwareFlowControl	= LL_USART_HWCONTROL_NONE ;
	USART_InitStruct.OverSampling			= LL_USART_OVERSAMPLING_16 ;
	LL_USART_Init (USART2, & USART_InitStruct) ;

	USART2->CR1 |= USART_CR1_UE ;			// Enable USART
	// Wait for TEAK and REAK flags set
	while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) != (USART_ISR_TEACK | USART_ISR_REACK))
	{
	}
}

//--------------------------------------------------------------------------------
// Send 1 char to UART

void uartPutChar (char cc)
{
	volatile USART_TypeDef	* uartBase = USART2 ;	// UARTx
	uint32_t				flagLF = 0u ;

	// If cc is LF, then send CR-LF
	if (cc == '\n')
	{
		flagLF = 1u ;
		cc = '\r' ;
	}

	uartBase->TDR = (uint16_t) cc ;
	// Wait until the FIFO is not full
	while ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0u)
	{
	}

	if (flagLF != 0u)
	{
		uartBase->TDR = '\n' ;
		while ((uartBase->ISR & USART_ISR_TXE_TXFNF) == 0u)
		{
		}
	}
}

//--------------------------------------------------------------------------------
//	Send 0 terminated string

void uartPuts (char * pStr)
{
	while (* pStr != 0)
	{
		uartPutChar (* pStr++) ;
	}
}

//--------------------------------------------------------------------------------
// Simple decimal number print without format

void	uartPrintI (int32_t value)
{
	char		str [12] ;
	char		* ptr ;
	char		chr ;
	uint32_t	neg = 0 ;

	if (value < 0)
	{
		neg = 1 ;
		value = - value ;
	}

	ptr = & str [sizeof (str) - 1] ;
	* ptr = 0 ;
	do
	{
		chr = (char) ((value % 10) + '0') ;
		*--ptr = chr ;
		value /= 10 ;
	} while (value != 0) ;

	if (neg != 0)
	{
		uartPutChar ('-') ;
	}

	while (* ptr != 0)
	{
		uartPutChar (* ptr++) ;
	}
}

//--------------------------------------------------------------------------------
// Simple hex number print with user defined width

void	uartPrintX (uint32_t value, uint32_t width)
{
	char		str [12] ;
	char		* ptr ;
	char		chr ;

	ptr = & str [sizeof (str) - 1] ;
	* ptr = 0 ;
	do
	{
		chr = (char) ((value & 0x0F) + '0') ;
		if (chr > '9')
		{
			chr += 7 ;
		}
		*--ptr = chr ;
		value >>= 4 ;
		if (width > 0)
		{
			width--;
		}
	} while (value != 0) ;

	while (width > 0)
	{
		*--ptr = ' ' ;
		width-- ;
	}

	while (* ptr != 0)
	{
		uartPutChar (* ptr++) ;
	}
}

#endif // WITH_MAIN

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	LL Support functions

//--------------------------------------------------------------------------------
//	AA_ASSERT support function

void	bspAssertFailed (uint8_t * pFileName, uint32_t line)
{
	(void) pFileName ;
	(void) line ;

	// Check if debug is enabled.
	// If debug is enabled, use bkpt instruction to jump to debugger
	// If debug is not enabled, a bkpt instruction triggers a HardFault exception
	#if (__CORTEX_M != 0)
	{
		// No CoreDebug on M0 / M0+
		if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u)
		{
			// Debug enabled, can use BKPT instruction
			BSP_DEBUG_BKPT () ;
		}
	}
	#endif

	// No debugger: loop with fast LED blink
	__disable_irq () ;
	while (1)
	{
		aaTaskDelay (200) ;
		gpioPinToggle (& ledDesc) ;
	}
}

//--------------------------------------------------------------------------------
// Support for STMicroelectronics stdperiph, HAL or LL code (stm32_assert.h)
// Not the same assert_failed() prototype in all families...
// For F4 / F1 / G0

void assert_failed (uint8_t * file, uint32_t line)
{
	bspAssertFailed (file, line) ;
}

//--------------------------------------------------------------------------------
