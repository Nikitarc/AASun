/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	AASun.h		Common definitions

	When		Who	What
	10/10/22	ac	Creation

----------------------------------------------------------------------
*/
#if ! defined AASUN_H_
#define AASUN_H_
//--------------------------------------------------------------------------------

#include	"aa.h"
#include	"aakernel.h"
#include	<stdbool.h>
#include	"spid.h"			// ADC timer PID synchronization to main cycle
#include	"cfgParameters.h"
#include	"util.h"

#include	<time.h>			// For struct tm

//--------------------------------------------------------------------------------
// ADC and timer configuration

#define	MAIN_PERIOD_US		20000u				// For 50 Hz
#define	MAIN_SAMPLE_COUNT	200u				// Count of samples in 1 main cycle
#define	MAIN_SAMPLE_PERIOD	(MAIN_PERIOD_US / MAIN_SAMPLE_COUNT)	// Sampling period in us

// Collect data for 1 second before computing energy indexes
#define	COLLECTION_COUNT	((uint32_t)(1000000 / MAIN_PERIOD_US * MAIN_SAMPLE_COUNT))

//--------------------------------------------------------------------------------
//	ADC and timer configuration
//	Defines only the IX for the used current sensors

// The indexes of the values in adcValues[]
#define		IX_V1		0
#define		IX_I1		1
#define		IX_I2		2
#if (I_SENSOR_COUNT >= 3)
#define		IX_I3		3
#endif
#if (I_SENSOR_COUNT == 4)
#define		IX_I4		4
#endif
#define		IX_IPOWER	1		// The current sensor to use to compute the main real power

// Count of used ADC channels: 1 for volt, and 2 to 4 for current
#if (defined IX_I4)
	#define	ADC_CHANCOUNT		5u
#elif (defined IX_I3)
	#define	ADC_CHANCOUNT		4u
#else
	#define	ADC_CHANCOUNT		3u
#endif

// Define the timer to use to trigger the ADC
#define	TIMSYNC				TIM3
#define	TIMSYNC_CHAN		LL_TIM_CHANNEL_CH4
#define	TIMSYNC_CLK_MHZ		16u								// The timer clock in MHz:
															// The prescaler is "timer input clock frequency / TIMSYNC_CLK_MHZ".
															// So if PCLK is 64 MHz and TIMSYNC_CLK_MHZ is 16 then the timer prescaler is 4
#define	TIMSYNC_PER_US		(MAIN_PERIOD_US / MAIN_SAMPLE_COUNT)	// Synchro timer period in us. A sample is acquired every TIMSYNC_PER_US microseconds


// Define the timer to use to trigger the SSRs
#define	TIMSSR				TIM1
#define	TIMSSR_CHAN1		TIM_CCER_CC1E		// Timer channel 1, logical SSR 1, high priority diverter
#define TIMSSR_CHAN2		TIM_CCER_CC4E		// Timer channel 4, logical SSR 2, low  priority diverter
#define	TIMSSR_CLK_HZ		51200u							// The timer clock in Hz. 51200 is 512 clock in 10ms.
															// The prescaler is "timer input clock frequency / TIMSSR_CLK_HZ".
															// So if PCLK is 64 MHz and TIMSSR_MHZ is 51200 then the prescaler is 1250

															// SSR timer period in us. 100us less than 1/2 main period
															// The SSR delay+pulse starts near the middle of the last sample period
															// Must end near the middle of the last sample period (for 50Hz: 9950us)
															// 10000 - 50 - 50 = 9900us, ARR = 9900/19.53 = 507
#define	TIMSSR_PERIOD_US	((MAIN_PERIOD_US / 2u) - MAIN_SAMPLE_PERIOD)
#define	TIMSSR_ARR			((((TIMSSR_PERIOD_US * TIMSSR_CLK_HZ) + 500000u) / 1000000u) - 1u)	// The ARR register value

															// The min pulse is 100 us, so the CCR max is: ARR - 100us
#define	TIMSSR_MAX			(TIMSSR_ARR -(((100u * TIMSSR_CLK_HZ) + 500000u) / 1000000u))		// The max CCR register value

//--------------------------------------------------------------------------------
//	For the pulse counters

typedef struct
{
	uint32_t	pulsepkWh ;			// Pulses per kWh
	uint32_t	pulseEnergyCoef ;	// Coefficient to convert pulses to energy (Wh) PULSE_E_SHIFT left shifted
	uint32_t	pulsePowerCoef ;	// Coefficient to convert pulses to power (W)   PULSE_P_SHIFT left shifted
	uint32_t	pulseMaxCount ;		// Overflow detection value
	uint32_t	pulsePeriodCount ;	// Pulse count for the last period

} pulseCounter_t ;

