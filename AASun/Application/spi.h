/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	spi.h		SPI for AASun

	When		Who	What
	07/12/22	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined SPI_H_
#define SPI_H_
//--------------------------------------------------------------------------------

#include "stm32g0xx_ll_spi.h"
#include "stdbool.h"

#define	displaySpi			SPI1	// For display, Flash,  extension
#define	flashSpi			SPI1

#define	w5500Spi			SPI2	// For W5500

#define	SPI_DISPLAY_BRDIV	LL_SPI_BAUDRATEPRESCALER_DIV8
#define	SPI_FLASH_BRDIV		LL_SPI_BAUDRATEPRESCALER_DIV2


#ifdef __cplusplus
extern "C" {
#endif

void		spiInit					(SPI_TypeDef * pSpi) ;
void		spiTake					(SPI_TypeDef * pSpi) ;
bool		spiTryTake				(SPI_TypeDef * pSpi) ;
void		spiGive					(SPI_TypeDef * pSpi) ;

void		spiSetBaudRate			(SPI_TypeDef * pSpi, uint32_t br) ;

// Tx only API
void		spiModeTxOnly			(SPI_TypeDef * pSpi) ;
void		spiTxByte				(SPI_TypeDef * pSpi, uint32_t byte) ;
void		spiTxByteFast			(SPI_TypeDef * pSpi, uint32_t byte) ;
void		spiWaitTxEnd			(SPI_TypeDef * pSpi) ;

// Full duplex API
void		spiModeDuplex			(SPI_TypeDef * pSpi) ;
uint32_t	spiTxRxByte				(SPI_TypeDef * pSpi, uint32_t byte) ;

// Screen DMA (TX only)
void		spiScreenDmaInit		(void) ;
void		spiScreenDmaXfer		(uint32_t bufferSize, uint8_t * pBuffer) ;

// W5500 DMA (TX only)
void		spiW5500DmaInit			(void) ;
void		spiW5500WriteDmaXfer	(uint32_t bufferSize, uint8_t * pBuffer) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// SPI_H_

