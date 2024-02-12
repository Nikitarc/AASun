/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	spi.c		SPI for AASun

	When		Who	What
	07/12/22	ac	Creation

----------------------------------------------------------------------
*/

#include "global.h"
#include "gpiobasic.h"
#include "spi.h"

#include "stm32g0xx_ll_bus.h"

//--------------------------------------------------------------------------------
// The GPIO used by the Flash

static const gpioPinDesc_t spi1GpioDesc [] =
{
	{	'B',	5u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// MOSI
	{	'B',	4u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// MISO
	{	'A',	5u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// SCK
	{	'A',	15u,	AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW	   },		// Flash CS
} ;

// The count of GPIO descriptors in spi1GpioDesc
static const uint32_t	spi1GpioCount = sizeof (spi1GpioDesc) / sizeof (gpioPinDesc_t) ;

//--------------------------------------------------------------------------------
// Initialize a SPI

void	spiInit (SPI_TypeDef * pSpi)
{
	uint32_t			ii ;
	LL_SPI_InitTypeDef	spiDef ;

	// Initialize GPIO
	if (pSpi == SPI1)
	{
		for (ii = 0 ; ii < spi1GpioCount ; ii++)
		{
			gpioConfigurePin (& spi1GpioDesc [ii]) ;
		}

		// Enable the SPI clock, then reset => all configuration registers to reset values
		LL_APB2_GRP1_EnableClock  (LL_APB2_GRP1_PERIPH_SPI1) ;
		LL_APB2_GRP1_ForceReset   (LL_APB2_GRP1_PERIPH_SPI1) ;
		LL_APB2_GRP1_ReleaseReset (LL_APB2_GRP1_PERIPH_SPI1) ;
	}
	else
	{
		return ;
	}

	// Configure SPI
	spiDef.TransferDirection = LL_SPI_FULL_DUPLEX ;
	spiDef.Mode              = LL_SPI_MODE_MASTER ;
	spiDef.DataWidth         = LL_SPI_DATAWIDTH_8BIT ;
	spiDef.ClockPolarity     = LL_SPI_POLARITY_LOW ;
	spiDef.ClockPhase        = LL_SPI_PHASE_1EDGE ;
	spiDef.NSS               = LL_SPI_NSS_SOFT ;
	spiDef.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV2 ;	// To get 32 MHz from 64 MHz
	spiDef.BitOrder          = LL_SPI_MSB_FIRST ;
	spiDef.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE ;
	spiDef.CRCPoly           = 0 ;
	LL_SPI_Init (pSpi, & spiDef);

	LL_SPI_SetRxFIFOThreshold (pSpi, LL_SPI_RX_FIFO_TH_QUARTER) ;
	LL_SPI_Enable (pSpi) ;
}

//--------------------------------------------------------------------------------
//	Set the SPI baud rate according the target
//	br is the clock divider value, not a frequency

void spiSetBaudRate (SPI_TypeDef * pSpi, uint32_t br)
{
	pSpi->CR1 = (pSpi->CR1 & ~SPI_CR1_BR) | br ;
}

//--------------------------------------------------------------------------------
// The Flash use the full SPI, so set duplex SPI mode

void spiModeDuplex (SPI_TypeDef * pSpi)
{
	pSpi->CR1 = pSpi->CR1 & ~(SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE) ;
}

//--------------------------------------------------------------------------------
// Write a byte and wait to receive a byte: duplex mode

uint32_t spiTxRxByte (SPI_TypeDef * pSpi, uint32_t byte)
{
	while ((pSpi->SR & SPI_SR_TXE) == 0u)
	{
	}

	* ((__IO uint8_t *) & pSpi->DR) = byte ;

	while ((pSpi->SR & SPI_SR_RXNE) == 0u)
	{
	}

	return * ((__IO uint8_t *) & pSpi->DR) ;
}
//--------------------------------------------------------------------------------
