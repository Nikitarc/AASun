/*
----------------------------------------------------------------------

	Alain Chebrou

	w25q.c		Winbond W25Qxx SPI flash library (up to W25Q128)
				Only the minimum required do read/write to the flash

	When		Who	What
	23/02/23	ac	Creation

----------------------------------------------------------------------
*/

#include "aa.h"
#include "aakernel.h"	// For critical section
#include "gpiobasic.h"

#include "spi.h"
#include "w25q.h"

// The SPI to use to access the flash is defined as flashSpi in spi.h

//--------------------------------------------------------------------------------
// Some W25Q commands

#define W25Q_READ_ID			0x9F
#define W25Q_WRITE_ENABLE		0x06
#define W25Q_WRITE_DISABLE		0x04
#define W25Q_READ_SR1			0x05
#define W25Q_READ_SR2			0x35
#define W25Q_READ_SR3			0x15
#define W25Q_WRITE_SR1			0x01
#define W25Q_WRITE_SR2			0x31
#define W25Q_WRITE_SR3			0x11
#define W25Q_READ				0x03
#define W25Q_PAGE_PROGRAM		0x02
#define W25Q_SECTOR_ERASE		0x20
#define W25Q_BLOCK32_ERASE		0x52
#define W25Q_BLOCK64_ERASE		0xD8
#define W25Q_CHIP_ERASE			0xC7
#define W25Q_POWERDOWN			0XB9
#define W25Q_POWERDOWN_RELEASE	0xAB
#define W25Q_RESET_ENABLE		0x66
#define W25Q_RESET				0x99

#define	W25Q_SR1_BUSY			0x01	// Erase/Write in Progress
#define	W25Q_SR1_WEL			0x02	// Write Enable Latch

#define	W25Q_BLOCK_SIZE			(64*1024)

//--------------------------------------------------------------------------------
// GPIO definition

#define	CS_PORT		GPIOA		// Flash chip select
#define	CS_PIN		15

// All this is resolved into constants at compile time to get the minimal code.

#define	csSet()		CS_PORT->BSRR = (1u << (CS_PIN + 16))		// Set chip select to active   state: 0
#define	csClear()	CS_PORT->BSRR = (1u << CS_PIN)				// Set chip select to inactive state: 1

#define	W25Q_SPI_DIV		LL_SPI_BAUDRATEPRESCALER_DIV2		// To get 32 MHz from 64 MHz

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	This doesn't initialize the GPIO or the SPI
//	It configure the SPI for FLASH communication: the SPI is shared among many devices

void	W25Q_SpiTake 	(void)
{
	spiTake        (flashSpi) ;
	spiSetBaudRate (flashSpi, W25Q_SPI_DIV) ;
	spiModeDuplex  (flashSpi) ;
}

void	W25Q_SpiGive	(void)
{
	spiGive (flashSpi) ;
}

void	W25Q_Init (void)
{
	csClear () ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Returns JEDEC device ID

uint32_t	W25Q_ReadDeviceId	(void)
{
	uint32_t	id ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_READ_ID) ;
	id = spiTxRxByte (flashSpi, 0) ;
	id = (id << 8) | spiTxRxByte (flashSpi, 0) ;
	id = (id << 8) | spiTxRxByte (flashSpi, 0) ;
	csClear () ;

	return id ;
}

//--------------------------------------------------------------------------------

void	W25Q_WaitWhileBusy (void)
{
	uint32_t	sr1 ;

	// CS deselect time after erase or program to read status register: 50 ns min
	bspDelayUs (5) ;

	csSet () ;

	spiTxRxByte (flashSpi, W25Q_READ_SR1) ;
	do
	{
		aaTaskDelay (1) ;
		sr1 = spiTxRxByte (flashSpi, 0) ;
	} while ((sr1 & W25Q_SR1_BUSY) != 0u) ;

	csClear () ;
}

//--------------------------------------------------------------------------------
//	Returns 1  if the flash is erase/write busy, else 0

uint32_t	W25Q_IsBusy	(void)
{
	uint32_t	sr1 ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_READ_SR1) ;
	sr1 = spiTxRxByte (flashSpi, 0) ;
	csClear () ;

	return ((sr1 & W25Q_SR1_BUSY) != 0u) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------
// Returns 1 if write is enabled, else 0

uint32_t	W25Q_WriteEnable (void)
{
	uint32_t	sr1 ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_WRITE_ENABLE) ;
	csClear () ;

	sr1 = W25Q_ReadSR1 () ;
	return ((sr1 & W25Q_SR1_WEL) != 0u) ? 1 : 0 ;
}

//--------------------------------------------------------------------------------

void	W25Q_WriteDisable	(void)
{
	csSet () ;
	spiTxRxByte (flashSpi, W25Q_WRITE_DISABLE) ;
	csClear () ;
}

//--------------------------------------------------------------------------------

uint32_t	W25Q_ReadSR1 (void)
{
	uint32_t	sr1 ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_READ_SR1) ;
	sr1 = spiTxRxByte (flashSpi, 0) ;
	csClear () ;

	return sr1 ;
}

//--------------------------------------------------------------------------------

