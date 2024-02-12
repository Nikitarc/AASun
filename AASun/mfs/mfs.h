/*
----------------------------------------------------------------------
	
	Alain Chebrou

	mfs.k		Minimalistic read only file system

	When		Who	What
	05/22/23	ac	Creation

----------------------------------------------------------------------
*/

#if (! defined MFS_H_)
#define MFS_H_

#include	<stdint.h>

//--------------------------------------------------------------------------------
//	Internal structures
//--------------------------------------------------------------------------------

#define	MFS_FS_VERSION		((1 << 16) | 0)
#define	MFS_SOFT_VERSION	((1 << 16) | 0)
#define	MFS_SB_MAGIC	(('5' << 24) | ('F' << 16) | ('A' << 8) | 'A')

#define	MFS_ENTRY_SIZE_MAX		96		// With 512 B block, allows min 5 files per directory block, max name length 88
#define	MFS_NAME_MAX			(MFS_ENTRY_SIZE_MAX	- sizeof (mfsEntryHdr_t))

//--------------------------------------------------------------------------------
//	Header of an entry in a directory: dir entry or file entry
//	An entry consists of this header followed by the name
//	The length of an entry is variable because the length of the name is variable

typedef struct mfsEntryHdr_s
{
	uint8_t		entrySize ;		// Byte count of this entry, 
	uint8_t		flags ;			// fileType_t
	uint16_t	blockNum ;		// 1st block of the file data or directory
	int32_t		fileSize ;		// File only: Byte count of data
	char		name [0] ;		// Name placeholder

} mfsEntryHdr_t ;

//--------------------------------------------------------------------------------
//	The header of a directory block

typedef struct mfsDirHdr_s
{
	struct mfsDirHdr_s	* pParent ;		// The 1st block of the parent directory
	struct mfsDirHdr_s	* pPrev ;		// The previous directory block for this directory
	struct mfsDirHdr_s	* pNext ;		// The next     directory block for this directory
	uint32_t			count ;			// Count of entry in this directory block

} mfsDirHdr_t ;

//--------------------------------------------------------------------------------
//	The super bloc on the disk,
//  Block 0 is the super block
//	Block 1 is the root directory

typedef struct mfsSuperBloc_s
{
	uint32_t		magic ;			// MFS_VOL_MAGIC
	uint32_t		version ;		// File system version
	uint32_t		blockSize ;		// Logical size of block on disk
	uint32_t		blockPower2 ;	// (1 << blockPower2) is blockSize
	char			text [0] ;

} mfsSuperBloc_t ;

//--------------------------------------------------------------------------------
// API structures
//--------------------------------------------------------------------------------

typedef enum
{
	MFS_ENONE			=  0,	// No error
	MFS_EIO				= -1,	// Low level memory driver error
	MFS_ECORRUPT		= -2,	// File system corrupted
	MFS_ENOTFOUND		= -3,	// File or directory not found
	MFS_EINVAL			= -4,	// Invalid parameter

} mfsError_t ;

typedef enum
{
	MFS_NOTFILE		= 0,	// Not a file, or free
	MFS_DIR			= 1,
	MFS_FILE		= 2

} fileType_t ;

//--------------------------------------------------------------------------------
// This structure is to be filled in by the user before passing it to mfsMount()
// If not used lock and unlock must be NULL

typedef struct mfsCtx_s
{
	// User provided data
	void			* userData ;	// This is provided to the low level memory driver

	// Low level driver read. Returns MFS_ENONE on success, else MFS_EIO
	int (* read)	(void * userData, uint32_t address, void * pBuffer, uint32_t size) ;

	// Lock/unlock: multi task and/or low level driver shared between several devices
	void (* lock)	(void * userData) ;
	void (* unlock)	(void * userData) ;

	// Internal data
	uint32_t		blockSize ;		// Logical size of block on disk
	uint32_t		blockPower2 ;	// (1 << blockPower2) is blockSize

} mfsCtx_t ;

//--------------------------------------------------------------------------------
//	The user must provide this struct for every file to mfsOpen()
//	It is initialized by mfsOpen()

typedef struct mfsFile_s
{
	mfsCtx_t		* pCtx ;
	uint8_t			flags ;			// fileType_t
	uint32_t		dataAddress ;	// 1st block of the file data 
	int32_t			fileSize ;		// File only: Byte count of data
	int32_t			position ;		// Current read position in the file

} mfsFile_t ;

//--------------------------------------------------------------------------------
//	The struct filled by mfsStat()
//	Allows to know if a dir or a file exist, and the size of the file

typedef struct mfsStat_s
{
	fileType_t		type ;
	int32_t			size ;

} mfsStat_t ;

//--------------------------------------------------------------------------------
//	Directory walk API

typedef struct mfsDir_s
{
	struct mfsDirHdr_s	* pNext ;
	uint32_t			address ;	// Address of the directory block
	uint16_t			offset ;	// Current entry offset in the block
	uint8_t				dirCount ;	// Count of entry in the block
	uint8_t				index ;		// Index of the next entry to read

} mfsDir_t ;

typedef struct mfsDirEntry_s
{
	int32_t			size	 ;		// File size, unused for dir
	fileType_t		type ;			// Entry type: file or dir
	uint8_t			name [MFS_NAME_MAX] ;

} mfsDirEntry_t ;

//--------------------------------------------------------------------------------
// Whence value for mfsSeek()

typedef enum mfsWhenceFlag
{
	MFS_SEEK_SET		= 0,		// The file offset is set to offset bytes
	MFS_SEEK_CUR		= 1,		// The file offset is set to its current location plus offset bytes
	MFS_SEEK_END		= 2			// The file offset is set to the size of the file plus offset bytes

} mfsWhenceFlag_t ;

//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

mfsError_t		mfsMount		(mfsCtx_t * pCtx) ;
mfsError_t		mfsUmount		(mfsCtx_t * pCtx) ;
mfsError_t		mfsStat			(mfsCtx_t * pCtx, const char * path, mfsStat_t * pStat) ;

mfsError_t		mfsOpen			(mfsCtx_t * pCtx, mfsFile_t * pFile, const char * path) ;
mfsError_t		mfsClose		(mfsFile_t * pFile) ;

mfsError_t		mfsRead			(mfsFile_t * pFile, void * pBuffer, int32_t size) ;
mfsError_t		mfsSeek			(mfsFile_t * pFile, int32_t offset, mfsWhenceFlag_t whence) ;
int32_t			mfsSize			(mfsFile_t * pFile) ;
void			mfsRewind		(mfsFile_t * pFile) ;

mfsError_t		mfsDirOpen		(mfsCtx_t * pCtx, const char * path, mfsDir_t * pDir) ;
mfsError_t		mfsDirRead		(mfsCtx_t * pCtx, mfsDir_t * pDir, mfsDirEntry_t * pEntry) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// MFS_H_
