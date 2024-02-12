/*
----------------------------------------------------------------------

	Alain Chebrou

	w25q.h		Winbond W25Qxx SPI flash library  (up to W25Q128)

	When		Who	What
	23/02/23	ac	Creation

----------------------------------------------------------------------
*/
#if ! defined W25Q_H_
#define W25Q_H_
//--------------------------------------------------------------------------------

// Specific to AASun flash loader
#define	W25Q_FLASH_ADDRESS			(1024*1024)		// The 1st MB is reserved
#define	W25Q_FLASH_SIZE				(1024*1024*7)	// 64Mbits => 8MBytes, the 1st MB is reserved
//#define	W25Q_FLASH_SIZE				(1024*128)		// For test: 2 blocs, 32 sectors

// Specific to W25Q64
#define	W25Q_BLOCK32_SIZE			(32*1024)
#define	W25Q_BLOCK64_SIZE			(64*1024)
#define	W25Q_SECTOR_SIZE			4096			// Smallest erasable piece
#define	W25Q_PAGE_SIZE				256

#ifdef __cplusplus
extern "C" {
#endif

void		W25Q_Init			(void) ;

uint32_t	W25Q_ReadDeviceId	(void) ;
uint32_t	W25Q_ReadSR1		(void) ;
void		W25Q_WriteSR1		(uint8_t sr1) ;
uint32_t	W25Q_WriteEnable	(void) ;
void		W25Q_WriteDisable	(void) ;
void		W25Q_Read			(void * pBuffer, uint32_t address, uint32_t byteCount) ;
void		W25Q_Write			(const void * pBuffer, uint32_t address, uint32_t byteCount) ;
void		W25Q_WritePage		(const uint8_t * pBuffer, uint32_t address, uint32_t byteCount) ;
void		W25Q_EraseSector	(uint32_t address) ;
void		W25Q_EraseBlock32	(uint32_t address) ;
void		W25Q_EraseBlock64	(uint32_t address) ;
void		W25Q_EraseChip		(void) ;
void		W25Q_WaitWhileBusy	(void) ;
uint32_t	W25Q_IsBusy			(void) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// W25Q_H_