void	W25Q_WriteSR1 (uint8_t sr1)
{
	W25Q_WriteEnable () ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_WRITE_SR1) ;
	spiTxRxByte (flashSpi, sr1) ;
	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------

void	W25Q_Read	(void * pBuffer, uint32_t address, uint32_t byteCount)
{
	uint8_t		* ptr = (uint8_t *) pBuffer ;
	uint32_t	ii ;

	AA_ASSERT (pBuffer != 0 &&  byteCount != 0) ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_READ) ;
	spiTxRxByte (flashSpi, (address >> 16) & 0xFF) ;
	spiTxRxByte (flashSpi, (address >>  8) & 0xFF) ;
	spiTxRxByte (flashSpi, (address & 0xFF)) ;

	for (ii = 0 ; ii < byteCount ; ii++)
	{
		* ptr++ = spiTxRxByte (flashSpi, 0x55) ;
	}

	csClear () ;
}

//--------------------------------------------------------------------------------
//	Pages addresses are aligned on page size
//	The address range to write must not cross a page boundary
//	The byte count is <= page size
//	Write time: up to 3ms

void	W25Q_WritePage	(const uint8_t * pBuffer, uint32_t address, uint32_t byteCount)
{
	const uint8_t		* ptr = pBuffer ;
	uint32_t	ii ;

	W25Q_WriteEnable () ;

	csSet () ;
	spiTxRxByte (flashSpi, W25Q_PAGE_PROGRAM) ;
	spiTxRxByte (flashSpi, (address >> 16) & 0xFF) ;
	spiTxRxByte (flashSpi, (address >>  8) & 0xFF) ;
	spiTxRxByte (flashSpi, (address & 0xFF)) ;

	for (ii = 0 ; ii < byteCount ; ii++)
	{
		spiTxRxByte (flashSpi, * ptr++) ;
	}

	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------
//	Don't forget to erase the sector or the block before write

void	W25Q_Write	(const void * pBuffer, uint32_t address, uint32_t byteCount)
{
	uint32_t		addr    = address ;
	uint32_t		remain  = byteCount ;
	const uint8_t	* pData = (const uint8_t *) pBuffer ;
	uint32_t		nn ;

	// Write 1st page
	nn = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE) ;		// Count of bytes to write to this page
	if (nn > remain)
	{
		nn = remain ;
	}
	W25Q_WritePage (pData, addr, nn) ;
	remain -= nn ;
	addr   += nn ;
	pData  += nn ;

	// Write full pages, except perhaps the last one
	while (remain != 0u)
	{
		nn = W25Q_PAGE_SIZE ;		// Count of bytes to write to this page
		if (nn > remain)
		{
			nn = remain ;
		}
		W25Q_WritePage (pData, addr, nn) ;
		remain -= nn ;
		addr   += nn ;
		pData  += nn ;
	}
}

//--------------------------------------------------------------------------------
//	address must be a multiple of W25Q_SECTOR_SIZE
//	Erase time from 60 to 400 ms

void		W25Q_EraseSector	(uint32_t address)
{
	AA_ASSERT ((address & (W25Q_SECTOR_SIZE - 1u)) == 0u) ;

	W25Q_WriteEnable () ;
	csSet () ;
	spiTxRxByte (flashSpi, W25Q_SECTOR_ERASE) ;
	spiTxRxByte (flashSpi, (address >> 16) & 0xFF) ;
	spiTxRxByte (flashSpi, (address >>  8) & 0xFF) ;
	spiTxRxByte (flashSpi, (address & 0xFF)) ;
	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------
//	address must be a multiple of 32k
//	Erase time from 120 to 1600 ms

void		W25Q_EraseBlock32	(uint32_t address)
{
	AA_ASSERT ((address & ((32*1024) - 1u)) == 0u) ;

	W25Q_WriteEnable () ;
	csSet () ;
	spiTxRxByte (flashSpi, W25Q_BLOCK32_ERASE) ;
	spiTxRxByte (flashSpi, (address >> 16) & 0xFF) ;
	spiTxRxByte (flashSpi, (address >>  8) & 0xFF) ;
	spiTxRxByte (flashSpi, (address & 0xFF)) ;
	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------
//	address must be a multiple of 64k
//	Erase time from 150 to 2000 ms

void		W25Q_EraseBlock64	(uint32_t address)
{
	AA_ASSERT ((address & ((64*1024) - 1u)) == 0u) ;

	W25Q_WriteEnable () ;
	csSet () ;
	spiTxRxByte (flashSpi, W25Q_BLOCK64_ERASE) ;
	spiTxRxByte (flashSpi, (address >> 16) & 0xFF) ;
	spiTxRxByte (flashSpi, (address >>  8) & 0xFF) ;
	spiTxRxByte (flashSpi, (address & 0xFF)) ;
	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------
//	Erase time up to 100 s

void		W25Q_EraseChip		(void)
{
	W25Q_WriteEnable () ;
	csSet () ;
	spiTxRxByte (flashSpi, W25Q_CHIP_ERASE) ;
	csClear () ;
	W25Q_WaitWhileBusy () ;
}

//--------------------------------------------------------------------------------


