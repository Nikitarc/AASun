/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	spi.c		SPI for AASun

	When		Who	What
	07/12/22	ac	Creation
	06/09/23	ac	Set pull down on W5500 MISO (get data 0 when the chip is physically absent)

----------------------------------------------------------------------
*/

#include "aa.h"
#include "gpiobasic.h"
#include "rccbasic.h"
#include "dmabasic.h"
#include "spi.h"

#include	"stm32g0xx_ll_dma.h"
#include	"stm32g0xx_ll_bus.h"

//--------------------------------------------------------------------------------
// The GPIO used by the display, Flash and expansion on SPI1
// The display doesn't use MISO

static const gpioPinDesc_t spi1GpioDesc [] =
{
	{	'B',	5u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_VERYHIGH   },	// MOSI
	{	'B',	4u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_VERYHIGH   },	// MISO
	{	'A',	5u,		AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_VERYHIGH   },	// SCK
	{	'D',	0u,		AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT_PP_UP | AA_GPIO_SPEED_HIGH },	// Display CS
	{	'D',	1u,		AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW },			// DC
	{	'D',	2u,		AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW },			// RST
	{	'A',	15u,	AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW },			// Flash CS
} ;

// The count of GPIO descriptors in spi1GpioDesc
static const uint32_t	spi1GpioCount = sizeof (spi1GpioDesc) / sizeof (gpioPinDesc_t) ;

// The GPIO used by the W5500 Ethernet on SPI2
static const gpioPinDesc_t spi2GpioDesc [] =
{
	{	'B',	15u,	AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// MOSI
	{	'B',	14u,	AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// MISO
	{	'B',	13u,	AA_GPIO_AF_0,	AA_GPIO_MODE_ALT_PP | AA_GPIO_SPEED_HIGH   },		// SCK
	{	'C',	7u,		AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW	},			// W5500 RST
	{	'C',	6u,		AA_GPIO_AF_0,	AA_GPIO_MODE_OUTPUT | AA_GPIO_SPEED_LOW	},			// W5500 CS
} ;

// The count of GPIO descriptors in spi2GpioDesc
static const uint32_t	spi2GpioCount = sizeof (spi2GpioDesc) / sizeof (gpioPinDesc_t) ;

// The semaphore to share SPI1: Flash, display, expansion
aaSemId_t 	spi1SemId = AA_INVALID_SEM ;

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

		if (spi1SemId == AA_INVALID_SEM)
		{
			// Initialize only once
			if (AA_ENONE != aaSemCreate	(1, & spi1SemId))
			{
				AA_ASSERT (0) ;
			}
		}
	}
	else
	{
		for (ii = 0 ; ii < spi2GpioCount ; ii++)
		{
			gpioConfigurePin (& spi2GpioDesc [ii]) ;
		}
		{
			// Set pull down on MISO, so when the W5500 is physically absent we get 0
			GPIO_TypeDef	* pGpio = gpioPortFromName (spi2GpioDesc[1].port) ;
			uint32_t		pin     = spi2GpioDesc[1].pin * 2u ;
			uint32_t		temp    = pGpio->PUPDR ;

			temp &= ~(GPIO_PUPDR_PUPD0 << pin) ;
			temp |= 2 << pin ;
			pGpio->PUPDR = temp ;

		}
	}

	// Enable the SPI clock, then reset => all configuration registers to reset values
	rccEnableClock (pSpi, 0u) ;
	rccResetPeriph (pSpi, 0u) ;

	// Configure SPI
	spiDef.TransferDirection = LL_SPI_FULL_DUPLEX ;
	spiDef.Mode              = LL_SPI_MODE_MASTER ;
	spiDef.DataWidth         = LL_SPI_DATAWIDTH_8BIT ;
	spiDef.ClockPolarity     = LL_SPI_POLARITY_LOW ;
	spiDef.ClockPhase        = LL_SPI_PHASE_1EDGE ;
	spiDef.NSS               = LL_SPI_NSS_SOFT ;
	spiDef.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV8 ;	// To get 8 MHz from 64 MHz
	spiDef.BitOrder          = LL_SPI_MSB_FIRST ;
	spiDef.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE ;
	spiDef.CRCPoly           = 0 ;
	LL_SPI_Init (pSpi, & spiDef);

	LL_SPI_SetRxFIFOThreshold (pSpi, LL_SPI_RX_FIFO_TH_QUARTER) ;
	LL_SPI_Enable (pSpi) ;
}

