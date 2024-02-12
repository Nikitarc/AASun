/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	cfgParameters.h		Default values for the configuration parameters
						Most of these parameters can be modified by the user

	When		Who	What
	08/02/23	ac	Creation

----------------------------------------------------------------------
*/
#if ! defined CFGPARAMETERS_H_
#define CFGPARAMETERS_H_
//--------------------------------------------------------------------------------
#include	"wizLan.h"
#include	"temperature.h"

#define	I_SENSOR_MAX			4		// Max count of managed current sensors
#define	I_SENSOR_COUNT			3		// Count of managed current sensors

#if (I_SENSOR_COUNT > I_SENSOR_MAX)
#error I_SENSOR_COUNT: Too many current sensor
#endif

// Voltage phase correction
#define	PHASE_SHIFT				11
#define PHASE_SHIFT_ROUND		((int32_t) (1u << (PHASE_SHIFT - 1u)))
#define	PHASE_CAL_DEFAULT		((int32_t) (2.00 * (1u << PHASE_SHIFT)))	// Default value for phase calibration

// Voltage calibration example: 386 LSB for 233Vrms=329Vpp => 0.854 Vpp per LSB.  0.854*2^11=1748
#define	VOLT_SHIFT				11u
#define VOLT_SHIFT_ROUND		((int32_t) (1u << (VOLTI_SHIFT - 1u)))
#define	VOLT_CAL				1750					// Voltage calibration, VOLT_SHIFT bits left shifted

#define	I_SHIFT					16u
#define	I1_CAL					595						// I1 calibration, I_SHIFT bits left shifted
#define	I2_CAL					579						// I2 calibration, I_SHIFT bits left shifted
#define	I3_CAL					579						// I3 calibration, I_SHIFT bits left shifted
#define	I4_CAL					579						// I4 calibration, I_SHIFT bits left shifted

#define	I1_OFFSET				0						// ADC offset for I1
#define	I2_OFFSET				0						// ADC offset for I2
#define	I3_OFFSET				0						// ADC offset for I3
#define	I4_OFFSET				0						// ADC offset for I4

// With a shift of 14, this lets 17 bits, so 131 kW. This is enough for personal use.
#define	POWER_SHIFT				14u
#define	POWER1_CAL				1023					// powerReal 1 calibration factor, POWER_SHIFT bits left shifted
#define	POWER2_CAL				995						// powerReal 2 calibration factor, POWER_SHIFT bits left shifted
#define	POWER3_CAL				900						// powerReal 3 calibration factor, POWER_SHIFT bits left shifted
#define	POWER4_CAL				900						// powerReal 4 calibration factor, POWER_SHIFT bits left shifted

#define	POWER1_OFFSET			((int32_t) (9   * (1 << POWER_SHIFT)))	// powerReal1 calibration offset, POWER_SHIFT bits left shifted
#define	POWER2_OFFSET			((int32_t) (10  * (1 << POWER_SHIFT)))	// powerReal2 calibration offset, POWER_SHIFT bits left shifted
#define	POWER3_OFFSET			((int32_t) (10  * (1 << POWER_SHIFT)))	// powerReal2 calibration offset, POWER_SHIFT bits left shifted
#define	POWER4_OFFSET			((int32_t) (10  * (1 << POWER_SHIFT)))	// powerReal2 calibration offset, POWER_SHIFT bits left shifted

//--------------------------------------------------------------------------------
//	Pulse counters

#define	PULSE_CAL				2000					// Default pulse count per kWh
#define	PULSE_E_SHIFT			4						// To improve the accuracy of energy calculations
#define	PULSE_P_SHIFT			4						// To improve the accuracy of power  calculations
#define	PULSE_E_MAX				100000000				// Maximum of the pulse counter displayed energy (Wh)
#define	PULSE_P_PERIOD			(60*5)					// Period (seconds) to compute power from pulse counters

//--------------------------------------------------------------------------------
// Main period synchronization PI controller factors (Derivative is not used).

