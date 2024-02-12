
#include	"global.h"
#include	"w25q.h"
#include	"Dev_inf.h"

#include	<string.h>					// memset

/**
 * @brief  System initialization.
 * @param  None
 * @retval  LOADER_OK = 1	: Operation succeeded
 * @retval  LOADER_FAIL = 0	: Operation failed
 */

// The Init function is mentioned by Entry Point in the linker script
// and directly called by STM32CubeProgrammer, so the startup code is not executed

int Init (void)
{

	*(uint32_t*) 0xE000EDF0 = 0xA05F0000 ; // enable interrupts in debug

    // Init .bss section to Zero
    memset (& _sbss, 0, & _ebss - & _sbss);

	SystemInit () ;

#if (WITH_MAIN == 0)
/* ADAPTATION TO THE DEVICE
 *
 * change VTOR setting for H7 device
 * SCB->VTOR = 0x24000000 | 0x200;
 *
 * change VTOR setting for other devices
 * SCB->VTOR = 0x20000000 | 0x200;
 */
	// Only if compiled as external loader
	SCB->VTOR = 0x20000000 | 0x200 ;
#endif

	bspSystemClockInit () ;
	initTick  () ;
	W25Q_Init () ;
	uartInit  () ;

	uartPuts    ("I ") ;
	uartPrintX  ((uint32_t) (& StorageInfo), 9) ;	// This avoid the .Dev_info section removing
	uartPuts    (VERSION) ;
	uartPutChar ('\n') ;

	return LOADER_OK ;
}

/**
 * @brief   Program memory.
 * @param   Address: page address
 * @param   Size   : size of data
 * @param   buffer : pointer to data buffer
 * @retval  LOADER_OK = 1	: Operation succeeded
 * @retval  LOADER_FAIL = 0	: Operation failed
 */
int Write(uint32_t Address, uint32_t Size, uint8_t* buffer)
{
	// Page (266 B) write time: 0.7 - 3 ms

	uartPutChar ('W') ;	uartPrintX  (Address, 9) ; uartPrintX  (Size, 9) ; uartPutChar ('\n') ;

	W25Q_Write (buffer, Address, Size) ;
	return LOADER_OK;
}

int Read(uint32_t Address, uint32_t Size, uint8_t *Buffer)
{
	uartPuts ("R") ;	uartPrintX  (Address, 9) ; uartPrintX  (Size, 9) ; uartPutChar ('\n') ;

	W25Q_Read (Buffer, Address, Size) ;
	return LOADER_OK;
}

/**
 * @brief   Sector erase.
 * @param   EraseStartAddress :  erase start address
 * @param   EraseEndAddress   :  erase end address
 * @retval  LOADER_OK = 1		: Operation succeeded
 * @retval  LOADER_FAIL = 0	: Operation failed
 */
int SectorErase (uint32_t EraseStartAddress, uint32_t EraseEndAddress)
{
	// 4K sector erase time: 60 - 400 ms
	uartPuts ("SE") ;	uartPrintX  (EraseStartAddress, 9) ; uartPrintX  (EraseEndAddress, 9) ; uartPutChar ('\n') ;

	while (EraseStartAddress <= EraseEndAddress)
	{
		uartPutChar ('E') ; uartPrintX  (EraseStartAddress, 9) ; uartPutChar ('\n') ;
		W25Q_EraseSector (EraseStartAddress) ;
		EraseStartAddress += W25Q_SECTOR_SIZE ;
	}
	return LOADER_OK;
}

/**
 * Description :
 * Mass erase of external flash area
 * Optional command - delete in case usage of mass erase is not planed
 * Inputs    :
 *      none
 * outputs   :
 *     none
 * Note: Optional for all types of device
 */
int MassErase (void)
{
	// Do not erase all the Flash: the 1st MB is reserved
	// Erase using 64K blocs
	// 64K block erase time: 150 - 2000 ms

	uint32_t	EraseStartAddress = W25Q_FLASH_ADDRESS ;
	uint32_t	EraseEndAddress   = EraseStartAddress + W25Q_FLASH_SIZE ;

	uartPuts ("Mass") ; uartPrintX  (EraseStartAddress, 9) ; uartPrintX  (EraseEndAddress, 9) ; uartPutChar ('\n') ;

	while (EraseStartAddress < EraseEndAddress)
	{
		W25Q_EraseBlock64 (EraseStartAddress) ;
		EraseStartAddress += W25Q_BLOCK64_SIZE ;
		uartPutChar ('M') ; uartPrintX  (EraseStartAddress, 9) ; uartPutChar ('\n') ;
	}
	return LOADER_OK;
}