//--------------------------------------------------------------------------------

void		spiTake	(SPI_TypeDef * pSpi)
{
	if (pSpi == flashSpi)
	{
		// This SPI is shared so take the semaphore
		 aaSemTake	(spi1SemId, AA_INFINITE) ;
	}
	else
	{
		AA_ASSERT (0) ;		// Invalid pSpi
	}
}

bool		spiTryTake	(SPI_TypeDef * pSpi)
{
	bool res = false  ;

	if (pSpi == flashSpi)
	{
		// This SPI is shared so try to take the semaphore
		res = aaSemTryTake (spi1SemId) == AA_ENONE ;
	}
	else
	{
		AA_ASSERT (0) ;		// Invalid pSpi
	}
	return res ;	// true if the semaphore is taken, else false
}

void	spiGive	(SPI_TypeDef * pSpi)
{
	if (pSpi == flashSpi)
	{
		// This SPI is shared so release the semaphore
		aaSemGive (spi1SemId) ;
	}
	else
	{
		AA_ASSERT (0) ;		// Invalid pSpi
	}
}

//--------------------------------------------------------------------------------
//	Set the SPI baud rate according the target
//	br is the clock divider value, not a frequency

void spiSetBaudRate (SPI_TypeDef * pSpi, uint32_t br)
{
	pSpi->CR1 = (pSpi->CR1 & ~SPI_CR1_BR) | br ;
}

//--------------------------------------------------------------------------------
// The OLED display doesn't use MISO, so set TX only SPI mode

void spiModeTxOnly (SPI_TypeDef * pSpi)
{
	pSpi->CR1 |= (SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE) ;
}

//--------------------------------------------------------------------------------
// The Flash use the full SPI, so set duplex SPI mode

void spiModeDuplex (SPI_TypeDef * pSpi)
{
	pSpi->CR1 = pSpi->CR1 & ~(SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE) ;
}

//--------------------------------------------------------------------------------
// Write a byte and wait for the end of transmission
// TX only mode (without receive)

void spiTxByte (SPI_TypeDef * pSpi, uint32_t byte)
{
	while (LL_SPI_GetTxFIFOLevel (pSpi) == LL_SPI_TX_FIFO_FULL)
	{
	}

	* ((__IO uint8_t *) & pSpi->DR) = byte ;

	while ((pSpi->SR & SPI_SR_BSY) != 0)
	{
	}
}

//--------------------------------------------------------------------------------
// Write a byte in the SPI FIFO without waiting for the end of transmission
// TX only mode (without receive)

void spiTxByteFast (SPI_TypeDef * pSpi, uint32_t byte)
{
	while (LL_SPI_GetTxFIFOLevel (pSpi) == LL_SPI_TX_FIFO_FULL)
	{
	}

	* ((__IO uint8_t *) & pSpi->DR) = byte ;
}

//--------------------------------------------------------------------------------
// Wait for the end of transmission

