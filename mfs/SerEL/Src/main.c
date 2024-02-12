

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "SerEL.h"
#include "getopt.h"

//------------------------------------------------------------------------------

#define		VERSION		"1.0"

#define		COMPORT		0
#define		BAUDRATE	57600
#define		BYTESIZE	8
#define		PARITY		NOPARITY

#define		TIMEOUT		5000		// ms

HANDLE		hComm ;					// The COM handle
uint32_t	comPort  = COMPORT ; 
uint32_t	baudRate = BAUDRATE ;
uint32_t	address  = 0x100000 ;
char		* filePath = "AASun_web.bin" ;

HANDLE		hFile ;					// The file handle
uint32_t	fileSize ;
bool		verifyOnly ;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

uint32_t	openCom ()
{
	char			name [32] ;
	DCB				dcb ;
	COMMTIMEOUTS	timeouts ;

	// Open COM port
	sprintf (name, "\\\\.\\COM%d", comPort) ;

	hComm = CreateFile(name,						// port name
                      GENERIC_READ | GENERIC_WRITE, // Read/Write
                      0,                            // No Sharing
                      NULL,                         // No Security
                      OPEN_EXISTING,				// Open existing port only
                      0,							// Non Overlapped I/O
                      NULL);						// Null for Comm Devices
	if (hComm == INVALID_HANDLE_VALUE)
	{
		return 0 ;
	}

	// Configure COM port
	GetCommState(hComm, & dcb) ;
	dcb.BaudRate	= baudRate ;	// Setting BaudRate
	dcb.ByteSize	= BYTESIZE ;	// Setting ByteSize
	dcb.StopBits	= ONESTOPBIT ;	// Setting StopBits = 1
	dcb.Parity		= PARITY ;		// Setting Parity
	dcb.fOutX		= 0 ;
	dcb.fInX		= 0 ;
	SetCommState (hComm, & dcb);

	// Configure timeouts
/*	timeouts.ReadIntervalTimeout         = 50; // in milliseconds
	timeouts.ReadTotalTimeoutConstant    = 50; // in milliseconds
	timeouts.ReadTotalTimeoutMultiplier  = 10; // in milliseconds
*/
	// No timeout on read: return immediately if nothing to read
	timeouts.ReadIntervalTimeout         =  MAXDWORD; // in milliseconds
	timeouts.ReadTotalTimeoutConstant    =  0; // in milliseconds
	timeouts.ReadTotalTimeoutMultiplier  =  0; // in milliseconds
	timeouts.WriteTotalTimeoutConstant   = 50; // in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; // in milliseconds
	SetCommTimeouts (hComm, & timeouts) ;

	// Configure the buffer size
/*
	if (SetupComm (hComm, 0x40000, 2048) == 0)
	{
		printf ("SetupComm error: %d\n", GetLastError ()) ;
	}
*/
	return 1 ;
}

//------------------------------------------------------------------------------
// Wait for serial data with timeout

bool	readCom (void * buffer, uint32_t count)
{
	DWORD		bytesReadCount ;
	uint32_t	len = 0 ; 
	uint32_t	timeout = TIMEOUT ;

	while (len != count && timeout != 0)
	{
		bytesReadCount = 0 ;
		if (0 != ReadFile (hComm, (uint8_t *) buffer+len, count-len, & bytesReadCount, NULL))
		{
			if (bytesReadCount != 0)
			{
				len += bytesReadCount ;
				continue ;
			}
		}
		Sleep (20) ;
		timeout -= 20 ;
	}
	if (timeout == 0)
	{
		printf ("readCom timeout\n") ;
	}
	return len == count ;
}

//------------------------------------------------------------------------------
// Used to empty the internal com receive buffer

void	readFlushCom (void)
{
	DWORD		bytesReadCount ;
	char		cc ; 

	while (1)
	{
		bytesReadCount = 0 ;
		if (0 != ReadFile (hComm, & cc, 1, & bytesReadCount, NULL))
		{
			if (bytesReadCount != 0)
			{
//printf ("%02X ", cc & 0xFF) ;
				continue ;
			}
		}
		break ;
	}
//putchar ('\n') ;
}

//------------------------------------------------------------------------------
//	Send data over serial com

