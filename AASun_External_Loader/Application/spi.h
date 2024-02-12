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

#define	flashSpi	SPI1

#ifdef __cplusplus
extern "C" {
#endif

void		spiInit					(SPI_TypeDef * pSpi) ;
void		spiSetBaudRate			(SPI_TypeDef * pSpi, uint32_t br) ;

// Full duplex API
void		spiModeDuplex			(SPI_TypeDef * pSpi) ;
uint32_t	spiTxRxByte				(SPI_TypeDef * pSpi, uint32_t byte) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// SPI_H_