void spiWaitTxEnd (SPI_TypeDef * pSpi)
{
	while ((pSpi->SR & SPI_SR_BSY) != 0)
	{
	}
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
//--------------------------------------------------------------------------------
//	Screen DMA initialization: memory to SPI
//	Doesn't use interrupt for short burst (128us)

#define	SCR_DMA_CHANNEL		LL_DMA_CHANNEL_4

void	spiScreenDmaInit (void)
{
	LL_AHB1_GRP1_EnableClock (LL_AHB1_GRP1_PERIPH_DMA1) ;

	LL_DMA_ConfigTransfer  (DMA1, SCR_DMA_CHANNEL,
							LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
							LL_DMA_MODE_NORMAL                |
							LL_DMA_PERIPH_NOINCREMENT         |
							LL_DMA_MEMORY_INCREMENT           |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_MDATAALIGN_BYTE            |
							LL_DMA_PRIORITY_MEDIUM) ;

	// Select SPI1 as DMA transfer request
	// Only 1 DMA on this MCU so DMA channel is also MUX channel
	((dmaMux_t *) DMAMUX1)->CCR [SCR_DMA_CHANNEL] = LL_DMAMUX_REQ_SPI1_TX ;

}

//--------------------------------------------------------------------------------
// Start DMA then wait TC
// CS must be set before this function call
// SPI should not be disabled/enabled because it causes noise on the CLK signal

void	spiScreenDmaXfer (uint32_t bufferSize, uint8_t * pBuffer)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [SCR_DMA_CHANNEL] ;
	uint32_t		tcMask ;

	// Set DMA data parameter
	pStream->CNDTR = bufferSize ;
	pStream->CMAR  = (uint32_t) pBuffer ;
	pStream->CPAR  = (uint32_t) & SPI1->DR ;

	// Clear DMA channel flags
	pDma->IFCR = DMA_FLAG_ALLIF << (SCR_DMA_CHANNEL << 2u) ;

	// Enable the DMA transfer
	pStream->CCR |= DMA_CCR_EN ;

	// Enable SPI DMA => this starts the transfer
	SPI1->CR2 |= SPI_CR2_TXDMAEN ;

	// Wait for the end of DMA (~128 us)
	tcMask = 1 << (1 + (4 * SCR_DMA_CHANNEL)) ;		// TC bit in ISR
	while ((pDma->ISR & tcMask) == 0)
	{
	}

	// Wait for end of SPI transfer
	while ((SPI1->SR & SPI_SR_FTLVL) != 0u)
	{
	}
	while ((SPI1->SR & SPI_SR_BSY) != 0u)
	{
	}

	// Disable DMA channel
	pStream->CCR &= ~DMA_CCR_EN ;

	// Disable SPI DMA
	SPI1->CR2 &= ~SPI_CR2_TXDMAEN ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	W5500 Write DMA initialization: memory to SPI
//	Doesn't use interrupt

#define	W5500_WR_DMA_CHANNEL		LL_DMA_CHANNEL_5

void	spiW5500DmaInit (void)
{
	LL_AHB1_GRP1_EnableClock (LL_AHB1_GRP1_PERIPH_DMA1) ;

	LL_DMA_ConfigTransfer  (DMA1, W5500_WR_DMA_CHANNEL,
							LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
							LL_DMA_MODE_NORMAL                |
							LL_DMA_PERIPH_NOINCREMENT         |
							LL_DMA_MEMORY_INCREMENT           |
							LL_DMA_PDATAALIGN_BYTE            |
							LL_DMA_MDATAALIGN_BYTE            |
							LL_DMA_PRIORITY_MEDIUM) ;

	// Select w5500Spi as DMA transfer request
	// Only 1 DMA on this MCU so DMA channel is also MUX channel
	((dmaMux_t *) DMAMUX1)->CCR [W5500_WR_DMA_CHANNEL] = LL_DMAMUX_REQ_SPI2_TX ;

}

//--------------------------------------------------------------------------------
// Start DMA then wait TC
// CS must be set before this function call
// SPI should not be disabled/enabled because it causes noise on the CLK signal

void	spiW5500WriteDmaXfer (uint32_t bufferSize, uint8_t * pBuffer)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	dmaStream_t		* pStream = & pDma->stream [W5500_WR_DMA_CHANNEL] ;
	uint32_t		tcMask ;

	// Set DMA data parameter
	pStream->CNDTR = bufferSize ;
	pStream->CMAR  = (uint32_t) pBuffer ;
	pStream->CPAR  = (uint32_t) & w5500Spi->DR ;

	// Clear DMA channel flags
	pDma->IFCR = DMA_FLAG_ALLIF << (W5500_WR_DMA_CHANNEL << 2u) ;

	// Enable the DMA transfer
	pStream->CCR |= DMA_CCR_EN ;

	// Enable SPI DMA => this starts the transfer
	w5500Spi->CR2 |= SPI_CR2_TXDMAEN ;

	// Wait for the end of DMA (~128 us)
	tcMask = 1 << (1 + (4 * W5500_WR_DMA_CHANNEL)) ;		// TC bit in ISR
	while ((pDma->ISR & tcMask) == 0)
	{
	}

	// Wait for end of SPI transfer
	while ((w5500Spi->SR & SPI_SR_FTLVL) != 0u)
	{
	}
	while ((w5500Spi->SR & SPI_SR_BSY) != 0u)
	{
	}

	// Disable DMA channel
	pStream->CCR &= ~DMA_CCR_EN ;

	// Disable SPI DMA
	w5500Spi->CR2 &= ~SPI_CR2_TXDMAEN ;

	// Empty RX FIFO
	while ((w5500Spi->SR & SPI_SR_RXNE) != 0)
	{
		(void) w5500Spi->DR ;
	}
}
