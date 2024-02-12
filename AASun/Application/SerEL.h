
#define		BLOCK_SIZE	512				// Size of exchanged data

enum
{
	ackMessId		= 1,
	nackMessId,
	eraseMessId,
	writeMessId,
	readReqMessId,
	readDataMessId,
	closeMessId,

} MessageId ;

typedef struct
{
	uint32_t		messId ;
	uint32_t		length ;

} messHeader_t ;

typedef struct
{
	messHeader_t	header ;
	struct
	{
		uint32_t		address ;
		uint32_t		size ;			// Bytes
	} data ;

} messErase_t ;

typedef struct
{
	messHeader_t	header ;
	struct
	{
		uint32_t		address ;
		uint32_t		size ;			// Bytes
	} data ;

} messReadReq_t ;

typedef struct
{
	messHeader_t	header ;
	uint8_t			data [0] ;

} messReadData_t ;

typedef struct
{
	messHeader_t	header ;
	struct
	{
		uint32_t		address ;
		uint32_t		size ;			// Bytes
		uint8_t			data [0] ;
	} data ;

} messWrite_t ;
