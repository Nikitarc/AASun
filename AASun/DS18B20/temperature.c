 /*
----------------------------------------------------------------------

	Alain Chebrou

	temperature.c	Interface to DS18B20 temperature sensor

	When		Who	What
	26/06/23	ac	Creation

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"AASun.h"
#include	"temperature.h"
#include	"ds18x20.h"
#include	"owuart.h"		// For owuartTimeout

//--------------------------------------------------------------------------------

typedef enum
{
	TS_ST_IDLE = 0,
	TS_ST_SEARCH_START,
	TS_ST_SEARCH,
	TS_ST_CONV_START,
	TS_ST_WAITING,
	TS_ST_READ,
	TS_ST_CHECK_START,
	TS_ST_CHECK,

	TS_CMD_IDLE = 0,
	TS_CMD_START,
	TS_CMD_RUNNING,
	TS_CMD_DONE,

} tsState_t;

static	uint8_t		tsState ;
static	uint8_t		tsIx ;
static	uint8_t		tsDiff ;

static	uint8_t		tsConvCmd,   tsConvStatus ;
static	uint8_t		tsCheckCmd,  tsCheckStatus ;
static	uint8_t		tsSearchCmd, tsSearchStatus ;

//--------------------------------------------------------------------------------

void	tsInit (void)
{
	ow_init ("") ;	// Initialize UART

	tsState  = TS_ST_IDLE ;
	tsIx     = 0 ;
	tsDiff   = 0 ;
	tsConvCmd      = TS_CMD_IDLE ;
	tsConvStatus   = TS_CMD_IDLE ;
	tsCheckCmd     = TS_CMD_IDLE ;
	tsCheckStatus  = TS_CMD_IDLE ;
	tsSearchCmd    = TS_CMD_IDLE ;
	tsSearchStatus = TS_CMD_IDLE ;
}

// Memorize a request to execute

void tsRequest (uint32_t request)
{
	if (request == TSREQUEST_CONV)
	{
		if (tsConvStatus != TS_CMD_RUNNING)
		{
			tsConvStatus = TS_CMD_IDLE ;
			tsConvCmd    = TS_CMD_START ;
		}
	}
	else if (request == TSREQUEST_CHECK)
	{
		if (tsCheckStatus != TS_CMD_RUNNING)
		{
			tsCheckStatus = TS_CMD_IDLE ;
			tsCheckCmd    = TS_CMD_START ;
		}
	}
	else if (request == TSREQUEST_SEARCH)
	{
		if (tsSearchStatus != TS_CMD_RUNNING)
		{
			tsSearchStatus = TS_CMD_IDLE ;
			tsSearchCmd    = TS_CMD_START ;
		}
	}
}

// Get request status: Returns 1 if the request is done

bool  tsRequestStatus (uint32_t request)
{
	if (request == TSREQUEST_CONV)
	{
		return (tsConvStatus == TS_CMD_DONE) ;
	}
	else if (request == TSREQUEST_CHECK)
	{
		return (tsCheckStatus == TS_CMD_DONE)  ;
	}
	else if (request == TSREQUEST_SEARCH)
	{
		return (tsSearchStatus == TS_CMD_DONE)  ;
	}
	return 0 ;
}

// Acknowledge request :
// The request status becomes IDLE

void  tsRequestAck (uint32_t request)
{
	if (request == TSREQUEST_CONV)
	{
		tsConvStatus = TS_CMD_IDLE ;
	}
	else if (request == TSREQUEST_CHECK)
	{
		tsCheckStatus = TS_CMD_IDLE ;
	}
	else if (request == TSREQUEST_SEARCH)
	{
		tsSearchStatus = TS_CMD_IDLE ;
	}
}

//--------------------------------------------------------------------------------

void	tempSensorNext (void)
{
	uint32_t	res ;

	switch (tsState)
	{
		case TS_ST_IDLE:
			if (tsCheckCmd == TS_CMD_START)
			{
				tsCheckCmd    = TS_CMD_IDLE ;
				tsCheckStatus = TS_CMD_RUNNING ;
				tsState       = TS_ST_CHECK_START ;
			}
			else if (tsSearchCmd == TS_CMD_START)
			{
				tsSearchCmd    = TS_CMD_IDLE ;
				tsSearchStatus = TS_CMD_RUNNING ;
				tsState        = TS_ST_SEARCH_START ;
			}
			else if (tsConvCmd == TS_CMD_START)
			{
				tsConvCmd    = TS_CMD_IDLE ;
				tsConvStatus = TS_CMD_RUNNING ;
				tsState      = TS_ST_CONV_START ;
			}
			break ;

		case TS_ST_SEARCH_START:
			tsIx     = 0 ;
			pTempSensors->count = 0 ;
			tsDiff   = OW_SEARCH_FIRST ;
			tsState  = TS_ST_SEARCH ;
			// Initialize rank
			for (uint32_t ii = 0 ;  ii < TEMP_SENSOR_MAX ; ii++)
			{
				pTempSensors->sensors [ii].rank = ii ;
			}
			// No break
			// [[fallthrough]]
			// FALLTHRU

		case TS_ST_SEARCH:
			if (tsIx == TEMP_SENSOR_MAX  ||  tsDiff == OW_LAST_DEVICE)
			{
				pTempSensors->count = tsIx ;
				tsSearchStatus = TS_CMD_DONE ;	// End of search
				tsState        = TS_ST_IDLE ;
			}
			else
			{
				tsDiff = ow_rom_search (tsDiff, pTempSensors->sensors [tsIx].id) ;
				if (owuartTimeout == 1)
				{
					// Time out in receiving char: restart
					owuartTimeout = 0 ;
					tsState = TS_ST_SEARCH ;
				}
				else if (tsDiff == OW_ERR_PRESENCE)
				{
					aaPuts ("OW All devices are offline now.\n") ;
					tsSearchStatus = TS_CMD_DONE ;	// End of search
					tsState        = TS_ST_IDLE ;
				}
				else if (tsDiff == OW_ERR_DATA)
				{
					aaPuts ("OW Bus error.\n") ;
					tsSearchStatus = TS_CMD_DONE ;	// End of search
					tsState        = TS_ST_IDLE ;
				}
				else
				{
					uint8_t	* pId = pTempSensors->sensors [tsIx].id ;
					aaPrintf ("Device %03u Type 0x%02x ID %02x%02x%02x%02x%02x%02x CRC 0x%02x\n",
							tsIx, pId[0], pId[6], pId[5], pId[4], pId[3], pId[2], pId[1], pId[7]);

					pTempSensors->sensors [tsIx].rank = tsIx ;
					tsIx ++;
				}
			}
			break ;

		case TS_ST_CONV_START:
			if (pTempSensors->count == 0)
			{
				tsConvStatus = TS_CMD_DONE ;	// End of reading
				tsState      = TS_ST_IDLE ;
			}
			else
			{
				DS18X20_start_meas (DS18X20_POWER_EXTERN, NULL) ;
				tsState  = TS_ST_WAITING ;
			}
			break ;

		case TS_ST_WAITING:
			if (DS18X20_conversion_in_progress() != DS18X20_CONVERTING)
			{
				tsIx     = 0 ;
				tsState  = TS_ST_READ ;
			}
			else
			{
				if (owuartTimeout == 1)
				{
					// Time out in receiving char: restart conversion
					owuartTimeout = 0 ;
					tsState = TS_ST_CONV_START ;
				}
			}
			break ;

		case TS_ST_READ:
			if (tsIx == pTempSensors->count)
			{
				tsConvStatus = TS_CMD_DONE ;	// End of reading
				tsState      = TS_ST_IDLE ;
			}
			else
			{
				if (pTempSensors->sensors [tsIx].present != 0)
				{
					uint8_t sp [DS18X20_SP_SIZE] ;	// Scratch pad

					res = DS18X20_read_scratchpad (pTempSensors->sensors [tsIx].id, sp, DS18X20_SP_SIZE) ;
					if (res == DS18X20_OK)
					{
						pTempSensors->sensors [tsIx].rawTemp = sp[0] | (sp[1] << 8) ;
//						pTempSensors->sensors [tsIx].decicelsius = DS18X20_raw_to_decicelsius(pTempSensors->sensors [tsIx].id[0], sp);
					}
					else
					{
						pTempSensors->sensors [tsIx].rawTemp = DS18X20_INVALID_RAW_TEMP ;
//					pTempSensors->sensors [tsIx].decicelsius = DS18X20_INVALID_DECICELSIUS ;
					}
				}
				tsIx++ ;
			}
			break ;

		case TS_ST_CHECK_START:
			for (int32_t ii = 0 ; ii < TEMP_SENSOR_MAX ; ii++)
			{
				pTempSensors->sensors [ii].present = 0u ;
				pTempSensors->sensors [ii].rawTemp = DS18X20_INVALID_RAW_TEMP ;
			}
			if (pTempSensors->count == 0)
			{
				tsCheckStatus = TS_CMD_DONE ;	// End of check
				tsState       = TS_ST_IDLE ;
				break ;
			}
			tsIx     = 0 ;
			tsState  = TS_ST_CHECK ;
			// No break
			// [[fallthrough]]
			// FALLTHRU

		case TS_ST_CHECK:
			{
				uint8_t sp [DS18X20_SP_SIZE] ;

				res = DS18X20_read_scratchpad (pTempSensors->sensors [tsIx].id, sp, DS18X20_SP_SIZE) ;
				if (owuartTimeout == 1)
				{
					// Time out in receiving char: restart
					owuartTimeout = 0 ;
					tsState = TS_ST_CHECK_START ;
				}
				else
				{
					if (res == DS18X20_OK)
					{
						pTempSensors->sensors [tsIx].present = 1u ;
aaPrintf ("ts %d\n", tsIx) ;
					}
					tsIx++ ;
					if (tsIx == pTempSensors->count)
					{
						tsCheckStatus = TS_CMD_DONE ;	// End of check
						tsState       = TS_ST_IDLE ;
					}
				}
			}
			break ;

		default:
			break ;
	}
}

//--------------------------------------------------------------------------------
//	Return the index in pTempSensors of the sensor with rank == rank

bool	tsIndex (uint32_t rank, uint32_t * pIndex)
{
	for (uint32_t ii = 0 ; ii < TEMP_SENSOR_MAX ; ii++)
	{
		if (pTempSensors->sensors [ii].rank == rank)
		{
			* pIndex = ii ;
			return true  ;
		}
	}
	return false ;	// Not found
}

//--------------------------------------------------------------------------------
//	Get the temperature of sensor with the index
// 	Returns raw temperature value: signed binary << TEMP_SENSOR_SHIFT

bool	tsGetTemp (uint32_t rank, int32_t * pTemp)
{
	ds18b20_t	* pTs ;
	uint32_t	index ;

	if (tsIndex (rank, & index))
	{
		pTs = & pTempSensors->sensors [index] ;
		if (pTs->present  &&  pTs->rawTemp != DS18X20_INVALID_RAW_TEMP)
		{
			* pTemp = pTs->rawTemp ;
			return true ;
		}
	}
	return false ;	// Bad rank or invalid temperature
}


//--------------------------------------------------------------------------------
/*
#include "util.h"

// Initialization at start of the state machine
// Init + Search + Conversion

void owTest1 (void)
{
	// Initialization at start of the state machine
	tsInit () ;

	// Start search
	tsRequest (TSREQUEST_SEARCH) ;

	// Wait end of search
	while (tsRequestStatus (TSREQUEST_SEARCH) == 0)
	{
		tempSensorNext () ;
		aaTaskDelay (10) ;
	}
	tsRequestAck (TSREQUEST_SEARCH) ;
	aaPuts ("Search OK\n") ;


	// Start conversion
	tsRequest (TSREQUEST_CONV) ;

	// Wait end of conversion
	while (tsRequestStatus (TSREQUEST_CONV) == 0)
	{
		tempSensorNext () ;
		aaTaskDelay (100) ;
	}
	tsRequestAck (TSREQUEST_CONV) ;
	aaPuts ("Conv OK\n") ;


	// Display values
	for (uint32_t ix = 0 ; ix < pTempSensors->count ; ix++)
	{
        iFracPrint (pTempSensors->sensors [ix].rawTemp, 4, 6, 1) ;
        aaPutChar ('\n') ;
	}
}

//--------------------------------------------------------------------------------
// Test conversion

void owTest2 (void)
{
	// Start conversion
	tsRequest (TSREQUEST_CONV) ;

	// Wait end of conversion
	while (tsRequestStatus (TSREQUEST_CONV) == 0)
	{
		tempSensorNext () ;
		aaTaskDelay (100) ;
	}
	tsRequestAck (TSREQUEST_CONV) ;
	aaPuts ("Conv OK\n") ;


	// Display values
	for (uint32_t ix = 0 ; ix < pTempSensors->count ; ix++)
	{
//		int16_t	dc = pTempSensors->sensors [ix].decicelsius ;
//		aaPrintf("TEMP %3d.%01d C    ", dc / 10, dc > 0 ? dc % 10 : -dc % 10);
		iFracPrint (pTempSensors->sensors [ix].rawTemp, 4, 6, 1) ;
		aaPutChar ('\n') ;
	}
}

//--------------------------------------------------------------------------------
// Check temperature sensors

void owTest3 (void)
{
	// Start conversion
	tsRequest (TSREQUEST_CHECK) ;

	// Wait end of check
	while (tsRequestStatus (TSREQUEST_CHECK) == 0)
	{
		tempSensorNext () ;
		aaTaskDelay (10) ;
	}
	tsRequestAck (TSREQUEST_CHECK) ;
	aaPuts ("Check OK\n") ;

	// Display values
	for (uint32_t ix = 0 ; ix < pTempSensors->count ; ix++)
	{
        iFracPrint (pTempSensors->sensors [ix].rawTemp, 4, 6, 1) ;
        aaPutChar ('\n') ;
	}
}

//--------------------------------------------------------------------------------
*/