bool	writeCom (void * buffer, uint32_t count)
{
	DWORD		bytesWrittenCount ;

	WriteFile (hComm, buffer, count, & bytesWrittenCount, NULL) ;
	if (count != bytesWrittenCount)
	{
		printf ("WriteCom error: %u %u", count, bytesWrittenCount) ;
		return false ;
	}
	return true ;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//	Wait for acknowledge from the target

bool waitAck ()
{
	messHeader_t	mess ;

	memset (& mess, 0 , sizeof (mess)) ;
	if (! readCom (& mess, sizeof (mess)))
	{
		return false ;
	}
//	printf ("Ack %s\n", mess.messId == ackMessId ? "Ok" : "Ko") ;
	return mess.messId == ackMessId ;
}

//------------------------------------------------------------------------------
//	Send flash sector erase request to the target

bool sendErase (uint32_t addr, uint32_t size)
{
	messErase_t		mess ; 

	mess.header.messId = eraseMessId ;
	mess.header.length = sizeof (mess.data) ;
	mess.data.address  = addr ;
	mess.data.size     = size ; 

	if (! writeCom (& mess, sizeof (mess)))
	{
		printf ("Error sendErase\n") ;
		return false ;
	}

	if (! waitAck ())
	{
		printf ("Error sendErase waitAck\n") ;
		return false ;
	}
	return true ;
}

//------------------------------------------------------------------------------
// Send a command to write some data to the target

bool sendWrite (uint32_t addr, void * buffer, uint32_t size)
{
	messWrite_t		mess ; 

	mess.header.messId = writeMessId ;
	mess.header.length = sizeof (mess.data) ;
	mess.data.address  = addr ;
	mess.data.size     = size ; 

	if (! writeCom (& mess, sizeof (mess)))
	{
		printf ("Error sendWrite header\n") ;
		return false ;
	}

	if (! writeCom (buffer, size))
	{
		printf ("Error sendWrite data\n") ;
		return false ;
	}

	if (! waitAck ())
	{
		printf ("Error sendWrite waitAck\n") ;
		return false ;
	}

	return true ;
}

//------------------------------------------------------------------------------
//	Send a command to request some flash data to the target
//	Then receive that data

bool sendRead (uint32_t addr, void * buffer, uint32_t size)
{
	messReadReq_t	messRequest ; 
	messReadData_t	messData ;

	// Send the read request
	messRequest.header.messId = readReqMessId ;
	messRequest.header.length = sizeof (messRequest.data) ;
	messRequest.data.address  = addr ;
	messRequest.data.size     = size ; 

	if (! writeCom (& messRequest, sizeof (messRequest)))
	{
		printf ("sendRead writeCom error\n"); 
		return false ;
	}

	// Receive read data message header
	if (! readCom (& messData, sizeof (messData)))
	{
		printf ("sendRead readCom error\n"); 
		return false ;
	}
	if (messData.header.messId != readDataMessId || messData.header.length != size)
	{
		printf ("sendRead readCom id:%u / size:%u error\n", messData.header.messId, messData.header.length); 
		return false; 
	}

	// Receive read data
	if (! readCom (buffer, messData.header.length))
	{
		printf ("sendRead readCom data error\n"); 
		return false ;
	}
	
	return true ;
}

//------------------------------------------------------------------------------
//	Send a command to close the session
//	Then wait for the acknowledge
//	The target will reboot

bool sendClose (void)
{
	messHeader_t		mess ; 

	mess.messId = closeMessId ;
	mess.length = 0 ;

	writeCom (& mess, sizeof (mess)) ;

	waitAck () ;
	return true ;
}

//------------------------------------------------------------------------------
//	Send the command to switch the target in External Loader mode
//	Then wait for the acknowledge

bool	sendSwitch (void)
{
	// Send the command
	if (! writeCom ("\rSerEL\r", 7))
	{
		printf ("Error Switching to SerEL\n") ;
		return false ;
	}
	// Wait until the command chars are echoed by the target, then discards
	Sleep (500) ;
	readFlushCom () ;

	if (! waitAck ())
	{
		printf ("Error Ack Switching to SerEL\n") ;
		return false ;
	}
	return true ;
}

//------------------------------------------------------------------------------
//	Write the file to the target external flash

bool download (void)
{
	uint32_t	len, size, addr ;
	uint8_t		data [BLOCK_SIZE] ;
	DWORD		bytesReadCount ;

	// Send flash sectors erase message
	printf ("Start erasing\n") ;
	if (! sendErase (address, fileSize))
	{
		return false ;
	}

	// Send file data
	printf ("Start download\n") ;
	size = fileSize ;
	addr = address ;
	while (size > 0)
	{
		printf ("sendWrite 0x%X %d\n", addr, fileSize - size) ;
 
		// Read a block od data from the file
		len = size ;
		if (len > BLOCK_SIZE)
		{
			len = BLOCK_SIZE ;
		}
		(void) ReadFile (hFile, data, len, & bytesReadCount, NULL) ;

		// Send the block of data to thetarget
		if (! sendWrite (addr, data, bytesReadCount))
		{
			return false ;
		}

		size -= bytesReadCount ;
		addr += bytesReadCount ;
	}
	return true ;
}

//------------------------------------------------------------------------------

bool verify (void)
{
	uint32_t	len, size, addr ;
	uint8_t		flashData [BLOCK_SIZE] ;
	uint8_t		fileData  [BLOCK_SIZE] ;
	DWORD		bytesReadCount ;

	{
		LARGE_INTEGER	li ;
		li.QuadPart = 0 ; 
		SetFilePointerEx (hFile, li, NULL, FILE_BEGIN) ;
	}

	printf ("Start verify\n") ;
	size = fileSize ;
	addr = address ;
	while (size > 0)
	{
		printf ("Verify 0x%X %d\n", addr, fileSize - size) ;

		// Read data from file
		len = size ;
		if (len > BLOCK_SIZE)
		{
			len = BLOCK_SIZE ;
		}
		(void) ReadFile (hFile, fileData, len, & bytesReadCount, NULL) ;

		// Get data from flash
		if (! sendRead (addr, flashData, len))
		{
			return false ;
		}

		// Verify
		if (memcmp (fileData, flashData, len) != 0)
		{
			printf ("Verify error\n") ;
			return false ;
		}

		size -= bytesReadCount ;
		addr += bytesReadCount ;
	}
	return true ;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void usage (void)
{
	printf ("usage: SerEL -c <com_port> -b <baudrate> -a <memory_address> -f <file_path>\n") ;
	printf ("Default: -c %u  -b %u  -a 0x%X -f %s\n", comPort, baudRate, address, filePath) ;
}

//------------------------------------------------------------------------------
//	Parse the command line parameters

bool	parse (int argc, char * argv[])
{
	int					c ;

	while ((c = getopt (argc, argv, "vc:b:a:f:?")) != -1)
	{
		switch (c)
		{
			case 'v':
				verifyOnly = true ;
				break;

			case 'c':
				comPort = strtoul (optarg, NULL, 0) ;
				break;

			case 'b':
				baudRate = strtoul (optarg, NULL, 0) ;
				break;

			case 'a':
				address = strtoul (optarg, NULL, 0) ;
				break;

			case 'f':
				filePath = optarg ;		// Source file path
				break;

			case '?':
				usage () ;
				return 0 ;
				break;
		}
	}
	if (filePath == NULL)
	{
		usage () ;
		return false ;
	}
	return true ;
}

//------------------------------------------------------------------------------

int main (int argc, char * argv[])
{

	printf ("Serial External Loader V%s\n\n", VERSION) ;

	if (parse (argc, argv) == false)
	{
		return 0 ;
	}
	printf ("Port           COM%u\n",  comPort) ;
	printf ("Baud rate      %u\n",     baudRate) ;
	printf ("Memory address 0x%08X\n", address) ;
	printf ("File           %s\n",     filePath) ;

	// Open file ans get its size
	hFile = CreateFile(filePath,		// port name
                      GENERIC_READ,		// Read/Write
                      0,                // No Sharing
                      NULL,             // No Security
                      OPEN_EXISTING,	// Open existing port only
                      0,				// Non Overlapped I/O
                      NULL);			// hTemplateFile
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf ("Error opening file\n") ;
		return 0 ;
	}
 	fileSize = GetFileSize (hFile, NULL) ;
	printf ("File size      %d\n",     fileSize) ;

	// Open the serial link
	if (openCom () != 1)
	{
		printf ("Error opening COM%d\n", comPort) ;
		CloseHandle (hFile) ;
		return 0 ;
	}

	// Switch target to serial external loader
	if (! sendSwitch ())
	{
		return 0 ;
	}

	if (! verifyOnly)
	{
		if (download ())
		{
			printf ("Write Ok\n") ;
			if (verify ())
			{
				printf ("Verify Ok\n") ;
			}
		}
	}
	else
	{
		if (verify ())
		{
			printf ("Verify Ok\n") ;
		}
	}

	// Terminate SerEL session
	sendClose () ;

	// Close file and serial link
	CloseHandle (hFile) ;
	if (hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle (hComm) ;
	}
	printf ("Terminated\n") ;
	return 1 ;

}

//------------------------------------------------------------------------------