#define	SPID_P_FACTOR			((1000 * SPID_SCALE_FACTOR + 5000) / 10000)
#define	SPID_I_FACTOR			((  40 * SPID_SCALE_FACTOR + 5000) / 10000)

//--------------------------------------------------------------------------------
// Power diverter PI controller factors (Derivative is not used)

#define	PPID_P_FACTOR			2
#define	PPID_I_FACTOR			25

#define	POWER_DIVERTER_SHIFT	7u						// The powerDiverterITerm precision shift
#define	POWER_DIVERTER_FACTOR	(1 << POWER_DIVERTER_SHIFT)

#define	POWER_DIVERTER1_MAX		(2832) 					// The power in W of the diverting device e.g. 3000 for the 3000W of a water heater
#define	POWER_DIVERTER2_MAX		(740) 					// The power in W of the diverting device e.g. 3000 for the 3000W of a water heater
#define	POWER_DIVERTER_OPENTHR	95						// Threshold to detect diverter open circuit
#define	POWER_MARGIN	        (0  << POWER_SHIFT)		// The minimum power to import

#define	POWER_DIVRES_SHIFT		6								// Diverter resistor value precision shift
#define	POWER_DIVRES_SHIFT_V	(1 << POWER_DIVRES_SHIFT)
#define	POWER_DIVRES_SHIFT_M	((1 << POWER_DIVRES_SHIFT) - 1)

//--------------------------------------------------------------------------------
//	Count of pulse counters to manage

#define	PULSE_COUNTER_MAX		2

//--------------------------------------------------------------------------------

#define	CFGPARAM_VERSION	1		// Configuration structure version
#define	ENERGYCNT_VERSION	1		// Energy counters structure version

//--------------------------------------------------------------------------------

#define	AASUNVAR_MAX		4		// Count of maintained variables

#define	ASV_DAYS			0		// Count of running days since power up
#define	ASV_ANTIL			1		// Anti-legionella counter
#define	ASV_USER1			2		// User counter 1
#define	ASV_USER2			3		// User counter 2

// Anti-legionella: Flags for alFlag in configuration
#define	AL_FLAG_EN			0x80	// Anti-legionella enabled
#define	AL_FLAG_INPUT		0x40	// Use Input (else use Temperature)
#define	AL_FLAG_NUMMASK		0x0F	// Mask to get the temperature index or the input number

//--------------------------------------------------------------------------------
// Energy counters, total or daily

typedef struct
{
	int32_t		date ;				// Date for history records
	int32_t		version ;			// Version of the energy structure
	int32_t		energyImported ;
	int32_t		energyExported ;
	int32_t		energyDiverted1 ;
	int32_t		energyDiverted2 ;
	int32_t		energy2 ;
	int32_t		energy3 ;
	int32_t		energy4 ;
	int32_t		energyPulse [PULSE_COUNTER_MAX] ;	// Pulse count, not energy (Wh)

} energyCounters_t ;

#define	energyCountersOffset	(2)		// version and date
#define	energyCountersCount		((sizeof (energyCounters_t) / 4) - energyCountersOffset)

//--------------------------------------------------------------------------------
//	Power daily history

typedef struct
{
	int32_t		imported ;
	int32_t		exported ;
	int32_t		diverted1 ;
	int32_t		diverted2 ;
	int32_t		power2 ;
	int32_t		power3 ;
	int32_t		power4 ;
	int32_t		powerPulse [PULSE_COUNTER_MAX] ;

} powerH_t ;	// powerH_t and powerHHeader_t must have the same size

typedef struct
{
	int32_t		magic ;
	int32_t		date ;
	int32_t		filling [2] ;
	int32_t		power2 ;
	int32_t		power3 ;
	int32_t		power4 ;
	int32_t		powerPulse [PULSE_COUNTER_MAX] ;

} powerHHeader_t ;	// powerH_t and powerHHeader_t must have the same size

// Power history for 24h
#define		POWER_HISTO_PERIOD		(15*60)				// Every 15mn
#define		POWER_HISTO_MAX			(24*4)				// Every 15mn for 24h
#define		POWER_HISTO_MAX_WHEADER	(POWER_HISTO_MAX+1)	// Add 1 item as header
#define		POWER_HISTO_SHIFT		4
#define		POWER_HISTO_MAGIC		0x12345678			// To check header validity
#define		HISTO_MAX				32					// A power of 2. How many slots in flash history.