//--------------------------------------------------------------------------------
// The data computed by the meterTask

typedef struct
{
	int32_t			iSumSqr ;
	int32_t			powerSum ;
	int32_t			iPeakAdcP ;		// Amplitude check for amplifier setting
	int32_t			iPeakAdcM ;

} eIData_t ;


typedef struct
{
	int32_t			voltSumSqr ;
	int32_t			vPeakAdcP ;		// Amplitude check for amplifier setting
	int32_t			vPeakAdcM ;

	eIData_t		iData [I_SENSOR_MAX] ;

	int32_t			powerDiverted ;	// Estimate in W << POWER_DIVERTER_SHIFT

} eData_t ;

// The data computed by the AASun task

typedef struct
{
	int32_t			iRms ;
	int32_t			powerReal ;
	int32_t			powerApp ;
	int32_t			cosPhi ;

} cIData_t ;

typedef struct
{
	int32_t			vRms ;
	cIData_t		iData [I_SENSOR_MAX] ;
	uint32_t		powerDiverted ;

} computedData_t ;

//--------------------------------------------------------------------------------
//	Diverter descriptor: information to manage a diverting channel

typedef struct
{
	int32_t		powerMargin ;		// The minimum power to import
	int32_t		powerDiverterMax ;	// The real power in W << POWER_DIVERTER_SHIFT of the diverting device
	int32_t		powerDiverterOpen ;	// Max diverter power threshold (Open circuit detection)
	int32_t		powerDiverter230 ;	// The power in W of the diverting device at 230 Vrms
	int32_t		powerDiverterRref; 	// The resistor in ohm of the diverting device

	uint32_t	ssrChannel ;		// The diverter timer channel

} powerDiv_t ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Global variables

// To define and instantiate global variables in only one place
// AASUN_MAIN is defined only in AASun.c
#if (defined AASUN_MAIN)
	#define EXTERN
#else
	#define EXTERN extern
#endif

EXTERN	uint32_t		statusWord ;		// Bits for errors and to enable/disable some parts of the application
EXTERN	uint32_t		displayWord ;		// Bits to enable/disable some test and debug display

EXTERN	int32_t			phaseCal ;
EXTERN	int32_t			voltCal ;
EXTERN	int32_t			powerSumHalfCycle ;		// To accumulate the real power of a half main cycle (power to divert)

EXTERN	uint32_t		pulsePowerPeriod ;	// The 'instantaneous' power is calculated every pulsePowerPeriod seconds
EXTERN	pulseCounter_t	pulseCounter [PULSE_COUNTER_MAX] ;

EXTERN	powerDiv_t		powerDiv     [POWER_DIV_MAX] ;
EXTERN	bool			bDiverterSet ;		// The request to enable/disable diverting
EXTERN	uint32_t		diverterChannel ;	// The current state of the diverting channels: DIV_SWITCH_xx
											// The active channel is diverterIndex
EXTERN	uint32_t		diverterIndex ;		// The index of the diverter in use (0 or 1)
EXTERN	uint32_t		diverterSwitch ;	// Request to switch diverting channel
#define	DIV_SWITCH_IDLE		0	// Nothing to do
#define	DIV_SWITCH_CHAN1	1	// Switch to diverting channel 0
#define	DIV_SWITCH_CHAN2	2	// Switch to diverting channel 1
#define	DIV_SWITCH_NONE		3	// No channel has condition to run

EXTERN	int32_t			syncPropFactor ;	// Main period synchronization PI controller parameters
EXTERN	int32_t			syncIntFactor ;		// (Derivative parameter is not used)

EXTERN	int32_t			powerPropFactor ;	// Diverter PI controller parameters
EXTERN	int32_t			powerIntFactor ;	// (Derivative parameter is not used)

EXTERN	eData_t			acquiredData ;		// Temporary buffer for AASun task
EXTERN	computedData_t	computedData ;		// Computed by the AASun task every second

EXTERN	energyCounters_t energyJ ;			// Energy accumulator in Joule
EXTERN	energyCounters_t energyWh ;			// Energy total in Wh
EXTERN	energyCounters_t dayEnergyWh ;		// Energy daily in Wh

EXTERN	int32_t			meterPapp ;			// Apparent power from the meter (Linky)
EXTERN	int32_t			meterBase ;			// Energy total from meter
EXTERN	int32_t			meterVolt ;			// Voltage from meter

EXTERN	localTime_t		localTime ;			// Time management
EXTERN	uint8_t			timeIsUpdated ;		// 0 Idle
											// 1 Time update in progress
											// 2 Time is updated, ready to use (synchronize the power history)

