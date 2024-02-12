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

#define	W25Q_PAGE_SIZE			256u
#define	W25Q_SECTOR_SIZE		4096u

#ifdef __cplusplus
extern "C" {
#endif

void		W25Q_Init			(void) ;
void		W25Q_SpiTake		(void) ;
void		W25Q_SpiGive		(void) ;

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