#define		pHistoHeader			((powerHHeader_t *) powerHistory)

// Shift for power other than diverted
#define 	POWER_HISTO_ROUND		((int32_t) (1 << (POWER_HISTO_SHIFT - 1u)))

// Shift adjust for power other than diverted
#define		POWER_HISTO_SHIFT_ADD	(POWER_SHIFT - POWER_HISTO_SHIFT)
#define 	POWER_HISTO_ROUND_ADD	((int32_t) (1u << (POWER_SHIFT - POWER_HISTO_SHIFT - 1u)))

// Shift adjust for diverted power
#define		POWER_HISTO_DIVSHIFT_ADD	(POWER_DIVERTER_SHIFT - POWER_HISTO_SHIFT)
#define 	POWER_HISTO_DIVROUND_ADD	((int32_t) (1u << (POWER_DIVERTER_SHIFT - POWER_HISTO_SHIFT - 1u)))

//--------------------------------------------------------------------------------
//	Diverting / forcing rules

#define	DIVE_EXPR_MAX		6				// Count of expression in a rule

#define	DIVE_TEMP_MAX		4				// Temperature number 1..DIVE_TEMP_MAX
#define	DIVE_INPUT_MAX		4				// Input number       1..DIVE_INPUT_MAX
#define	DIVE_POWER_MAX		I_SENSOR_COUNT	// Power number       1..I_SENSOR_COUNT

#define	DIVE_TEMP_HYST		2		// Temperature hysteresis

typedef struct
{
	// For rule  check
	int8_t		tempHyst ;		// Current temperature hysteresis

	uint8_t		type ;			// Source type: T I P
	uint8_t		number ;		// Source index: 3 for T4
	uint8_t		comp ;			// comparison operator: < > =
	int32_t		value ;

} divExpr_t ;

// Diverting rule descriptor
typedef struct
{
	uint16_t	flags ;
	uint16_t	count ;		// Count of expressions
	divExpr_t	expr [DIVE_EXPR_MAX] ;

} divRule_t ;

// Forcing rule descriptor
typedef struct
{
	uint8_t		status ;		// Running, ...
	uint8_t		flags ;			// On/Off, Mode, ...
	uint8_t		channel ;		// The output channel on which this rule applies: 0 to 3
	uint8_t		autoState ;		// AUTO mode automaton state
	uint16_t	ratio ;			// Power % for SSR outputs, or %B for burst mode
	uint16_t	delay ;			// Reload value from delay expression DMIN DMAX
	uint16_t	delayCount ;	// For DMIN DMAX delay processing

	uint16_t	autoOn ;		// For AUTO mode
	uint16_t	autoOff ;
	uint16_t	autoKf ;		// Filter time constant
	uint16_t	kfCounter ;		// For AUTO mode time filter

	uint16_t	counter ;		// For AUTO mode and burst ratio timing (these 2 are exclusive)

	divRule_t	startRule ;
	divRule_t	stopRule ;

} forceRules_t ;

#define	FORCE_MAX				8		// Count of forcing entry
#define	POWER_DIV_MAX			2		// There is POWER_DIV_MAX channels for SSR diverting


//--------------------------------------------------------------------------------
// Some resources are exclusive (this is no longer true with a modified PCB)
// TODO: to remove when the expansion PCB will be available

#define	OPT_LINKY			0x01
#define	OPT_TEMPERATURE		0x02
#define	OPT_COUNTER2		0x04

// The allowed configuration for PCB V1
//#define	OPT_SELECTED		(OPT_LINKY)
#define	OPT_SELECTED		(OPT_LINKY | OPT_TEMPERATURE | OPT_COUNTER2)	// Modified PCB V1
//#define	OPT_SELECTED		(OPT_LINKY | OPT_COUNTER2)	// Modified PCB V1

