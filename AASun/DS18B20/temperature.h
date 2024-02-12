 /*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	temperature.h	Interface to DS18B20 temperature sensor

	When		Who	What
	26/06/23	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined TEMPERATURE_H_
#define TEMPERATURE_H_
//-----------------------------------------------------------------------------
#include "onewire.h"

#define		TEMP_SENSOR_MAX		4
#define		TEMP_SENSOR_SHIFT	4	// For rawTemp

#define		DS18X20_INVALID_RAW_TEMP	((2000 / 10) << TEMP_SENSOR_SHIFT)	// DS18X20_INVALID_DECICELSIUS is 2000

typedef struct
{
	uint8_t		rank ;				// Logical rank defined by the user
	uint8_t		present ;			// Physically present: 0 or 1
	uint8_t 	id [OW_ROMCODE_SIZE] ;
	int16_t 	rawTemp ;			// Raw temperature value (signed binary << TEMP_SENSOR_SHIFT)
//	int16_t 	decicelsius ;		// Temperature in 1/10°C
									// On reading error the value is DS18X20_INVALID_DECICELSIUS, 200.0

} ds18b20_t ;

typedef struct
{
	uint8_t		count ;				// Number of sensors found during search
	ds18b20_t	sensors [TEMP_SENSOR_MAX] ;

} tempSensors_t ;

#ifdef __cplusplus
extern "C" {
#endif

extern	tempSensors_t	tempSensors ;

// The request
#define		TSREQUEST_SEARCH	1	// Initial search for sensors
#define		TSREQUEST_CHECK		2	// Check if sensors are physically presents
#define		TSREQUEST_CONV		3	// Read temperature

void		tsInit				(void) ;
void 		tsRequest			(uint32_t request) ;
bool		tsRequestStatus		(uint32_t request) ;
void		tsRequestAck		(uint32_t request) ;
void		tempSensorNext 		(void) ;

bool		tsIndex				(uint32_t rank, uint32_t * pIndex) ;
bool		tsGetTemp			(uint32_t rank, int32_t * pTemp) ;


#ifdef __cplusplus
}
#endif

//-----------------------------------------------------------------------------
#endif	// TEMPERATURE_H_
