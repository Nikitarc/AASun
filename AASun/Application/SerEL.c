/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	SerEl.c		Write HTTP file system to the external flash

	When		Who	What
	10/10/22	ac	Creation
	18/02/23	ac	port to PCB V1
	04/09/23	ac	Use modified PCB V1: Linky on LPUART1 and pin PB10 =>
	 	 	 	 	allows to use Linky AND pulse counter 2 AND temperature sensors
	01/05/24	ac	Doesn't use malloc anymore

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aaprintf.h"

#include	<stdlib.h>		// For strtoul()

#include	"AASun.h"
#include	"SerEL.h"
#include	"w25q.h"		// Flash

//--------------------------------------------------------------------------------

typedef union
{
	messHeader_t		mesHeader ;		// Header or messages without data: ack, terminate...
	messErase_t			messErase ;
	messReadReq_t		messReadReq ;
	messReadData_t		messReadData ;
	messWrite_t			messWrite ;

} messUnion_t ;

//--------------------------------------------------------------------------------

static	void	writeCom (void * pBuffer, uint32_t count)
{
	uint8_t	* ptr = pBuffer ;

	for (uint32_t ii = 0 ; ii < count ; ii++)
	{
		aaPutChar (* ptr++) ;
	}
}

static	void	readCom (void * pBuffer, uint32_t count)
{
	uint8_t	* ptr = pBuffer ;

	for (uint32_t ii = 0 ; ii < count ; ii++)
	{
		* ptr++ = aaGetChar () ;
	}
}

static	bool	sendAck ()
{
	messHeader_t	mess ;

	mess.messId = ackMessId ;
	mess.length = 0 ;
	writeCom (& mess, sizeof (mess)) ;
	return true ;
}

//--------------------------------------------------------------------------------

void	SerEL (void)
{
	uint8_t		* pData = getWizBuffer () ;
	messUnion_t	mess ;

	aaTaskDelay (1000) ;	// wait until all chars are echoed (empty uart buffer)

	// Switch the serial baud rate to safe value
	// Without LFCRLF and with interrupt
//	uartInit (AA_UART_CONSOLE, 19200, 0, & aaConsoleHandle) ;

	// Remove LFCRLF flag from the UART driver
	uartSetFlag	(aaGetDefaultConsole ()->hDev, UART_FLAG_LFCRLF, 0) ;

	// Send Ack
	sendAck () ;

	// Wait for command
	while (1)
	{
		// Read the message header (without message data)
		readCom (& mess, sizeof (messHeader_t)) ;

		if (mess.mesHeader.messId == closeMessId)
		{
			sendAck () ;
			aaTaskDelay (1000) ;	// Allows char transmission
			break ;		// Session ended
		}

		else if (mess.mesHeader.messId == readReqMessId)
		{
			uint32_t	size ;

			// Read message data
			readCom (& mess.messReadReq.data, mess.mesHeader.length) ;
			size = mess.messReadReq.data.size ;		// Data size to read from flash

			// Read data from flash
			W25Q_SpiTake () ;
			W25Q_Read (pData, mess.messReadReq.data.address, size) ;
			W25Q_SpiGive () ;

			// Send data
			mess.messReadData.header.messId = readDataMessId ;
			mess.messReadData.header.length = size ;
			writeCom (& mess.messReadData, sizeof (mess.messReadData)) ;
			writeCom (pData, size) ;
		}

		else if (mess.mesHeader.messId == eraseMessId)
		{
			uint32_t	size ;
			uint32_t	address, sectorAddress ;

			// Read message data
			readCom (& mess.messErase.data, mess.mesHeader.length) ;
			address = mess.messErase.data.address ;		// Data address in flash
			size    = mess.messErase.data.size ;		// Byte size to erase

			// Compute 1st sector address and sector count
			sectorAddress = address & ~(W25Q_SECTOR_SIZE - 1) ; 	// Down round to sector size
			size += address - sectorAddress ;
			size = (size + (W25Q_SECTOR_SIZE - 1)) / W25Q_SECTOR_SIZE ;		// Sector count

			// Erase sectors
			for (uint32_t ii = 0 ; ii < size ; ii++)
			{
				W25Q_SpiTake () ;
				W25Q_EraseSector (sectorAddress) ;
				W25Q_SpiGive () ;
				sectorAddress += W25Q_SECTOR_SIZE ;
			}
			sendAck () ;
		}

		else if (mess.mesHeader.messId == writeMessId)
		{
			uint32_t	size ;
			uint32_t	address ;

			// Read message header data
			readCom (& mess.messWrite.data, mess.mesHeader.length) ;
			address = mess.messWrite.data.address ;		// Data address in flash
			size    = mess.messWrite.data.size ;		// Data size to write to flash

			// Receive data to write to flash
			readCom (pData, size) ;

			// Write to flash
			W25Q_SpiTake () ;
			W25Q_Write	(pData, address, size) ;
			W25Q_SpiGive () ;
			sendAck () ;
		}
	}
}

//--------------------------------------------------------------------------------

