/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfsTest.c	Some examples using MFS

	When		Who	What
	05/31/23	ac	Creation

----------------------------------------------------------------------
*/

#include	<stdint.h>
#include	<stdio.h>
#include	"mfs.h"

//--------------------------------------------------------------------------------

// The MFS read function to set in the MFS context

static	int simRead (void * userData, uint32_t address, void * pBuffer, uint32_t size) ;

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
	mfsCtx.userData = NULL ;
	mfsCtx.lock     = NULL ;
	mfsCtx.unlock   = NULL ;
	mfsCtx.read     = simRead ;

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
		fwrite (readBuffer, 1, nn, stdout) ;
		fileSize += nn;
	}
	printf (">\nBytes read: %d\n", fileSize) ;	// End marker

	mfsClose  (& mfsFile) ;
	mfsUmount (& mfsCtx) ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	This is to simulate the MFS Flash.
//	The M filesystem is a file on Windows/Linux

static	char		* imageMfs = "image.bin" ;		// The filesystem to use 
static	FILE		* imgFs ;

static	int simRead (void * userData, uint32_t address, void * pBuffer, uint32_t size)
{
	(void) userData  ; 
	fseek (imgFs, address, SEEK_SET) ;
	fread (pBuffer, 1, size, imgFs) ;
	return MFS_ENONE ;
}

//	The files to dump:

const char	* filePath1 = "/fonts/fonts2" ;
const char	* filePath2 = "js/npm.js" ;
const char	* filePath3 = "css/ie10-viewport-bug-workaround.css" ;

const char	* filePath [] =
{
//	"/fonts/fonts2",
	"js/npm.js",
	"/css/ie10-viewport-bug-workaround.css",
} ;

const	uint32_t	filePathCount = sizeof (filePath) / sizeof (char *) ;

//--------------------------------------------------------------------------------
//	On Windows/Linux the M filesystem is a file

int	jsmnTest (void) ;

int	main ()
{
	// Open the MFS filesystem image
	imgFs = fopen (imageMfs, "rb") ;
	if (imgFs == NULL)
	{
		printf ("Can't open image file: %s\r\n", imageMfs) ;
		return 0 ;
	}

	for (uint32_t ii = 0 ; ii < filePathCount ; ii++)
	{
		dumpFile (filePath [ii]) ;
		printf ("\n-------------------------------\n") ;
	}

	fclose (imgFs) ;
}

//--------------------------------------------------------------------------------