#if (0)
/**
 * Description :
 * Calculates checksum value of the memory zone
 * Inputs    :
 *      StartAddress  : Flash start address
 *      Size          : Size (in WORD)
 *      InitVal       : Initial CRC value
 * outputs   :
 *     R0             : Checksum value
 * Note: Optional for all types of device
 */
uint32_t CheckSum (uint32_t StartAddress, uint32_t Size, uint32_t InitVal)
{
	uint8_t missalignementAddress = StartAddress % 4;
	uint8_t missalignementSize = Size;
	uint32_t cnt;
	uint32_t Val;

	StartAddress -= StartAddress % 4;
	Size += (Size % 4 == 0) ? 0 : 4 - (Size % 4);

	for (cnt = 0; cnt < Size; cnt += 4)
	{
		Val = *(uint32_t*) StartAddress;
		if (missalignementAddress)
		{
			switch (missalignementAddress)
			{
			case 1:
				InitVal += (uint8_t) (Val >> 8 & 0xff);
				InitVal += (uint8_t) (Val >> 16 & 0xff);
				InitVal += (uint8_t) (Val >> 24 & 0xff);
				missalignementAddress -= 1;
				break;
			case 2:
				InitVal += (uint8_t) (Val >> 16 & 0xff);
				InitVal += (uint8_t) (Val >> 24 & 0xff);
				missalignementAddress -= 2;
				break;
			case 3:
				InitVal += (uint8_t) (Val >> 24 & 0xff);
				missalignementAddress -= 3;
				break;
			default:
				break ;
			}
		}
		else if ((Size - missalignementSize) % 4 && (Size - cnt) <= 4)
		{
			switch (Size - missalignementSize)
			{
			case 1:
				InitVal += (uint8_t) Val;
				InitVal += (uint8_t) (Val >> 8 & 0xff);
				InitVal += (uint8_t) (Val >> 16 & 0xff);
				missalignementSize -= 1;
				break;
			case 2:
				InitVal += (uint8_t) Val;
				InitVal += (uint8_t) (Val >> 8 & 0xff);
				missalignementSize -= 2;
				break;
			case 3:
				InitVal += (uint8_t) Val;
				missalignementSize -= 3;
				break;
			default:
				break ;
			}
		}
		else
		{
			InitVal += (uint8_t) Val;
			InitVal += (uint8_t) (Val >> 8 & 0xff);
			InitVal += (uint8_t) (Val >> 16 & 0xff);
			InitVal += (uint8_t) (Val >> 24 & 0xff);
		}
		StartAddress += 4;
	}

	return (InitVal);
}

/**
 * Description :
 * Verify flash memory with RAM buffer and calculates checksum value of
 * the programmed memory
 * Inputs    :
 *      FlashAddr     : Flash address
 *      RAMBufferAddr : RAM buffer address
 *      Size          : Size (in WORD)
 *      InitVal       : Initial CRC value
 * outputs   :
 *     R0             : Operation failed (address of failure)
 *     R1             : Checksum value
 * Note: Optional for all types of device
 */
BSP_ATTR_USED uint64_t Verify (uint32_t MemoryAddr, uint32_t RAMBufferAddr, uint32_t Size, uint32_t missalignement)
{
	uint32_t VerifiedData = 0, InitVal = 0;
	uint64_t checksum;
	Size *= 4;

	checksum = CheckSum((uint32_t) MemoryAddr + (missalignement & 0xf), Size - ((missalignement >> 16) & 0xF), InitVal);
	while (Size > VerifiedData)
	{
		if (*(uint8_t*) MemoryAddr++  !=  *((uint8_t*) RAMBufferAddr + VerifiedData))
		{
			return ((checksum << 32) + (MemoryAddr + VerifiedData));
		}
		VerifiedData++;
	}

	return (checksum << 32);
}
#endif