EXTERN	tempSensors_t	* pTempSensors ;

EXTERN	powerH_t		powerHistory [POWER_HISTO_MAX_WHEADER] ;
EXTERN	powerH_t		powerHistoryTemp ;		// To accumulate data for the current history period
EXTERN	uint32_t		powerHistoIx ;

EXTERN	int32_t			aaSunVariable		[AASUNVAR_MAX] ;

EXTERN	uint32_t		wifiSoftwareVersion ;	// Version of the WIFI interface software
EXTERN	uint32_t		wifiModeAP ;			// True if the WIFI interface is in "Access Point" mode

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// in AASun.c
uint32_t	AASunVersion			(void) ;

// In adc.c
void		adcDmaInit				(uint32_t bufferSize, uint16_t * pBuffer) ;
void		adcInit					(void) ;
void		adcStart				(void) ;
void		adcTimerInit			(void) ;
void		adcTimerStart			(void) ;
void		adcStop					(void) ;
void		adcSetTaskId			(aaTaskId_t taskId) ;
void		ssrTimerInit			(void) ;
//void		ssrTimerEnable			(void) ;

void		timerOutputChannelSet	(TIM_TypeDef * pTim, uint32_t channel, uint32_t ocValue) ;

// In diverter.c
extern	const uint16_t	p2Delay [] ;
int32_t		divProcessing			(uint32_t meterStep) ;
void		diverterNext			(void) ;

bool		divRuleCompile			(divRule_t * pRule, char * pText, uint32_t * pError) ;
uint32_t	divRulePrint			(divRule_t * pRule, char * pText, uint32_t size) ;
bool		divRuleCheck			(divRule_t * pRule) ;
void		divRuleEnable			(divRule_t * pRule, bool bOn) ;

void		forceInit				(void) ;
bool		forceRuleCompile		(forceRules_t * pForce, char * pStrStart, char * pStrStop, uint32_t * pError) ;
uint32_t	forceRulePrint			(forceRules_t * pForce, bool bStart, char * pText, uint32_t size) ;
bool		forceRuleCheck			(forceRules_t * pForce, bool bStart) ;
void		forceRuleEnable			(forceRules_t * pForce, bool bEnable) ;
void		forceRuleInvalidate		(forceRules_t * pForce) ;
void		forceRuleInvalidateAll	(void) ;
void		forceRuleRemoveAll		(void) ;
bool		forceRuleIsValid		(forceRules_t * pForce) ;
uint32_t	forceJsonStatus			(char * pStr, uint32_t maxLen) ;
bool		forceSetManualOrder		(uint32_t index) ;

// In linky.c
typedef void (* ticCallback_t) (uint32_t index, const char * pLabel, char * pSavePtr) ;

void		ticSetCallback			(ticCallback_t pCallback) ;
void		ticTimeoutStart			(void) ;
void		ticInit					(void) ;
void		ticNext					(void) ;
void		ticToggleDisplay		(void) ;

#define		MODE_HIS			0
#define		MODE_STD			(MODE_HIS ^ 1u)
extern		uint8_t				ticMode ;			// MODE_HIS or MODE_STD

// In timeUpdate.c
void		timeUpdateRequest		(uint32_t mode) ;	// Selects the available source for date/time update
void		lanTimeRequest			(uint32_t mode) ;
void		linkyTimeRequest		(uint32_t mode) ;

void		timeUpdateTic			(char * pStr)  ;	// Callback from linky
void		timeUpdateWifi			(struct tm * pTm) ;	// callback from WIFI

typedef enum
{
	NEXT_START	= 0,		// Start a date/time update
	NEXT_RETRY,				// Try next date/time source
	NEXT_RESET				// Update done, reset the state machine

} nextComman_t ;

void		timeNext				(nextComman_t command) ;	// The date/time update state machine

// In SerEL.c : serial download of HTTP file system
void		SerEL					(void) ;

// Indexes of required Linky TIC information (order in the pTicLabel array)
#define		TIC_IDX_DATE		0
#define		TIC_IDX_EAST		1
#define		TIC_IDX_SINSTS		2
#define		TIC_IDX_UMOY1		3

#define		TIC_IDX_BASE		4
#define		TIC_IDX_PAPP		5

//--------------------------------------------------------------------------------
//	For debug: Fast setting of test points
//	The I/O are defined in  BSP/bspboard.h

__ALWAYS_STATIC_INLINE	void	tst0_0 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT0_PORT_NUMBER)->BRR = (1u << BSP_OUTPUT0_PIN_NUMBER) ;
}

