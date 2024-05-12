/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfsBuild.c	Build a Minimalistic read only file system

	When		Who	What
	05/22/23	ac	Creation

----------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#if defined _MSC_VER
	#include	"getopt.h"
	#include	"dirent.h"
#else
	#include <unistd.h>
#endif

#include	"mfs.h"

// in crc.c
uint32_t crc32Init	(void) ;
uint32_t crc32		(uint32_t crc, const char * s, size_t n) ;

//--------------------------------------------------------------------------------


// Information to remember when creating a folder
typedef struct
{
	uint32_t		blockNumFirst ;		// Block offset in the FS of the 1st block of this directory (to be used as parent for child dir)
	uint32_t		blockNum ;			// Block offset in the FS of the current block of this directory
	uint32_t		offset ;			// Offset of next entry in the directory block
	mfsDirHdr_t		* pDir ;			// Typed pointer to pBlock
	uint8_t			* pBlock ;			// The directory block

} dirCtx_t ;

// Default arguments values
char			* src = "AASun_web" ;		// Source directory path
char			* dst = "AASun_web.bin" ;	// Destination image path

FILE			* dstFile ;

// The logical block size to use for the filesystem
// It is different from FLASH erase block size
uint32_t		blockSize   = 512 ;
uint32_t		blockPower2 ;			// 2^blockPower2 = blockSize

uint8_t			* pDataBlock ;
uint32_t		lastBlock ;				// This is always the bock num past the end of the file

mfsSuperBloc_t	* pSuperBloc ;

// Convert a block number to a physical address
#define			bloc2DirAddr(blocNum)		((mfsDirHdr_t *) (blocNum << blockPower2))		
#define			bloc2Addr(blocNum)			(blocNum << blockPower2)		

// Some statistics of the created filesystem
uint32_t		dirCount, fileCount ;

//--------------------------------------------------------------------------------
//  Count trailing 0 of value (value 0 is forbidden)
//	Return the value N of: value = 2^N,
//	Example: if value is 512 then ctz returns 8, and 512 = 2^8

static	uint32_t	ctz (uint32_t value)
{
	#if (defined __GNUC__)
		return __builtin_ctz (value) ;
	#else
		uint32_t n ; 
		_BitScanReverse (& n, value) ;
		return n ;
	#endif
}

//--------------------------------------------------------------------------------
//	Create a new dir context.
//  The dir block header is initialized to 0
//	blockNum is the 1st bloc for this directory

static	dirCtx_t *	newDirCtx (uint32_t blockNum)
{
	dirCtx_t	* pCtx ;

	pCtx = malloc (sizeof (dirCtx_t)) ;
	if (pCtx == NULL)
	{
		// To satisfy VS...
		printf ("Malloc error\n") ;
		exit (0) ;
	}
	pCtx->pBlock = malloc (blockSize) ;
	if (pCtx->pBlock == NULL)
	{
		// To satisfy VS...
		printf ("Malloc error\n") ;
		exit (0) ;
	}
	pCtx->pDir   = (mfsDirHdr_t *) pCtx->pBlock ;
	pCtx->offset = sizeof (mfsDirHdr_t) ;
	pCtx->blockNumFirst = blockNum ;
	pCtx->blockNum      = blockNum ;

	memset (pCtx->pDir, 0, sizeof (mfsDirHdr_t)) ;
	return pCtx ;
}

static	void	releaseDirCtx (dirCtx_t * pCtx)
{
	free (pCtx->pBlock) ;
	free (pCtx) ;
}

//--------------------------------------------------------------------------------
// Upade on disk a dir block which is already written
// Do not use to create a new dir block

static	void	updateDir (dirCtx_t * pDirCtx)
{
	int32_t pos ;

	pos = ftell (dstFile) ;
	fseek  (dstFile, bloc2Addr (pDirCtx->blockNum), SEEK_SET) ;
	fwrite (pDirCtx->pBlock, blockSize, 1, dstFile) ;
	fseek  (dstFile, pos, SEEK_SET) ;
}

//--------------------------------------------------------------------------------

