/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfsExtract.c	Extracts the components of an MFS filesystem image and reconstructs the tree structure.
					This allows you to check, with WinMerge for example, that the files have not been altered.

	When		Who	What
	05/22/23	ac	Creation

----------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#if defined _MSC_VER
	#include	<direct.h>
	#include	"getopt.h"
	#include	"dirent.h"
	#define		mkdir(path)		_mkdir (path)
	#define		chdir(path)		SetCurrentDirectory (path)
#else
	#include	<unistd.h>
	#include	<sys/stat.h>
	#include	<sys/types.h>
	#define		mkdir(path)		mkdir (path, 0777)
#endif

#include	"mfs.h"

//--------------------------------------------------------------------------------

// The MFS image file to read 
char		* srcFile = "AASun_web.bin" ;
FILE		* imgFs ;

// The folder to create, which will contain the folder tree from the MFS image
char		* dstPath = "extracted" ;

// Used to copy files from MFS to the new folder tree
FILE		* dstFile ;
uint8_t		readBuffer [512] ;

uint32_t	dirCount, fileCount ;

//--------------------------------------------------------------------------------
// This function reads in the MFS file, it is part of mfs context

int simRead (void * userData, uint32_t address, void * pBuffer, uint32_t size)
{
	fseek (imgFs, address, SEEK_SET) ;
	fread (pBuffer, 1, size, imgFs) ;
	return MFS_ENONE ;
}

//--------------------------------------------------------------------------------
// Extract files and rebuild folders
// Recursive function

void	walkDir (mfsCtx_t * pCtx, const char * dirPath)
{
	mfsError_t		err ;
	mfsDir_t		dir ;
	mfsDirEntry_t	entry ;
	char			currentPath [PATH_MAX] ;

	err = mfsDirOpen (pCtx, dirPath, & dir) ;
	if (err != MFS_ENONE)
	{
		printf ("mfsDirOpen error: %d\n", err) ;
		return ;
	}
	while (mfsDirRead (pCtx, & dir, & entry) == MFS_ENONE)
	{
		// Build the path for this entry.
		strcpy (currentPath, dirPath) ;
		// Avoid '//' at the begnning of the path
		if (currentPath [1] != 0)
		{
			strcat (currentPath, "/") ;
		}
		strcat (currentPath, entry.name) ;

		if ((entry.type & MFS_DIR) != 0)
		{
			// Create the directory, and make it current
			printf ("D          %s\n", currentPath) ;
			(void) mkdir (entry.name) ;	// Skip /
			(void) chdir (entry.name) ;

			walkDir (pCtx, currentPath) ;	// Recursive call!

			(void) chdir ("..") ;
			dirCount ++ ;
		}
		else
		{
			// Copy the file
			mfsFile_t	mfsFile ;
			int32_t		nn ;

			printf ("F %8d %s\n", entry.size, currentPath) ;
			err = mfsOpen (pCtx, & mfsFile, currentPath) ;
			if (err != MFS_ENONE)
			{
				printf ("mfsOpen error: %d\nAbort\n", err) ;
				exit (0) ;
			}

			// Open the destination file
			dstFile = fopen (entry.name, "wb+") ;
			if (dstFile == NULL)
			{
				printf ("Can't create file: %s\r\n", entry.name) ;
				exit (0) ;
			}

			while (0 != (nn = mfsRead (& mfsFile, readBuffer, sizeof (readBuffer))))
			{
				fwrite (readBuffer, 1, nn, dstFile) ;
			}
			fclose (dstFile) ;
			mfsClose (& mfsFile) ;
			fileCount ++ ;
		}
	}
}

//--------------------------------------------------------------------------------

void usage (void)
{
	printf ("usage: mfsExtract -i <input_image_file> -o <output_folder_to_create>\n") ;
	printf ("Default: -i %s  -o %s\n", srcFile, dstPath) ;
}

//--------------------------------------------------------------------------------

int	main (int argc, char ** argv)
{
	mfsError_t		err ;
	mfsCtx_t		mfsCtx ;
	char			c ;

	// Parse command line parameters
	while ((c = getopt(argc, argv, "i:o:b:?")) != -1)
	{
		switch (c)
		{
			case 'i':
				srcFile = optarg ;		// Source MFS image file
				break;

			case 'o':
				dstPath = optarg ;		// Destination directory path
				break;

			case '?':
				usage () ;
				return 0 ;
				break;
		}
	}
	if ((srcFile == NULL)  ||  (dstPath == NULL))
	{
		usage () ;
		return 0 ;
	}

	// Open the MFS image file
	imgFs = fopen (srcFile, "rb") ;
	if (imgFs == NULL)
	{
		printf ("Can't open image file: %s\r\n", srcFile) ;
		return 0 ;
	}

	// Initialize the MFS context and mount the filesystem
	mfsCtx.userData = NULL ;
	mfsCtx.read   = simRead ;
	mfsCtx.lock   = NULL ;
	mfsCtx.unlock = NULL ;


	err = mfsMount (& mfsCtx) ;
	if (err != MFS_ENONE)
	{
		printf ("mfsMount error: %d\nAbort\n", err) ;
		fclose (imgFs) ;
		return 0 ;
	}

	// Create the folder to receive the files from the MFS
	(void) _mkdir (dstPath) ;
	chdir (dstPath) ;

	// Extract MFS files
	dirCount  = 0 ;
	fileCount = 0 ;

	walkDir (& mfsCtx, "/") ;		// Extract the tree from the MFS filesystem

	chdir ("..") ;

	fclose (imgFs) ;
	printf ("Directory count: %8u\nFile count:      %8u\n", dirCount, fileCount) ;
	{
		uint32_t crc, size ;
		mfsGetCrc (& mfsCtx, & crc, & size) ;
		printf ("File size:       %8u\nFile CRC:      0x%08X\n", size, crc) ;
	}

	return 1 ;
}

//--------------------------------------------------------------------------------