__ALWAYS_STATIC_INLINE	void	tst0_1 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT0_PORT_NUMBER)->BSRR = (1u << BSP_OUTPUT0_PIN_NUMBER) ;
}

__ALWAYS_STATIC_INLINE	void	tst1_0 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT1_PORT_NUMBER)->BRR = (1u << BSP_OUTPUT1_PIN_NUMBER) ;
}

__ALWAYS_STATIC_INLINE	void	tst1_1 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT1_PORT_NUMBER)->BSRR = (1u << BSP_OUTPUT1_PIN_NUMBER) ;
}

__ALWAYS_STATIC_INLINE	void	tst2_0 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT2_PORT_NUMBER)->BRR = (1u << BSP_OUTPUT2_PIN_NUMBER) ;
}

__ALWAYS_STATIC_INLINE	void	tst2_1 (void)
{
	BSP_IO_GPIO(BSP_OUTPUT2_PORT_NUMBER)->BSRR = (1u << BSP_OUTPUT2_PIN_NUMBER) ;
}

//--------------------------------------------------------------------------------
// To manage the status and display words

// statusWord bits values

// Displayed on line A
#define	STSW_DIV_ENABLED		0x00000001			// The state of the diverter (on/off)
#define	STSW_DIVERTING			0x00000002			// Some energy is diverted
#define	STSW_LINKY_ON			0x00000004			// The Linky TIC is managed
#define	STSW_TIME_OK			0x00000008			// Time is initialized
#define	STSW_PWR_HISTO_ON		0x00000010			// Power history on
#define	STSW_W5500_EN			0x00000020			// W5500 LAN chip present
#define	STSW_WIFI_EN			0x00000040			// WIFI connected
#define	STSW_RESERVED			0x00000080			//

// Displayed on line B
#define	STSW_NOT_SYNC			0x00000100			// Not synchronized to main period
#define	STSW_NOT_FLASHCFG		0x00000200			// Invalid configuration in FLASH
#define	STSW_ADC_OVERFLOW		0x00000400			// ADC voltage or current value overflow
#define	STSW_DIVERTING_MAX		0x00000800			// Diverting is at maximum energy
#define	STSW_DIVERTER_OPEN		0x00001000			// The diverter resistor is not connected
#define	STSW_NET_LINK_OFF		0x00002000			// Network not connected

// Displayed on line C


// displayWord bits values For test and debug

#define	DPYW_DISPLAY_DATA1		0x00000001			// To display I1 data every second
#define	DPYW_DISPLAY_DATA2		0x00000002			// To display I2 data every second
#define	DPYW_DISPLAY_DATA3		0x00000004			// To display I3 data every second
#define	DPYW_DISPLAY_DATA4		0x00000008			// To display I4 data every second

#define	DPYW_DISPLAY_DIV_DATA	0x00000010			// To display diverting RT data
#define	DPYW_DISPLAY_SPID		0x00000020			// To display main synchronization PID trace
#define	DPYW_DISPLAY_WH			0x00000040			// To display energies every second
#define	DPYW_DISPLAY_PULSE1		0x00000080			// To display pulse counter 1 every PULSE_P_PERIOD s
#define	DPYW_DISPLAY_PULSE2		0x00000100			// To display pulse counter 2 every PULSE_P_PERIOD s
#define	DPYW_DISPLAY_HISTO		0x00000200			// To display history data every POWER_HISTO_PERIOD s

static inline bool statusWTest (uint32_t statusMask)
{
	return ((statusWord & statusMask) == 0) ? false : true ;
}

static inline void statusWSet (uint32_t statusMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	statusWord |= statusMask ;
	bspRestoreIrq (irqSts) ;
}

static inline void statusWClear (uint32_t statusMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	statusWord &= ~statusMask ;
	bspRestoreIrq (irqSts) ;
}

static inline void statusWToggle (uint32_t statusMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	statusWord ^= statusMask ;
	bspRestoreIrq (irqSts) ;
}


static inline bool displayWTest (uint32_t displayMask)
{
	return ((displayWord & displayMask) == 0) ? false : true ;
}

static inline void displayWSet (uint32_t displayMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	displayWord |= displayMask ;
	bspRestoreIrq (irqSts) ;
}

static inline void displayWClear (uint32_t displayMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	displayWord &= ~displayMask ;
	bspRestoreIrq (irqSts) ;
}

static inline void displayWToggle (uint32_t displayMask)
{
	aaIrqStatus_t	irqSts = bspSaveAndDisableIrq () ;
	displayWord ^= displayMask ;
	bspRestoreIrq (irqSts) ;
}

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// AASUN_H_