static	mfsEntryHdr_t *	addEntry (dirCtx_t * pDirCtx, fileType_t type, char * pName, char * path)
{
	mfsEntryHdr_t	* pEntry ;
	uint32_t		len ;

	// Is there enough space for a new entry in the current dir block
	len = sizeof (mfsEntryHdr_t) + strlen (pName) + 1 ;	// +1 for final 0
	len = (len + 3) & ~0x03 ;		// Align len to multiple of 4
	if (len > MFS_ENTRY_SIZE_MAX)
	{
		printf ("Name too long: %s\nAbort\n", pName) ;
		exit (0) ;
	}

	if ((pDirCtx->offset + len) > blockSize)
	{
		// Not enough space: write current dir block then allocate a new block for this dir
		pDirCtx->pDir->pNext = bloc2DirAddr (lastBlock) ;	// Link to the next block
		updateDir (pDirCtx) ;								// Update current block

printf ("D+ %u\n", lastBlock) ;
		pDirCtx->pDir->pNext = NULL ;						// Initialize next block
		pDirCtx->pDir->pPrev = bloc2DirAddr (pDirCtx->blockNum) ;
		pDirCtx->pDir->count = 0 ;
		pDirCtx->blockNum    = lastBlock++ ;
		pDirCtx->offset      = sizeof (mfsDirHdr_t) ;

		// Reserve the place for the new current dir block in the file
		fwrite (pDirCtx->pBlock, blockSize, 1, dstFile) ;
	}

	// Add the entry
	pEntry = (mfsEntryHdr_t *) (pDirCtx->pBlock + pDirCtx->offset) ;
	pEntry->entrySize = len ;
	pEntry->flags     = type ;
	pEntry->fileSize  = -1 ;
	strcpy (pEntry->name, pName) ;
	pDirCtx->offset += len ;
	pDirCtx->pDir->count++ ;

	// Copy the file
	if (type == MFS_FILE)
	{
		FILE			* dataFile ;
		int32_t			nn ;
		int32_t			fileSize ;

		dataFile = fopen (path, "rb") ;
		if (dataFile == NULL)
		{
			printf ("Can't open file: %s\nAbort\n", path) ;
			exit (0) ;
		}
		pEntry->blockNum = lastBlock ;
		fileSize = 0 ;
		while (0 != (nn = fread (pDataBlock, 1, blockSize, dataFile)))
		{
			fwrite (pDataBlock, 1, blockSize, dstFile) ;	// Always write a full block length
			lastBlock++ ;
			fileSize += nn; 
		}
		pEntry->fileSize = fileSize ;
		fclose (dataFile) ;
		fileCount++ ;
	}

	return pEntry ;
}

//--------------------------------------------------------------------------------
// Walk the source tree to build the MFS image

void	build (dirCtx_t * pDirCtx, char * src)
{
	DIR				* dir ;
	struct dirent	* entry ;
	char			currentPath [PATH_MAX] ;

	dir = opendir (src) ;
	if (dir != NULL)
	{
		while ((entry = readdir (dir)))
		{
			// Skip . and ..
			if ((strcmp (entry->d_name, ".") == 0) || (strcmp (entry->d_name, "..") == 0))
			{
				continue ;
			}
			if (strlen (entry->d_name) >= MFS_NAME_MAX)
			{
				printf ("Name too long: %s\nAbors\n", entry->d_name) ;
				abort () ;
			}
			// Build the path of this entry
			strcpy (currentPath, src) ;
			strcat (currentPath, "/") ;
			strcat (currentPath, entry->d_name) ;
			if (entry->d_type == DT_REG)
			{
				printf ("F %s\n", currentPath) ;
				addEntry (pDirCtx, MFS_FILE, entry->d_name, currentPath) ;
			}
			else if (entry->d_type == DT_DIR)
			{
				dirCtx_t		* pNewDirCtx  ;
				mfsEntryHdr_t	* pEntry ;

				printf ("D %s\n", currentPath) ;
				pEntry = addEntry (pDirCtx, MFS_DIR, entry->d_name, currentPath) ;

				// New dir entry, set the 1st dir block 
				pEntry->blockNum = lastBlock ;

				pNewDirCtx = newDirCtx (lastBlock++) ;
				pNewDirCtx->pDir->pParent = bloc2DirAddr (pDirCtx->blockNum) ;
				fwrite (pNewDirCtx->pBlock, blockSize, 1, dstFile) ;
				build (pNewDirCtx, currentPath) ;
				updateDir (pNewDirCtx) ;
				releaseDirCtx (pNewDirCtx) ;
				dirCount++ ;
			}
			else
			{
			}
		}
	}
	updateDir (pDirCtx) ;
	closedir (dir) ; 
}

