/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfs.c		Minimalistic read only file system

	When		Who	What
	05/22/23	ac	Creation
	04/03/24	ac	Add CRC and size of the file system to the super bloc
					This alows to compare 2 fisystem and detect change

----------------------------------------------------------------------
*/

#include	<stdio.h>
#include	<string.h>
#include	"mfs.h"

//--------------------------------------------------------------------------------
// TODO : put scratchpad and nameScratchpad in mfsContext ? No if lock

static	union scratchpad
{
	uint8_t			buffer [MFS_ENTRY_SIZE_MAX] ;	// Space for a full dir entry
	mfsEntryHdr_t	fileEntry ;
	mfsDirHdr_t		dirBloc ;
	mfsSuperBloc_t	superBloc ;

} scratchpad ;

static	char nameScratchpad [MFS_NAME_MAX] ;

//--------------------------------------------------------------------------------
// Is is used when the user doesn't provide lock/unlock function

static	void	lockStub (void * param)
{
	(void) param ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Search a name in a directory
// Address is the address of a dir block
// On return the scratchpad contain the fileEntry of this name

static	mfsError_t	searchDir (mfsCtx_t * pCtx, mfsDirHdr_t * address, const char * pName)
{
	mfsError_t		err ;
	uint32_t		dirAddress ;	// Directory bloc address on disk
	uint32_t		entryAddress ;	// Entry address on disk
	uint32_t		count ;
	uint32_t		ii ;
	mfsDirHdr_t		* pNext ;

	pNext = address ;
	while (1)
	{
		// Read the next block of the dir
		dirAddress = (uint32_t) (uintptr_t) pNext ;
		err = pCtx->read (pCtx->userData, dirAddress, & scratchpad, sizeof (mfsDirHdr_t)) ;
		if (err != MFS_ENONE)
		{
			return err ;
		}
		pNext = scratchpad.dirBloc.pNext ;	// Pointer to next block of this dir
		count = scratchpad.dirBloc.count ;	// Count of entry in this dir block

		entryAddress = dirAddress + sizeof (mfsDirHdr_t) ;
		for (ii = 0; ii < count; ii++)
		{
			// Read the entry header
			err = pCtx->read (pCtx->userData, entryAddress, & scratchpad, sizeof (mfsEntryHdr_t)) ;
			if (err != MFS_ENONE)
			{
				return err ;
			}
			// Read the entry name after the header in the scratchpad
			err = pCtx->read (pCtx->userData,
							entryAddress + sizeof (mfsEntryHdr_t),
							scratchpad.fileEntry.name,
							scratchpad.fileEntry.entrySize - sizeof (mfsEntryHdr_t)) ;
			if (err != MFS_ENONE)
			{
				return err ;
			}

			// Compare names
			if (strcmp (pName, scratchpad.fileEntry.name) == 0)
			{
				return MFS_ENONE ;	// Name found
			}

			// Not found try next entry
			entryAddress += scratchpad.fileEntry.entrySize ;
		}
		if (pNext == NULL)
		{
			// No next dir block, end of dir reached
			break ;
		}
	}
	return MFS_ENOTFOUND ;
}

//--------------------------------------------------------------------------------
//	Traverse the path for an absolute path (beginning with '/')
//	On return the scratchpad contain the fileEntry of the last name

static	mfsError_t	searchPath (mfsCtx_t * pCtx, mfsDirHdr_t * address, const char * path)
{
	mfsError_t		err ;
	const char		* pName ;
	mfsDirHdr_t		* pDirBlock ;
	size_t			length ;

	if (path [0] == '/'  &&  path [1] == 0)
	{
		// Special case: the path is the root dir
		// There is no entry for the root dir (don't have a parent), so build one
		scratchpad.fileEntry.blockNum  = 1 ;
		scratchpad.fileEntry.flags     = MFS_DIR ;
		scratchpad.fileEntry.fileSize  = -1 ;
		scratchpad.fileEntry.entrySize = sizeof (mfsEntryHdr_t) + 2 ;
		scratchpad.fileEntry.name [0]  = '/' ;
		scratchpad.fileEntry.name [1]  = 0 ;
		return MFS_ENONE ;	// Found
	}

	pName = path ;
	pDirBlock = address ;
	while (1)
	{
		// Extract the name
		if (* pName == '/')
		{
			pName++ ;	// Skip the begining '/'
		}
		length = strcspn (pName, "/") ;
		memcpy (nameScratchpad, pName, length) ;
		nameScratchpad [length] = 0 ;
		pName += length ;

		err = searchDir (pCtx, pDirBlock, (const char *) & nameScratchpad) ;
		if (err < 0)
		{
			return err ;
		}
		if (* pName == 0)
		{
			// End of path
			return MFS_ENONE ;
		}
		if ((scratchpad.fileEntry.flags & MFS_DIR) == 0)
		{
			// A file entry in the middle of the path: This is not a valid path
			return MFS_ENOTFOUND ;
		}

		// Directory enty found, follow the link
		pDirBlock = (mfsDirHdr_t *) (scratchpad.fileEntry.blockNum << pCtx->blockPower2) ;
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

mfsError_t	mfsMount	(mfsCtx_t * pCtx)
{
	mfsError_t	err ;
	mfsSuperBloc_t * pSuper = & scratchpad.superBloc ;

	if (pCtx->lock == NULL)
	{
		pCtx->lock = lockStub ; 
	}
	if (pCtx->unlock == NULL)
	{
		pCtx->unlock = lockStub ; 
	}
	pCtx->lock (pCtx->userData) ;

	// Read the super bloc header at offset 0
	err = pCtx->read (pCtx->userData, 0, pSuper, sizeof (mfsSuperBloc_t)) ;
	if (err == MFS_ENONE)
	{
		if ((pSuper->magic != MFS_SB_MAGIC)  ||  (pSuper->version != MFS_FS_VERSION)  ||
		    (pSuper->blockSize != (1u << pSuper->blockPower2)))
		{
			err = MFS_ECORRUPT ;
		}
		else
		{
			pCtx->blockSize   = pSuper->blockSize ;
			pCtx->blockPower2 = pSuper->blockPower2 ;
			pCtx->fsCRC       = pSuper->fsCRC ;
			pCtx->fsSize      = pSuper->fsSize ;
			err = MFS_ENONE ;
		}
	}
	pCtx->unlock (pCtx->userData) ;
	return err ;
}

//--------------------------------------------------------------------------------

mfsError_t	mfsUmount	(mfsCtx_t * pCtx)
{
	// Free allocated buffers
	(void) pCtx ;

	return MFS_ENONE ;
}

//--------------------------------------------------------------------------------
//	path is an absolute path starting at the root directory '/'

mfsError_t	mfsOpen		(mfsCtx_t * pCtx, mfsFile_t * pFile, const char * path)
{
	mfsError_t		err = MFS_ENONE ;
	mfsDirHdr_t		* pDirBlock ;

	pCtx->lock (pCtx->userData) ;

	pDirBlock = (mfsDirHdr_t *) pCtx->blockSize ;		// Root directory is the 2nd block
	err = searchPath (pCtx, pDirBlock, path) ;
	if (err == MFS_ENONE)
	{
		if ((scratchpad.fileEntry.flags & MFS_FILE) == 0)
		{
			// The last name of the path is not the name of a file
			err = MFS_ENOTFOUND ;
		}
		else
		{
			// The last name is a file => OK
			pFile->pCtx        = pCtx ;
			pFile->flags       = scratchpad.fileEntry.flags ;
			pFile->dataAddress = scratchpad.fileEntry.blockNum << pCtx->blockPower2 ;
			pFile->fileSize    = scratchpad.fileEntry.fileSize ;
			pFile->position    = 0 ;	// Begining of the data
		}
	}

	pCtx->unlock (pCtx->userData) ;
	return err ;
}

//-------------------------------------------------------------------------------

mfsError_t		mfsClose	(mfsFile_t * pFile)
{
	// Free allocated buffers
	(void) pFile ;

	return MFS_ENONE ;
}

//--------------------------------------------------------------------------------

mfsError_t		mfsRead		(mfsFile_t * pFile, void * pBuffer, int32_t size)
{
	mfsError_t		err = MFS_ENONE ;
	int32_t			readSize = size ;

	pFile->pCtx->lock (pFile->pCtx->userData) ;

	if (pFile->position == pFile->fileSize)
	{
		// Already at end of file
		readSize = 0 ;
	}
	else
	{
		readSize = size ;
		if (readSize > (pFile->fileSize - pFile->position))
		{
			readSize = pFile->fileSize - pFile->position ;
		}
		if (readSize > 0)
		{
			err = pFile->pCtx->read (pFile->pCtx->userData, pFile->dataAddress + pFile->position, pBuffer, readSize) ;
			if (err == MFS_ENONE)
			{
				pFile->position += readSize ;
			}
		}
	}

	pFile->pCtx->unlock (pFile->pCtx->userData) ;
	if (err != MFS_ENONE)
	{
		return err ;
	}
	return readSize ;
}

//--------------------------------------------------------------------------------

mfsError_t		mfsSeek		(mfsFile_t * pFile, int32_t offset, mfsWhenceFlag_t whence)
{
	mfsError_t		err = MFS_ENONE ;

	pFile->pCtx->lock (pFile->pCtx->userData) ;

	switch (whence)
	{
		case MFS_SEEK_SET:
			if (offset >= 0  &&  offset <= pFile->fileSize)
			{
				pFile->position = offset ;
			}
			else
			{
				err = MFS_EINVAL ;
			}
			break ;

		case MFS_SEEK_CUR:
			{
				int32_t newPos = pFile->position + offset ;
				if (newPos >= 0  &&  newPos <= pFile->fileSize)
				{
					pFile->position = newPos ;
				}
				else
				{
					err = MFS_EINVAL ;
				}
			}
			break ;

		case MFS_SEEK_END:
			if (offset <= 0  &&  -offset <= pFile->fileSize)
			{
				pFile->position = pFile->fileSize + offset ;
			}
			else
			{
				err = MFS_EINVAL ;
			}
			break ;

		default:
			err = MFS_EINVAL ;
			break ;
	}

	pFile->pCtx->unlock (pFile->pCtx->userData) ;
	if (err != MFS_ENONE)
	{
		return err ;
	}
	return pFile->position ;
}

//--------------------------------------------------------------------------------

mfsError_t		mfsStat		(mfsCtx_t * pCtx, const char * path, mfsStat_t * pStat)
{
	mfsError_t		err = MFS_ENONE ;
	mfsDirHdr_t		* pDirBlock ;

	pCtx->lock (pCtx->userData) ;

	pDirBlock = (mfsDirHdr_t *) pCtx->blockSize ;		// Root directory is the 2nd block
	err = searchPath (pCtx, pDirBlock, path) ;
	if (err == MFS_ENONE)
	{
		pStat->type = scratchpad.fileEntry.flags & (MFS_DIR | MFS_FILE);
		pStat->size = scratchpad.fileEntry.fileSize ;
	}

	pCtx->unlock (pCtx->userData) ;
	return err ;
}

//--------------------------------------------------------------------------------

int32_t		mfsSize		(mfsFile_t * pFile)
{
	return pFile->fileSize ;
}

//--------------------------------------------------------------------------------

void		mfsRewind	(mfsFile_t* pFile)
{
	pFile->position = 0 ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Directory walk API

mfsError_t		mfsDirOpen (mfsCtx_t * pCtx, const char * path, mfsDir_t * pDir)
{
	mfsError_t		err = MFS_ENONE ;
	mfsDirHdr_t		* dirAddress ;

	pCtx->lock (pCtx->userData) ;

	dirAddress = (mfsDirHdr_t *) pCtx->blockSize ;		// Root directory is the 2nd block
	err = searchPath (pCtx, dirAddress, path) ;
	if (err == MFS_ENONE)
	{
		if ((scratchpad.fileEntry.flags & MFS_DIR) == 0)
		{
			err = MFS_EINVAL ;		// Not a directory
		}
		else
		{
			// It is a directory
			pDir->address  = scratchpad.fileEntry.blockNum << pCtx->blockPower2 ;
			pDir->index    = 0 ;
			pDir->offset   = sizeof (mfsDirHdr_t) ;	// Offset of 1st entry
			err = pCtx->read (pCtx->userData, pDir->address, & scratchpad, sizeof (mfsDirHdr_t)) ;
			if (err == MFS_ENONE)
			{
				pDir->pNext    = scratchpad.dirBloc.pNext ;
				pDir->dirCount = scratchpad.dirBloc.count ;
			}
		}
	}

	pCtx->unlock (pCtx->userData) ;
	return MFS_ENONE ;
}

//--------------------------------------------------------------------------------
// If the end of the directory is reached, MFS_ENOTFOUND is returned

mfsError_t		mfsDirRead		(mfsCtx_t * pCtx, mfsDir_t * pDir, mfsDirEntry_t * pEntry)
{
	mfsError_t		err = MFS_ENONE ;
	uint32_t		entryAddress ;

	pCtx->lock (pCtx->userData) ;

	if (pDir->index == pDir->dirCount)
	{
		if (pDir->pNext == NULL)
		{
			// End of directory reached
			err = MFS_ENOTFOUND ;
		}
		else
		{
			// Continue with the next block of the directory
			pDir->address  = (uintptr_t) pDir->pNext ;
			pDir->index    = 0 ;
			pDir->offset   = sizeof (mfsDirHdr_t) ;	// Offset of 1st entry
			err = pCtx->read (pCtx->userData, pDir->address, & scratchpad, sizeof (mfsDirHdr_t)) ;
			if (err == MFS_ENONE)
			{
				pDir->pNext    = scratchpad.dirBloc.pNext ;
				pDir->dirCount = scratchpad.dirBloc.count ;
			}
			if (pDir->dirCount == 0)
			{
				// End of directory reached
				err = MFS_ENOTFOUND ;
			}
		}
	}

	// Read the entry header
	if (err == MFS_ENONE)
	{
		entryAddress = pDir->address + pDir->offset ;
		err = pCtx->read (pCtx->userData, entryAddress, & scratchpad, sizeof (mfsEntryHdr_t)) ;
		if (err == MFS_ENONE)
		{
			pEntry->type = scratchpad.fileEntry.flags ;
			pEntry->size = scratchpad.fileEntry.fileSize ;

			// Read the entry name
			err = pCtx->read (pCtx->userData,
							entryAddress + sizeof (mfsEntryHdr_t),
							pEntry->name,
							scratchpad.fileEntry.entrySize - sizeof (mfsEntryHdr_t)) ;
			if (err == MFS_ENONE)
			{
				pDir->offset += scratchpad.fileEntry.entrySize ;
				pDir->index++ ;
			}
		}
	}

	pCtx->unlock (pCtx->userData) ;
	return err ;
}

//--------------------------------------------------------------------------------
