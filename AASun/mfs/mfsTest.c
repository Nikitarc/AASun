/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfsTest.c	An example using MFS on microcontroller

	When		Who	What
	05/31/23	ac	Creation

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"mfs.h"

#define		printf		aaPrintf

//--------------------------------------------------------------------------------

#include	"w25q.h"		// Flash

// The user provided function to set in the MFS context

static	int mfsDevRead (void * userData, uint32_t address, void * pBuffer, uint32_t size)
{
	W25Q_Read (pBuffer, address + (uint32_t) userData, size) ;
	return MFS_ENONE ;
}

static	void mfsDevLock (void * userData)
{
	(void) userData ;
	W25Q_SpiTake () ;
}

static	void mfsDevUnlock (void * userData)
{
	(void) userData ;
	W25Q_SpiGive () ;
}

//--------------------------------------------------------------------------------
// Example of reading an MFS file

void	dumpFile (const char * filePath)
{
	mfsCtx_t	mfsCtx ;
	mfsFile_t	mfsFile ;
	mfsStat_t	fileStat ;
	mfsError_t	err ;
	uint32_t	nn, fileSize ;
	uint8_t		readBuffer [80] ;

	// Initialize the MFS context
	// userData is used to provide the starting address of the file system in the FLASH
	mfsCtx.userData = (void *) 0x100000 ;
	mfsCtx.lock     = mfsDevLock ;
	mfsCtx.unlock   = mfsDevUnlock ;
	mfsCtx.read     = mfsDevRead ;

	// Mount the MFS
	err = mfsMount (& mfsCtx) ;
	if (err != MFS_ENONE)
	{
		printf ("mfsMount error: %d\nAbort\n", err) ;
		return ;
	}

	// Test if the file exist
	err = mfsStat (& mfsCtx, filePath, & fileStat) ;
	if (err != MFS_ENONE)
	{
		printf ("mfsStat error: %d\nAbort\n", err) ;
		return ;
	}
	printf ("Information from: mfsStat (\"%s\")\n", filePath) ;
	printf ("  Type %s\n", ((fileStat.type & MFS_DIR) != 0) ? "DIR" : ((fileStat.type & MFS_FILE) != 0) ? "FILE" : "???") ;
	printf ("  Size %d\n\n", fileStat.size) ;

	if ((fileStat.type & MFS_DIR) == MFS_DIR)
	{
		printf ("Can't dump a directory\n") ;
		return ;
	}

	// Open the MFS file
	err = mfsOpen (& mfsCtx, & mfsFile, filePath) ;
	if (err != MFS_ENONE)
	{
		printf ("msfOpen error: %d\nAbort\n", err) ;
		return ;
	}

	// Dump the file
	printf ("File content:\n") ;
	fileSize = 0 ;
	printf ("<") ;	// Start marker
	while (0 != (nn = mfsRead (& mfsFile, readBuffer, sizeof (readBuffer))))
	{
		for (uint32_t ii = 0 ; ii < nn ; ii++)
		{
			aaPutChar (readBuffer [ii]) ;
		}
		fileSize += nn;
	}
	printf (">\nBytes read: %d\n", fileSize) ;	// End marker

	mfsClose  (& mfsFile) ;
	mfsUmount (& mfsCtx) ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	The files to dump:

const char	* filePath [] =
{
	"/fonts/fonts2",
	"/js/npm.js",
	"/css/ie10-viewport-bug-workaround.css",
} ;

const	uint32_t	filePathCount = sizeof (filePath) / sizeof (char *) ;

//--------------------------------------------------------------------------------

void	mfsTest (void)
{
	for (uint32_t ii = 0 ; ii < filePathCount ; ii++)
	{
		dumpFile (filePath [ii]) ;
		printf ("\n-------------------------------\n") ;
	}
}

//--------------------------------------------------------------------------------