//--------------------------------------------------------------------------------

void usage (void)
{
	printf ("usage: mfsBuid -i <source_dir> -o <output_image_file> -b <block_size>\n") ;
	printf ("Default: -i %s  -o %s  -b %u\n", src, dst, blockSize) ;
}

//--------------------------------------------------------------------------------

int	main (int argc, char ** argv)
{
	dirCtx_t			* pRootCtx ;
	int					c ;

	// Parse command line parameters
	while ((c = getopt(argc, argv, "i:o:b:?")) != -1)
	{
		switch (c)
		{
			case 'i':
				src = optarg ;		// Source directory path
				break;

			case 'o':
				dst = optarg ;		// Destination MFS image file
				break;

			case 'b':
				blockSize = strtoul (optarg, NULL, 0) ;
				break;

			case '?':
				usage () ;
				return 0 ;
				break;
		}
	}
	if ((src == NULL)  ||  (dst == NULL)  ||  (blockSize == 0))
	{
		usage () ;
		return 0 ;
	}

	// Check the provided block size
	if ((blockSize & (blockSize - 1)) != 0)
	{
		printf ("Invalid block size: %u\n", blockSize) ;
		return 0 ;
	}
	blockPower2 = ctz (blockSize) ;

	// Open the destination image file
	dstFile = fopen (dst, "wb+") ;
	if (dstFile == NULL)
	{
		printf ("Can't create image file: %s\n", dst) ;
		return 0 ;
	}

	// Allocate a data block
	pDataBlock = malloc (blockSize) ;
	if (pDataBlock == NULL)
	{
		printf ("Malloc error\n") ;
		return 0 ;
	}

	// Write super block: 1st block in the image
	lastBlock = 0 ;
	{
		pSuperBloc = (mfsSuperBloc_t *) malloc (blockSize) ;
		if (pSuperBloc == NULL)
		{
			printf ("Malloc error\n") ;
			return 0 ;
		}
		memset (pSuperBloc, 0, blockSize) ;
		pSuperBloc->version     = MFS_FS_VERSION ;
		pSuperBloc->magic       = MFS_SB_MAGIC ;
		pSuperBloc->blockSize   = blockSize ;
		pSuperBloc->blockPower2 = blockPower2 ;
		strcpy (pSuperBloc->text, "MFS: Minimalist FileSystem") ;
		fwrite (pSuperBloc, blockSize, 1, dstFile) ;
		lastBlock++ ;
	}

	// Initialize the root directory: 2nd block in the image
	pRootCtx = newDirCtx (lastBlock++) ;
	fwrite (pRootCtx->pBlock, blockSize, 1, dstFile) ;

	fileCount = 0;
	dirCount  = 0 ;

	build (pRootCtx, src) ;		// Build the MFS filesystem

	releaseDirCtx (pRootCtx) ;

	// Compute file CRC excluding the 1st block (supe block containing the CRC)
	fseek  (dstFile, blockSize, SEEK_SET) ;
	pSuperBloc->fsCRC = crc32Init () ;
	while (fread (pDataBlock, blockSize, 1, dstFile) != 0)
	{
		pSuperBloc->fsCRC = crc32 (pSuperBloc->fsCRC, (char *) pDataBlock, blockSize) ;
	}
	pSuperBloc->fsSize = lastBlock * blockSize ;

	fseek  (dstFile, 0, SEEK_SET) ;
	fwrite (pSuperBloc, blockSize, 1, dstFile) ;

	printf ("Directory count: %8u\nFile count:      %8u\n", dirCount, fileCount) ;
	printf ("File size:       %8u\nCrc:           0x%08X\n", pSuperBloc->fsSize, pSuperBloc->fsCRC) ;
	printf ("Block size:      %8u\n", blockSize) ;

	free (pSuperBloc) ;
	free (pDataBlock) ;

	fclose (dstFile) ;

	return 1 ;
}

//--------------------------------------------------------------------------------