#define	ISOPT_LINKY			((OPT_SELECTED & OPT_LINKY) != 0)
#define	ISOPT_TEMPERATURE	((OPT_SELECTED & OPT_TEMPERATURE) != 0)
#define	ISOPT_COUNTER2		((OPT_SELECTED & OPT_COUNTER2) != 0)

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// The structure to write/read the configuration parameters in FLASH

// This structure must have a size multiple of 4, because the flash is written with 32 bits words.
// This is checked with the following assert:
#define STATIC_ASSERT(X)            STATIC_ASSERT2(X,__LINE__)
#define STATIC_ASSERT2(X,L)         STATIC_ASSERT3(X,L)
#define STATIC_ASSERT3(X,L)         STATIC_ASSERT_MSG(X,at_line_##L)
#define STATIC_ASSERT_MSG(COND,MSG) \
    typedef char static_assertion_##MSG[(!!(COND))*2-1]

typedef struct
{
	int32_t			iCal ;
	int32_t			iAdcOffset ;
	int32_t			powerCal ;
	int32_t			powerOffset ;

} iSensorCfg_t ;

#define		ENAME_MAX	12					// Max size of energy counter name, including 0

typedef struct
{
	uint32_t		version ;				// Version of the parameter structure

	int32_t			phaseCal ;
	int32_t			voltCal ;
	iSensorCfg_t	iSensor [I_SENSOR_MAX] ;

	int32_t			pulseCal [PULSE_COUNTER_MAX] ;	// Pulse counter: count / kWh

	int32_t			powerMargin1 ;			// The minimum power to import (W << POWER_SHIFT)
	int32_t			powerDiverter1_230 ;	// The power in W of the diverting device at 230Vrms

	int32_t			powerMargin2 ;			// The minimum power to import (W << POWER_SHIFT)
	int32_t			powerDiverter2_230 ;	// The power in W of the diverting device at 230Vrms

	int32_t			syncPropFactor ;		// Period synchronizer PI controller
	int32_t			syncIntFactor ;

	int32_t			powerPropFactor ;		// Power diverter PI controller
	int32_t			powerIntFactor ;

	lanCfg_t		lanCfg ;				// The W5500 network interface configuration

	tempSensors_t	tempsensors ;			// Temperature sensors

	char			eName [energyCountersCount][ENAME_MAX] ;

	uint32_t		favoritePage ;			// The page to display at startup

	divRule_t		diverterRule [POWER_DIV_MAX] ;
	forceRules_t	forceRules   [FORCE_MAX] ;

	uint8_t			alFlag ;				// Anti-legionella: 0x80=enabled + temperature rank (0 based) or input index (0 based)
	int8_t			alValue ;				// Anti-legionella temperature threshold or input value

	uint8_t			reserved1 ;				// Provision for future use
	uint8_t			reserved2 ;
	uint32_t		reserved3 [7] ;

	uint32_t		ckSum ;					// The checksum of the structure

} configParameters_t ;

// Make sure the size of the configParameters_t type is a multiple of 4
STATIC_ASSERT_MSG (sizeof (configParameters_t) % 4 == 0, configParameters_t_not_x4) ;

extern	configParameters_t aaSunCfg ;

//--------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void		readCfg						(void) ;
bool		writeCfg					(void) ;
void		defaultCfg					(void) ;
void		divSetPmax					(uint32_t idx, int32_t power, int32_t voltage) ;

bool		readTotalEnergyCounters		(void) ;
void		writeTotalEnergyCounters	(void) ;

bool		histoInit					(void) ;
void		histoStart					(void) ;
void		histoNext					(void) ;
bool		histoRead					(uint32_t rank, energyCounters_t * pCounters, powerH_t * pPower) ;
bool		histoPowerRead				(void * pBuffer, uint32_t rank, uint32_t offset, uint32_t len) ;
void		histoWrite					(energyCounters_t * pCounters, powerH_t * pPower) ;
bool		histoCheckRank				(uint32_t rank, uint32_t * pAddrs) ;
void		histoErase					(void) ;

void		histoFlashErase				(void) ;

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// CFGPARAMETERS_H_
