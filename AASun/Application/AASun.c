/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	AASun.c		Energy metering/diverting

	When		Who	What
	10/10/22	ac	Creation
	18/02/23	ac	port to PCB V1
	04/09/23	ac	V1.2	Use modified PCB V1: Linky on LPUART1 and pin PB10 =>
	 	 	 	 			allows to use Linky AND pulse counter 2 AND temperature sensors
	 	 	 	 			Input of J7 should not be used (now it's Linky RX)

	23/09/23 	ac	V1.3	Add command to clear energy counters
	23/09/23	ac	V1.4	Add pull up on UART TX pin (helps not to block when there is no temperature sensor interface)
	23/10/06	ac	V1.5	Replace uint32_t by int32_t in powerH_t typedef (so powers are signed)
	23/10/09	ac	V1.6	Manage I3 and I4
	23/10/19	ac	V1.7	Add SeEL: Serial flash external loader
	23/10/22	ac	V1.8	Add TX DMA for W5500
							Unsigned for all energies (PV can sink current at night!)
							cpmax with 2 parameters: watt and optional V
							Add configurable names to energies (for graph display)
							Add date/time update after general power outage (add timeout because the Internet box isn't alive)
	23/10/27	AC	V1.9	Add 2nd diverter
							Split AASun.c and create diverter.c
							Bug in Linky. Now doesn't use TX PB11 - can add DS18B20
							Add digital input/output processing for diverting rules
							PA11 output dedicated to on board relay. SSR2 moved to PB3 (pin 13 of J14)
	23/11/30	ac	V1.10	Add forcing processing
							Add forcing options: Power ratio for SSR output, long ratio (multi minute period)
	23/12/01	ac	V1.11	New AUTO forcing mode
							Add telnetFlush() in wizLan.c (avoid Telnet TX buffer overflow)
	23/12/05	ac	V1.12	Add aaSunVariable[]
							Add Anti-legionella
							Add Week Day in forcing expression
	24/01/16	ac	V1.13	Error computing exported energy
	24/01/29	ac	V1.14	Adjust I/O for expansion PCB
	24/02/07	ac	V1.15	Attempt to strengthen one wire communication for buggy sensors (timeout)
							Rework of send_http_response_body()
	24/02/14	ac	V1.16	Add # operator for Variable comparison in forcing rule


----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aaprintf.h"
#include	"aautils.h"

#include	<stdlib.h>		// For strtoul()
#include	<string.h>		// For memset, memcpy...

#define		AASUN_MAIN		// To instantiate all EXTERN here
#include	"AASun.h"

#include	"temperature.h"
#include	"sh1106.h"		// OLED display
#include	"w25q.h"		// Flash

#define		AASUN_VERSION		((1u << 16) | 16u)

// For debug: displays tasks stack usage
static aaTaskInfo_t	taskInfo [AA_TASK_MAX] ;
extern void	displaytaskInfo (aaTaskInfo_t * pTaskInfo) ;

static void displayEnergy (char * text, energyCounters_t * pE) ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Energy meter configuration

#define	LOCK_THR				20						// The PID is locked when the value is in the range: -LOCK_THR / +LOCK_THR
#define	LOCK_COUNT				20u						// The PID is locked when the value is in the lock range for LOCK_COUNT main cycle

static	uint16_t				adcValues [ADC_CHANCOUNT] ;
static	uint8_t					meterStep ;				// From 0 to MAIN_SAMPLE_COUNT-1
static	volatile uint8_t		meterCmd ;
static	uint32_t				lockCnt ;

static	uint32_t				collectionCount ;		// To accumulate data for 1 second
static	eData_t					eData ;					// Buffer to collect data
static	eData_t					collectionData ;		// Buffer to transmit collected data to the main loop
static	uint32_t				collectionDataOk ;		// Collected data in collectionData are ready to be processed

static	int32_t					phasePrev ;				// For voltage phase correction

// This table allows you to obtain the index of the ADC value of a current sensor in the adcValues array using its rank
// e.g. the index for I1 is: adcIndex[0]. Current sensors are numbered from 0 to 3.
static const uint8_t adcIndex [ADC_CHANCOUNT] =
{
		IX_I1,
		IX_I2,
#if (defined IX_I3)
		IX_I3,
#endif
#if (defined IX_I4)
		IX_I4,
#endif
};

//--------------------------------------------------------------------------------
// To compute ADC offsets. See https://learn.openenergymonitor.org/electricity-monitoring/ctac/digital-filters-for-offset-removal
// The filter length is 2^12=4096 samples. This is about 20 main cycle or 410 ms
// The offset ripple is proportional to the sample amplitude,
// So to reduce the ripple to the level of 1 ADC LSB the samples values are divided by 4 before being applied to the filter.
static	int32_t					adcOffset ;				// Offset of volt values
static	int32_t					adcOffsetFilter ;		// Internal to filter
#define	ADCOFFSET_DIV			4
#define	ADCOFFSET_SHIFT			12						// Time constant of 4096 samples (to get 63% of a step response)
#define	ADCOFFSET_SHIFT_ROUND	((int32_t) (1u << (ADCOFFSET_SHIFT - 1u)))

static inline void offsetFilter (void)
{
	adcOffsetFilter += ((int32_t) adcValues [IX_V1] - adcOffset) / ADCOFFSET_DIV ;
	adcOffset = (adcOffsetFilter + ADCOFFSET_SHIFT_ROUND) >> ADCOFFSET_SHIFT ;
}

//--------------------------------------------------------------------------------
// An array for 1 main cycle of samples (debug)

static	uint16_t				iSamples ;
static	uint16_t				samples [(ADC_CHANCOUNT + 1) * MAIN_SAMPLE_COUNT] ;	// +1 for phase corrected voltage
static	uint16_t				* pSamples ;

typedef enum
{
	samplesStateIdle	= 0,
	samplesStateStart,
	samplesStateAcq,
	samplesStateDone

} samplesState_e ;

static	volatile samplesState_e	samplesState ;

//--------------------------------------------------------------------------------
//	Return AASun version, with 2 numbers: Version - Release, eg 0x00010002 for 1.2

uint32_t	AASunVersion	(void)
{
	return AASUN_VERSION ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	The core AASUn task: the meter task

// Define the AASUn meter task stack, to avoid use of dynamic memory
#define	METER_STACK_SIZE	256u
static	bspStackType_t		meterStack [METER_STACK_SIZE] BSP_ATTR_ALIGN(8) BSP_ATTR_NOINIT ;

static	bool	synchroInit (void)
{
	uint32_t	ii ;
	int32_t		voltAdcPrev ;
	int32_t		voltAdc ;

	// Compute adcOffset start value: accumulation of samples during one main cycle
	voltAdc = 0 ;
	for (ii = 0 ; ii < MAIN_SAMPLE_COUNT ; ii++)
	{
		// Wait for the end of DMA signal
		if (AA_ENONE != aaSignalWait (1u, NULL, AA_SIGNAL_AND, 10u))
		{
			// Timeout.
			if (meterCmd != 0)
			{
				// Halt requested
				meterCmd = 0 ;
				return false ;
			}
			continue ;
		}
		voltAdc += (int32_t) adcValues [IX_V1] ;
		bspToggleOutput (BSP_LED0) ;		// Toggle every 100�s!
	}
	// Initialize the ADC offset filter with a good approximate value
	adcOffset = voltAdc / MAIN_SAMPLE_COUNT ;
	adcOffsetFilter = adcOffset << ADCOFFSET_SHIFT ;
aaPrintf ("analog offset %d\n", adcOffset) ;

	// Synchronize the ADC acquisition to the up zero crossing voltage

	// 1- Check until we are in the rising slope of the sinus
	voltAdcPrev = (int32_t) adcValues [IX_V1] - adcOffset ;
	while (1)
	{
		// Wait for the end of DMA signal
		if (AA_ENONE != aaSignalWait (1u, NULL, AA_SIGNAL_AND, 10u))
		{
			// Timeout.
			if (meterCmd != 0)
			{
				// Halt requested
				meterCmd = 0 ;
				return false ;
			}
			continue ;
		}

		// Offset computing
		offsetFilter () ;

		voltAdc = (int32_t) adcValues [IX_V1] - adcOffset ;
		int32_t diff = voltAdc - voltAdcPrev ;
		if (voltAdc < LOCK_THR  &&  voltAdc > -LOCK_THR  &&  diff > 8)
		{
			// voltAdc is near mid range and diff is > 0, so we are in the rising slope of the sinus
			meterStep = 0 ;		// 1st adc period of main cycle
			break ;
		}
		voltAdcPrev = voltAdc ;
		bspToggleOutput (BSP_LED0) ;
	}

	// 2- Initialize the PID
	sPidInit    ((int32_t) (TIMSYNC_PER_US * TIMSYNC_CLK_MHZ)) ;
	sPidFactors (syncPropFactor, syncIntFactor) ;

	// 3- Lock (or synchronize) the ADC acquisition timer with the up zero crossing of the voltage
	//    When the PLL is locked, return to normal mode
	lockCnt = LOCK_COUNT ;
	while (1)
	{
		// Wait for the end of DMA signal
		if (AA_ENONE != aaSignalWait (1u, NULL, AA_SIGNAL_AND, 10u))
		{
			// Timeout.
			if (meterCmd != 0)
			{
				// Halt requested
				meterCmd = 0 ;
				return false ;
			}
			continue ;
		}

		// Offset computing
		offsetFilter () ;
		voltAdc = (int32_t) adcValues [IX_V1] - adcOffset ;

		if (meterStep == 0u)
		{
			// Processing of the 1st step of the main cycle
tst1_1 () ;	// Rising of pulse on main 0 crossing

			// Adjust the ADC timer period: the voltage at the beginning of the 1st step is aimed at 0
			// Negate the error to sync on rising edge:
			// if the error is > 0 then we need to shorten the timer period
if (displayWTest (DPYW_DISPLAY_SPID)) aaPrintf ("%4d ", -voltAdc) ;
			uint32_t timArr = sPid (-voltAdc) ;
			TIMSYNC->ARR = timArr ;

			// Check if the PID is locked: voltage stable and near 0
			// The PID is locked if lockCnt is 0
			if (voltAdc < LOCK_THR  &&  voltAdc > -LOCK_THR)
			{
				if (lockCnt != 0)
				{
					lockCnt--;
				}
			}
			else
			{
				lockCnt = LOCK_COUNT ;	// voltAdc out of range: Restart the delay
			}
		}
		else
		{
			// Processing of other steps of the main cycle
tst1_0 () ;		// Falling of pulse on main  0 crossing

			// 4- If locked the synchronization is ended
			if (lockCnt == 0  && (meterStep == (MAIN_SAMPLE_COUNT - 1u)))
			{
				// Locked and in the last step before the end of the main cycle
				// Initialize for normal mode
				memset (& eData, 0, sizeof (eData)) ;	// Initialize the computed data
				phasePrev          = voltAdc ;
				powerSumHalfCycle  = 0 ;
				collectionCount    = COLLECTION_COUNT ;
				collectionDataOk   = 0 ;
				meterStep          = 0 ;				// The next step is the 1st of the period
				break ;									// Synchronization is complete
			}
		}

		// Next step
		meterStep ++ ;
		if (meterStep == MAIN_SAMPLE_COUNT)
		{
			meterStep = 0 ;
		}
		bspToggleOutput (BSP_LED0) ;
	}
aaPuts ("Running\n") ;
	statusWClear (STSW_NOT_SYNC) ;
	statusWSet   (STSW_NORMAL) ;

	bspOutput (BSP_LED0, BSP_LED0_OFF) ;		// Blue LED off
	return true ;
}

//--------------------------------------------------------------------------------
// This task processes the ADC samples

void	meterTask (uintptr_t arg)
{
	int32_t			voltAdcRaw ;	// The voltage value from the ADC
	int32_t			voltAdc ;		// The voltage value phase shifted
	int32_t			currentAdc ;
	uint32_t		ii ;

	(void) arg ;

	// Task created
	meterCmd = 0 ;

	// Prepare to receive a signal from the ADC DMA
	adcSetTaskId  (aaTaskSelfId ()) ;
	aaSignalClear (AA_SELFTASKID, 1) ;

	if (! synchroInit ())
	{
		// Halt requested
		AA_ASSERT (0) ;
		return ;
	}

	while (1)
	{
		// Wait for the signal from end of ADC DMA or a command
		if (AA_ENONE != aaSignalWait (1u, NULL, AA_SIGNAL_AND, 2u))
		{
			// Timeout.
			if (meterCmd != 0)
			{
				// Halt requested
				timerOutputChannelSet (TIMSSR, powerDiv[0].ssrChannel, 511) ;	// Stop diverter
				timerOutputChannelSet (TIMSSR, powerDiv[1].ssrChannel, 511) ;
				meterCmd = 0 ;	// This is an acknowledge of the request
				return ;		// This deletes the meterTask
			}
			continue ;
		}

		// For test: generate a pulse at the 0 crossing
		if (meterStep == 0)
		{
			tst1_1 () ;		// Start of pulse on main up 0 crossing
		}
		else
		{
			tst1_0 () ;		// End of pulse on main up 0 crossing
		}

		// ------------------------------------------------------
		// SSR timer start, to do very early to avoid jitter

		if (meterStep == 0  ||  meterStep == (MAIN_SAMPLE_COUNT/2u))
		{
			// 1st step of either half main cycle: start the SSR timer
			TIMSSR->CR1 |= TIM_CR1_CEN ;
		}

		// ------------------------------------------------------
		// To do at every sample

		// ADC offset computing
		offsetFilter () ;

		voltAdcRaw = (int32_t) adcValues [IX_V1] - adcOffset ;
		if (voltAdcRaw > eData.vPeakAdcP)
		{
			eData.vPeakAdcP = voltAdcRaw ;		// For calibration helper
		}
		if (voltAdcRaw < eData.vPeakAdcM)
		{
			eData.vPeakAdcM = voltAdcRaw ;		// For calibration helper
		}

		// Phase correction for voltage
		voltAdc = voltAdcRaw + ((phaseCal * (voltAdcRaw - phasePrev) + PHASE_SHIFT_ROUND) >> PHASE_SHIFT) ;
		phasePrev = voltAdcRaw ;

		eData.voltSumSqr += voltAdc * voltAdc ;

		// Current sensors
		for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
		{
			adcValues [adcIndex[ii]] += aaSunCfg.iSensor [ii].iAdcOffset ;	// Hardware offset correction

			currentAdc = (int32_t) adcValues [adcIndex[ii]] - adcOffset ;
			eData.iData[ii].iSumSqr  += currentAdc * currentAdc ;
			eData.iData[ii].powerSum += voltAdc * currentAdc ;
			if (currentAdc > eData.iData[ii].iPeakAdcP)
			{
				eData.iData[ii].iPeakAdcP = currentAdc ;		// For calibration helper
			}
			if (currentAdc < eData.iData[ii].iPeakAdcM)
			{
				eData.iData[ii].iPeakAdcM = currentAdc ;		// For calibration helper
			}
		}

		// ------------------------------------------------------
		// For debug and calibration
		// Memorize raw samples acquisition if requested
		// Start at the beginning of the main cycle
		if (samplesState == samplesStateStart  &&  meterStep == 0)
		{
			iSamples = 0 ;
			pSamples = samples ;
			samplesState = samplesStateAcq ;
		}
		if (samplesState == samplesStateAcq)
		{
			* pSamples++ = voltAdcRaw ;	// Raw ADC voltage
			for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
			{
				* pSamples++ = (int32_t) adcValues [adcIndex [ii]] - adcOffset ;
			}
			* pSamples++ = voltAdc ;	// Phase corrected voltage
			iSamples ++ ;
			if (iSamples == MAIN_SAMPLE_COUNT)
			{
				samplesState = samplesStateDone ;
			}
		}

		// ------------------------------------------------------
		// Is there enough accumulated data (1 second) ?

		collectionCount -- ;
		if (collectionCount == 0)
		{

			bspOutput (BSP_LED0, BSP_LED0_ON) ;		// Blue LED on

			if (collectionDataOk == 0)
			{
				// collectionData is free
				collectionData   = eData ;
				collectionDataOk = 1 ;
			}
			else
			{
				// collectionData is not free: delete data
			}
			memset (& eData, 0, sizeof (eData)) ;
			collectionCount = COLLECTION_COUNT ;
		}

		// ------------------------------------------------------
		// For power diverting: accumulate real power for 1/2 main period
		powerSumHalfCycle += voltAdc * ((int32_t) adcValues [IX_IPOWER] - adcOffset) ;

		// ------------------------------------------------------
		// PLL adjustment: synchronize the ADC timer to the main cycle

		if (meterStep == 0)
		{
			uint32_t	timArr ;

			timArr = sPid (-voltAdcRaw) ;
			TIMSYNC->ARR = timArr ;

			// Check PID lock
			if (voltAdcRaw < LOCK_THR  &&  voltAdcRaw > -LOCK_THR)
			{
				// voltAdc is in the lock range
				if (lockCnt != 0)
				{
					lockCnt--;
					if (lockCnt == 0)
					{
						// Synchronized and locked
aaPuts ("LK\n") ;	// TODO remove
 					}
				}
			}
			else
			{
				// voltAdc is not in the lock range
				statusWSet   (STSW_NOT_SYNC) ;
				if (lockCnt == 0)
				{
					// Display only when switching from lock to unlock
aaPrintf ("UL %d\n", voltAdcRaw) ;	// TODO remove
				}
				lockCnt = LOCK_COUNT ;	// Restart the lock delay
			}
		}

		// ------------------------------------------------------
		// Computing SSR delay

		if (meterStep == (MAIN_SAMPLE_COUNT - 2u)  ||  meterStep == (MAIN_SAMPLE_COUNT/2u - 2u))
		{
			// 2 steps before the end of the 1/2 period: Force timer stop and clear the counter/prescaler
			// If the pulse stops too late, from time to time the SSR remains on for the next half period
			TIMSSR->CR1 &= ~TIM_CR1_CEN ;
			TIMSSR->EGR |= TIM_EGR_UG ;
		}

		if (meterStep == (MAIN_SAMPLE_COUNT - 1u)  ||  meterStep == (MAIN_SAMPLE_COUNT/2u - 1u))
		{
			// In last step of either half main cycle
			// Set the SSR timer compare register for the next half period
			eData.powerDiverted += divProcessing (meterStep) ;		// In W << POWER_DIVERTER_SHIFT
		}

		// ------------------------------------------------------
		// Processing in miscellaneous steps

		pulsePoll () ;

		switch (meterStep)
		{
			case 20:
				buttonPoll () ;		// Every 20 ms
				break ;

			case 50:
			case MAIN_SAMPLE_COUNT/2u + 50:
				pulsePoll () ;		// Every 10 ms
				break ;

			default:
				break ;
		}

		// Next step
		meterStep ++ ;
		if (meterStep == MAIN_SAMPLE_COUNT)
		{
			meterStep = 0 ;
		}
tst0_0 () ;		// Falling: of sample Processing
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Start the diverter when the application is started
//	Configures the ADC and create the meter task

void diverterStart (void)
{
	statusWClear (STSW_NORMAL) ;
	statusWSet   (STSW_NOT_SYNC) ;

	// Configure ADC acquisition
	adcDmaInit (ADC_CHANCOUNT, adcValues) ;
	adcInit () ;
	adcTimerInit () ;
	ssrTimerInit () ;
	adcStart () ;

	diverterIndex   = 0 ;				// Default diverter in use
	diverterSwitch  = DIV_SWITCH_IDLE ;	// No pending switch request
	diverterChannel = DIV_SWITCH_NONE ;	// No active diverting channel

	// Create the AASun meter task with the highest task priority
	meterCmd = 1 ;		// This will be set to 0 by the created task
	aaTaskCreate (
			AA_PRIO_MAX,		// Task priority
			"tMeter",			// Task name
			meterTask,			// Entry point,
			0,					// Entry point parameter
			meterStack,			// Stack pointer
			METER_STACK_SIZE,	// Stack size
			AA_FLAG_STACKCHECK,	// Flags
			NULL) ;				// Created task id
	while (meterCmd == 1u)
	{
		aaTaskDelay (2) ;	// Let some time to create the task
	}

	// Start ADC acquisition
	adcTimerStart ()  ;
}

//--------------------------------------------------------------------------------
//	Stop the diverter when the application is ended
//	Disables the ADC and instructs the meter task to destroy itself

void diverterStop (void)
{
	statusWClear (STSW_NORMAL) ;
 	statusWSet   (STSW_STOPPED) ;

 	adcStop () ;			// Stop the ADC acquisition: no more DMA interrupt
	meterCmd = 1u ;			// Request to stop the AASun meter task
	while (meterCmd == 1u)
	{
		aaTaskDelay (2) ;	// Wait until the task is deleted
	}
}

//--------------------------------------------------------------------------------
// Called when the Linky send an information we want
// pSavePtr is intended for use by strtok_r() to retrieve values

void myTicCallback (uint32_t index, const char * pLabel, char * pSavePtr)
{
	char	* pStr ;

	(void) pLabel ;

	switch (index)
	{
		// Values from standard mode (separator 0x09)
		// -----------------------------------------
		case  TIC_IDX_DATE:
			{
				pStr = strtok_r (NULL, "\t", & pSavePtr) ;

				// Initialize the date/time if it is requested
				if (bLinkyDateUpdate)
				{
					localTime.hh = ((pStr [7]  & 0x0F) * 10) + (pStr [8]  & 0x0F) ;
					localTime.mm = ((pStr [9]  & 0x0F) * 10) + (pStr [10] & 0x0F) ;
					localTime.ss = ((pStr [11] & 0x0F) * 10) + (pStr [12] & 0x0F) ;

					if (pStr [0] == 'E')
					{
						// On summer time : adjust the hour to UTC+1 by subtracting 1 to the hour
						localTime.hh -- ;
						if (localTime.hh == -1)
						{
							localTime.hh = 23 ;
						}
					}
// TODO Is this OK ?
					// Avoid day+1 from 23h to 0h on summer time
					if (statusWTest (STSW_TIME_OK) == false  ||  localTime.hh != 23)
					{
						localTime.yy = ((pStr [1] & 0x0F) * 10) + (pStr [2] & 0x0F) + 2000 ;
						localTime.mo = ((pStr [3] & 0x0F) * 10) + (pStr [4] & 0x0F) ;
						localTime.dd = ((pStr [5] & 0x0F) * 10) + (pStr [6] & 0x0F) ;
					}
					localTime.wd = sntpSecondToWd (sntpDateToSecond (& localTime)) ;
					bLinkyDateUpdate = false ;
					statusWSet (STSW_TIME_OK) ;

					// If required, signal to AASun task to start/synchronize power history
					if (timeIsUpdated == 1)
					{
						timeIsUpdated = 2 ;
					}
				}
			}
			break ;

		case  TIC_IDX_EAST:
			pStr = strtok_r (NULL, "\t", & pSavePtr) ;
			meterBase = strtol (pStr, NULL, 10) ;		// Imported energy total (V)
			break ;

		case  TIC_IDX_SINSTS:
			pStr = strtok_r (NULL, "\t", & pSavePtr) ;
			meterPapp = strtol (pStr, NULL, 10) ;		// Apparent power (VA)
			break ;

		case  TIC_IDX_UMOY1:
			pStr = strtok_r (NULL, "\t", & pSavePtr) ;	// Date
			pStr = strtok_r (NULL, "\t", & pSavePtr) ;	// Voltage
			meterVolt = strtol (pStr, NULL, 10) ;		// Voltage (V)
			break ;


		// Values from historic mode (separator 0x20)
		// ------------------------------------------
		case  TIC_IDX_BASE:
			pStr = strtok_r (NULL, " \t", & pSavePtr) ;
			if (pStr != NULL)
			{
				meterBase = strtol (pStr, NULL, 10) ;
			}
			break ;

		case TIC_IDX_PAPP:
			pStr = strtok_r (NULL, " \t", & pSavePtr) ;
			if (pStr != NULL)
			{
				meterPapp = strtol (pStr, NULL, 10) ;
			}
			break ;

		default:
			// Ignore
			break ;
	}
}
//--------------------------------------------------------------------------------
//	SNTP callback

// result: 0 success, -1 timeout

void sntpCb (int32_t result, datetime * time)
{
	if (result == 0u)
	{
		// Success
aaPrintf("CurT %02d:%02d:%02d  %04d/%02d/%02d\n", localTime.hh, localTime.mm, localTime.ss, localTime.yy, localTime.mo, localTime.dd) ;	// TODO remove
aaPrintf("SNTP %02d:%02d:%02d  %04d/%02d/%02d\n", time->hh, time->mm, time->ss, time->yy, time->mo, time->dd) ;	// TODO remove

		bspDisableIrq () ;			// Called from low priority task: need exclusive use
		localTime.hh = time->hh ;
		localTime.mm = time->mm ;
		localTime.ss = time->ss ;
		localTime.yy = time->yy ;
		localTime.mo = time->mo ;
		localTime.dd = time->dd ;
		localTime.wd = time->wd ;
		statusWSet (STSW_TIME_OK) ;
		bspEnableIrq () ;

		// If required, signal to AASun task to start/synchronize power history
		if (timeIsUpdated == 1)
		{
			timeIsUpdated = 2 ;
		}
	}
	else
	{
		aaPuts ("SNTP timeout\n") ;
		timeIsUpdated = 0 ;			// Time update failed, free to retry
	}
}

//--------------------------------------------------------------------------------
//	DNS callback
//	This callback is used to translate pool.ntp.org domain name to an ip address
//	then do a SNTP request to set the date/time

// Result:
//	0	Success, ip is valid
//	1	DNS primary server fail
//	2	DNS alternate server fail

void dnsCb (int32_t result, uint8_t * ip)
{
	if (result == 0u)
	{
		// Success
		aaPrintf ("DNS Translated to [%d.%d.%d.%d]\n", ip[0], ip[1], ip[2], ip[3]) ;
		sntpRequest (ip, SNTP_TZ, sntpCb) ; ;
	}
	else if (result == 1u)
	{
		// DNS primary fail, try alternate DNS
		aaPrintf ("DNS Pri fail\n") ;
		dnsRequest (1, NULL, dnsCb) ;
	}
	else
	{
		// DNS alternate fail
		aaPrintf ("DNS Alt fail\n") ;
		timeIsUpdated = 0 ;			// Time update failed, free to retry
	}
}

//--------------------------------------------------------------------------------
// Need to update the time. Source may be SNTP or Linky.
// If mode is 1: When the time is updated then synchronize the history

void	timeUpdateRequest (uint32_t mode)
{
	if (mode == 1u)
	{
		// After time update synchronize power history
		timeIsUpdated = 1 ;		// This will be 2 when the date/time is updated
	}

	// If the net is on, then send SNTP request to update date/time
	if (statusWTest (STSW_NET_LINK_OFF) == 0)
	{
		// This starts a procedure to get an IP address, then send a SNTP request, then set the date/time
aaPrintf ("dnsRequest\n") ;
		dnsRequest (0, "pool.ntp.org", dnsCb) ;
	}
	else
	{
		// If present the Linky will updated date/time
		if (ISOPT_LINKY)
		{
aaPrintf ("Linky date request\n") ;
			bLinkyDateUpdate = true ;
		}
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// The power diverter main task

static	char			cmdBuffer [128] ;	// Large because forcing rules are large

void	AASun ()
{
	char		* pCmd ;
	char		* savePtr ;
	char		* pArg1 ;
	char		* pArg2 ;
	char		* pArg3 ;
 	int32_t		arg1 ;
 	int32_t		arg2 ;
 	int32_t		arg3 ;
 	uint32_t	ii ;
/*
	// Attempt to allow the application to be started after downloading by CubeProgrammer
 	aaPrintf ("\nRCC->CSR 0x%08X\n", RCC->CSR) ;
 	if ((RCC->CSR & RCC_CSR_SFTRSTF) == 0)
 	{
 		aaPuts ("\nbspResetHardware\n") ;
 		aaTaskDelay (50) ;
		bspResetHardware () ;
 	}
 	else
 	{
 		RCC->CSR |= RCC_CSR_RMVF ;
 	}
 	aaPrintf ("\nRCC->CSR 0x%08X\n", RCC->CSR) ;

*/

 	// Upon power on, internal pull-down resistors on UCPD1 CC1 and CC2 pins are enabled:
 	// disable these pull-down resistors
 	SYSCFG->CFGR1 |= SYSCFG_CFGR1_UCPD1_STROBE | SYSCFG_CFGR1_UCPD2_STROBE ;

	bspOutput (BSP_LED0, BSP_LED0_ON) ;		// Blue LED on

 	arg1 = AASunVersion () ;
 	aaPrintf ("\nAASun version %u.%02u\n", arg1 >> 16, arg1 & 0xFFFFu) ;

 	// On some devices HSE doesn't run...
	aaPuts ("Clock source: ") ;
	((RCC->CR & RCC_CR_HSERDY) != 0) ? aaPuts ("HSE\n") : aaPuts ("HSI\n") ;

 	displayInit () ;
 	pageUpdate () ;		// Displays the page 0 (AA logo)

 	memset (aaSunVariable, 0, sizeof (aaSunVariable)) ;
 	buttonInit () ;
 	inOutInit () ;
 	W25Q_Init () ;
 	timersInit () ;
 	timerStart (TIMER_ENERGY_IX, TIMER_ENERGY_PERIOD) ;
 	timeInit (& localTime) ;
	statusWClear (STSW_TIME_OK) ;
	if (ISOPT_TEMPERATURE)
	{
		tsInit () ;			// Temperature sensors
	}

	// Read configuration from FLASH
	readCfg () ;
	readTotalEnergyCounters () ;		// Read permanent energy counters
	memset (& energyJ,     0, sizeof (energyJ)) ;
	memset (& dayEnergyWh, 0, sizeof (dayEnergyWh)) ;
	dayEnergyWh.date = timeGetDayDate (& localTime) ;
	if (! histoInit ())
	{
		aaPuts ("histoInit error\n") ;
	}

	// Check the temperature sensors:
	// Compare the sensors of the configuration and those physically present
	if (ISOPT_TEMPERATURE)
	{
		tsRequest (TSREQUEST_CHECK) ;

		// Wait end of temperature sensors check
		while (tsRequestStatus (TSREQUEST_CHECK) == 0)
		{
			tempSensorNext () ;
			aaTaskDelay (2) ;
		}
		tsRequestAck (TSREQUEST_CHECK) ;
	}

	// Start LAN and low priority processes
	lowProcessesInit () ;

	// Start temperature sensors conversion
	if (ISOPT_TEMPERATURE)
	{
		tsRequest (TSREQUEST_CONV) ;
	}

	aaTaskDelay (2000) ;	// To allow to see the banner page, and the WIZNET to start
	pageSet (aaSunCfg.favoritePage) ;

	if (ISOPT_LINKY)
	{
		statusWSet (STSW_LINKY_ON) ;
		ticInit () ;		// Start Linky TIC receive
		ticSetCallback (myTicCallback) ;
	}

	diverterStart () ;		// Create and start the meter task

	// Get date/time
	// This will triggers time update, and date update in history data
	timeUpdateRequest (1) ;
/*
// Flush console input from Cube programmer
while (aaCheckGetChar () != 0)
{
	(void) aaGetChar () ;
}
*/
	bDiverterSet = true ;		// Enable diverting

	// Main loop: Wait for user commands, and acquired data
	while (1)
	{
		// Check if there is data to process (every second)
		if (collectionDataOk == 1u)
		{
			// Process data collected by the meter task
			// Copy collectionData so if processing takes longer than 1 second there will be no data lost
			acquiredData = collectionData ;
			collectionDataOk = 0 ;		// collectionData is free

			// Manage software timer tasks
			if (0 != timeTick (& localTime))	// To call every second
			{
				// It is midnight
				// Start the timer to get the time/date from Linky or DNS (daily time synchronization)
				// Don't do this at midnight to avoid time server overload
				timerStart (TIMER_DATE_IX, TIMER_DATE_PERIOD) ;

				aaSunVariable [ASV_DAYS]++ ;	// Running days counter
				aaSunVariable [ASV_ANTIL]++ ;	// Anti-legionella counter
			}
			if (timerExpired (TIMER_DATE_IX))
			{
				// Periodic time/date update after midnight
				if (timeIsUpdated == 0u)
				{
					timeUpdateRequest (1) ;	// When the date is received, timeIsUpdated will change to 2
				}
				timerStop (TIMER_DATE_IX) ;
			}
			if (timeIsUpdated == 2)
			{
				// Time/date newly updated, synchronize power history
				timeIsUpdated = 0 ;
				histoStart () ;
			}

			if (timerExpired (TIMER_ENERGY_IX))
			{
				writeTotalEnergyCounters () ;	// Periodic flash backup of total energy counters
			}

			if (timerExpired (TIMER_HISTO_IX))
			{
				// Compute the average power for the elapsed power history period.
				// If it is the last period of the day writes the history to the flash.
				histoNext () ;

				// If the time is not available, and a time update is not in progress
				// then request a time update
				// (this may be necessary after a general power outage and the Ethernet box boots up more slowly than the AASun)
				if (! statusWTest (STSW_TIME_OK)  &&  timeIsUpdated == 0u)
				{
					timeUpdateRequest (1) ;
				}
			}

			computedData.vRms       = voltCal * usqrt (acquiredData.voltSumSqr / COLLECTION_COUNT) ;	// VOLT_SHIFT  bits left shifted

			// Compute the power of the diverter resistor at Vrms
			// powerDiverterMax = ((U * U) /  Rref) << POWER_DIVERTER_SHIFT ;
			int32_t     pMax = (computedData.vRms >> (VOLT_SHIFT - POWER_DIVRES_SHIFT)) ;	// Vrms << POWER_DIVRES_SHIFT
			powerDiv_t	* pDiv = & powerDiv [diverterIndex] ;
			pMax =  (pMax * pMax) / pDiv->powerDiverterRref ;								// pMax << POWER_DIVRES_SHIFT
			pDiv->powerDiverterMax = pMax << (POWER_DIVERTER_SHIFT - POWER_DIVRES_SHIFT) ;	// powerDiverterMax << POWER_DIVERTER_SHIFT
			pDiv->powerDiverterOpen = (((pDiv->powerDiverterMax >> POWER_DIVERTER_SHIFT) * POWER_DIVERTER_OPENTHR) / 100)  << POWER_DIVERTER_SHIFT ;

			for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
			{
				iSensorCfg_t	* pICfg = & aaSunCfg.iSensor [ii] ;
				computedData.iData[ii].iRms      = pICfg->iCal     * usqrt (acquiredData.iData[ii].iSumSqr / (COLLECTION_COUNT / 64)) ;						// I_SHIFT     bits left shifted
				computedData.iData[ii].powerReal = pICfg->powerCal * (acquiredData.iData[ii].powerSum / (int32_t) COLLECTION_COUNT) - pICfg->powerOffset ;	// POWER_SHIFT bits left shifted
				computedData.iData[ii].powerApp  = (int32_t) (((int64_t) computedData.vRms * (int64_t) computedData.iData[ii].iRms) >>
											((VOLT_SHIFT + I_SHIFT) - POWER_SHIFT)) ;					// POWER_SHIFT bits left shifted
				// Lower the power values to not overflow the 32 bits
				computedData.iData[ii].cosPhi    = (1000 * (computedData.iData[ii].powerReal >> 10)) / (computedData.iData[ii].powerApp >> 10) ;			// Cos Phi * 1000
			}

			// The diverted power is accumulated every 1/2 main period
			// So divide by the count of 1/2 main period in 1 second: (1000000 / MAIN_PERIOD_US) * 2
			// The result is POWER_DIVERTER_SHIFT bits left shifted
			computedData.powerDiverted = acquiredData.powerDiverted / ((1000000 / MAIN_PERIOD_US) * 2) ;


			// Update energy counters:
			// We use the following formula:
			//     1W = 1J/s  then  1W x 1sec = 1J   (J = Joule)
			// As we get the power every second the power variables (like power1Real) can be considered as Joules
			// Also
			//		1Wh = 3600J
			// So when we have accumulated 3600J we have 1Wh
			energyJ.energy2 += computedData.iData[1].powerReal ;	// Joules << POWER_SHIFT
			if (energyJ.energy2 >= 0)
			{
				while (energyJ.energy2 >= (3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy2 ++ ;
					energyWh.energy2 ++ ;
					energyJ.energy2 -= (3600 << POWER_SHIFT) ;
				}
			}
			else
			{
				while (energyJ.energy2 <= -(3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy2 -- ;
					energyWh.energy2 -- ;
					energyJ.energy2 += (3600 << POWER_SHIFT) ;
				}
			}
			powerHistoryTemp.power2 += (computedData.iData[1].powerReal + POWER_HISTO_ROUND_ADD) >> POWER_HISTO_SHIFT_ADD ;

#if (defined IX_I3)
			energyJ.energy3 += computedData.iData[2].powerReal ;	// Joules << POWER_SHIFT
			if (energyJ.energy3 >= 0)
			{
				while (energyJ.energy3 >= (3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy3 ++ ;
					energyWh.energy3 ++ ;
					energyJ.energy3 -= (3600 << POWER_SHIFT) ;
				}
			}
			else
			{
				while (energyJ.energy3 <= -(3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy3 -- ;
					energyWh.energy3 -- ;
					energyJ.energy3 += (3600 << POWER_SHIFT) ;
				}
			}
			powerHistoryTemp.power3 += (computedData.iData[2].powerReal + POWER_HISTO_ROUND_ADD) >> POWER_HISTO_SHIFT_ADD ;
#endif
#if (defined IX_I4)
			energyJ.energy4 += computedData.iData[3].powerReal ;	// Joules << POWER_SHIFT
			if (energyJ.energy34 >= 0)
			{
				while (energyJ.energy4 >= (3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy4 ++ ;
					energyWh.energy4 ++ ;
					energyJ.energy4 -= (3600 << POWER_SHIFT) ;
				}
			}
			else
			{
				while (energyJ.energy4 <= -(3600 << POWER_SHIFT))
				{
					dayEnergyWh.energy4 -- ;
					energyWh.energy4 -- ;
					energyJ.energy4 += (3600 << POWER_SHIFT) ;
				}
			}
			powerHistoryTemp.power4 += (computedData.iData[3].powerReal + POWER_HISTO_ROUND_ADD) >> POWER_HISTO_SHIFT_ADD ;
#endif

			// Energy1 can be < 0 in case of export!!!
			{
				int32_t		energy = computedData.iData[0].powerReal ;
				if (energy >= 0)
				{
					energyJ.energyImported += energy ;
					while (energyJ.energyImported >= (3600 << POWER_SHIFT))
					{
						dayEnergyWh.energyImported ++ ;
						energyWh.energyImported ++ ;
						energyJ.energyImported -= (3600 << POWER_SHIFT) ;
					}
					powerHistoryTemp.imported += (energy + POWER_HISTO_ROUND_ADD) >> POWER_HISTO_SHIFT_ADD ;
				}
				else
				{
					energyJ.energyExported -= energy ;
					while (energyJ.energyExported >= (3600 << POWER_SHIFT))
					{
						dayEnergyWh.energyExported ++ ;
						energyWh.energyExported ++ ;
						energyJ.energyExported -= (3600 << POWER_SHIFT) ;
					}
					powerHistoryTemp.exported += (-energy + POWER_HISTO_ROUND_ADD) >> POWER_HISTO_SHIFT_ADD ;
				}
			}

			// Set diverting indicators, and check for diverter open circuit
			if (computedData.powerDiverted == 0) 	// W << POWER_DIVERTER_SHIFT
			{
				// Nothing to divert
				statusWClear (STSW_DIVERTING) ;
				statusWClear (STSW_DIVERTING_MAX) ;
				statusWClear (STSW_DIVERTER_OPEN) ;
			}
			else
			{
				statusWSet (STSW_DIVERTING) ;
				if (computedData.powerDiverted >= (uint32_t) pDiv->powerDiverterOpen)
				{
					// The power diverted is near max
					statusWSet   (STSW_DIVERTING_MAX) ;
					statusWClear (STSW_DIVERTER_OPEN) ;

					// TODO Check for diverter open circuit
					if (0)
					{
						// The power is not actually diverted
						computedData.powerDiverted = 0 ;
						statusWSet (STSW_DIVERTER_OPEN) ;
					}
				}
				else
				{
					// Some power diverted
					statusWClear (STSW_DIVERTING_MAX) ;
				}
			}

			// Get pulse counters counts
			pulsePowerPeriod++ ;
			if (pulsePowerPeriod == PULSE_P_PERIOD)
			{
				uint32_t		count ;
				pulseCounter_t	* pPulse ;

				pulsePowerPeriod = 0 ;

				for (ii = 0 ; ii < PULSE_COUNTER_MAX ; ii++)
				{
					pPulse = & pulseCounter [ii] ;
					if (pPulse->pulsepkWh == 0u)
					{
						continue ; // Unused counter
					}
					count = pulseGet (ii) ;
					pPulse->pulsePeriodCount = count ;
					energyWh.energyPulse [ii] += count ;
					powerHistoryTemp.powerPulse[ii] += count ;
					// Avoid overflow on energy total: simulate wrap
					if (energyWh.energyPulse [ii] > (int32_t) pPulse->pulseMaxCount)
					{
						energyWh.energyPulse [ii] -= pPulse->pulseMaxCount ;
					}
					dayEnergyWh.energyPulse [ii] += count ;	// No overflow expected on daily counters

					if ((ii == 0  &&  displayWTest (DPYW_DISPLAY_PULSE1))  ||  (ii == 1  && displayWTest (DPYW_DISPLAY_PULSE2)))
					{
						aaPrintf ("Pulse %d C:%-9u E:%-9u P:%-9u, Daily: C:%-9u E:%-9u\n",
								ii,
								// Total:
								energyWh.energyPulse [ii],
								(energyWh.energyPulse [ii] * pPulse->pulseEnergyCoef) >> PULSE_E_SHIFT,
								(count * pPulse->pulsePowerCoef) >> PULSE_P_SHIFT,
								// Daily:
								dayEnergyWh.energyPulse [ii],
								(dayEnergyWh.energyPulse [ii] * pPulse->pulseEnergyCoef) >> PULSE_E_SHIFT) ;
					}
				}
			}

			// Update diverted energy counter. This power is always positive
			if (diverterIndex == 0)
			{
				powerHistoryTemp.diverted1 += (computedData.powerDiverted + POWER_HISTO_DIVROUND_ADD) >> POWER_HISTO_DIVSHIFT_ADD ;
				energyJ.energyDiverted1 += computedData.powerDiverted ;		// Joules << POWER_DIVERTER_SHIFT
				while (energyJ.energyDiverted1 >= (3600 << POWER_DIVERTER_SHIFT))
				{
					dayEnergyWh.energyDiverted1 ++ ;
					energyWh.energyDiverted1 ++ ;
					energyJ.energyDiverted1 -= (3600 << POWER_DIVERTER_SHIFT) ;
				}
			}
			else
			{
				powerHistoryTemp.diverted2 += (computedData.powerDiverted + POWER_HISTO_DIVROUND_ADD) >> POWER_HISTO_DIVSHIFT_ADD ;
				energyJ.energyDiverted2 += computedData.powerDiverted ;		// Joules << POWER_DIVERTER_SHIFT
				while (energyJ.energyDiverted2 >= (3600 << POWER_DIVERTER_SHIFT))
				{
					dayEnergyWh.energyDiverted2 ++ ;
					energyWh.energyDiverted2 ++ ;
					energyJ.energyDiverted2 -= (3600 << POWER_DIVERTER_SHIFT) ;
				}
			}

			// Check the diverting rules and select the appropriate channel
			diverterNext () ;

			// Check ADC overflow
			if (acquiredData.vPeakAdcP > 500  ||  acquiredData.iData[0].iPeakAdcP > 500  ||  acquiredData.iData[1].iPeakAdcP > 500
#if (defined IX_I3)
					  ||  acquiredData.iData[2].iPeakAdcP > 500
#endif
#if (defined IX_I4)
					  ||  acquiredData.iData[3].iPeakAdcP > 500
#endif
				)
			{
				statusWSet (STSW_ADC_OVERFLOW) ;
			}
			else
			{
				statusWClear (STSW_ADC_OVERFLOW) ;
			}

			// Temperature sensors
			if (ISOPT_TEMPERATURE)
			{
				if (tsRequestStatus (TSREQUEST_CHECK))
				{
					tsRequestAck (TSREQUEST_CHECK) ;
					aaPuts ("TS Check done\n") ;
				}
				if (tsRequestStatus (TSREQUEST_SEARCH))
				{
					tsRequestAck (TSREQUEST_SEARCH) ;
					aaPuts ("TS Search done\n") ;
				}
				if (tsRequestStatus (TSREQUEST_CONV))
				{
					tsRequestAck (TSREQUEST_CONV) ;		// Conversion acknowledge
					tsRequest (TSREQUEST_CONV) ;		// Request new conversion

					// Anti-legionella processing
					if ((aaSunCfg.alFlag & AL_FLAG_EN) != 0)
					{
						if ((aaSunCfg.alFlag & AL_FLAG_INPUT) != 0)
						{
							// Use Input
							uint32_t	value ;
							if (inputGet (aaSunCfg.alFlag & AL_FLAG_NUMMASK, & value))
							{
								if (value == (uint32_t) aaSunCfg.alValue)
								{
									aaSunVariable [ASV_ANTIL] = 0 ;	// Reset anti-legionella counter
								}
							}
						}
						else
						{
							// Use temperature
							int32_t	temp ;
							if (tsGetTemp (aaSunCfg.alFlag & AL_FLAG_NUMMASK, & temp))
							{
								temp >>= TEMP_SENSOR_SHIFT ;
								if (temp >= aaSunCfg.alValue)
								{
									aaSunVariable [ASV_ANTIL] = 0 ;	// Reset anti-legionella counter
								}
							}
						}
					}
				}
			}

			// Update the display after all data are updated
			pageUpdate () ;

			// Debug displays
			if (displayWTest (DPYW_DISPLAY_WH))
			{
				// Display total energy
				aaPrintf ("Inp %6d, Exp %6d, Div1 %6d, Div2 %6d, E2 %6d",
						energyWh.energyImported, energyWh.energyExported, energyWh.energyDiverted1, energyWh.energyDiverted2, energyWh.energy2) ;
#if (defined IX_I3)
				aaPrintf (", E3 %6d", energyWh.energy3) ;
#endif
#if (defined IX_I4)
				aaPrintf (", E4 %6d", energyWh.energy4) ;
#endif
				aaPrintf (", C1 %6d, C2 %6d\n", energyWh.energyPulse[0], energyWh.energyPulse[1]) ;
			}

			// Debug display of I sensors data every second,if required by DPYW_DISPLAY_DATAx bits
			{
				uint32_t	dpyw = displayWord >> 0 ; 	// Shift DPYW_DISPLAY_DATAx bits on the right
				for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
				{
					if ((dpyw & (1u << ii)) != 0u)
					{
						aaPuts ("V ") ;
						iFracPrint (computedData.vRms, VOLT_SHIFT, 3, 2) ;
						aaPuts (", ") ;

						aaPrintf ("I%d ", ii+1) ;
						iFracPrint (computedData.iData[ii].iRms, I_SHIFT, 3, 2) ;
						aaPuts (", ") ;
						aaPuts ("p1Real ") ;
						iFracPrint (computedData.iData[ii].powerReal, POWER_SHIFT, 5, 2) ;
						aaPuts (", ") ;
						aaPuts ("p1App ") ;
						iFracPrint (computedData.iData[ii].powerApp, POWER_SHIFT, 5, 2) ;
						aaPuts (", ") ;
						aaPrintf ("CPhi %5d", computedData.iData[ii].cosPhi) ;
						if (ii == 0)
						{
							aaPuts (", pDiv ") ;
							iFracPrint (computedData.powerDiverted, POWER_DIVERTER_SHIFT, 5, 2) ;
						}
						aaPuts ("\n") ;
					}
				}
			}

			// The synchronization lock is checked every main period, and cleared on the next second
			statusWClear   (STSW_NOT_SYNC) ;
			bspOutput (BSP_LED0, BSP_LED0_OFF) ;		// Blue LED off: End of calculations performed every second
		}
		// End of data processing


		// Check for user command line
		if (0 == aaGetsNonBlock (cmdBuffer, sizeof (cmdBuffer)))
		{
			// No command, do other stuff: check buttons, Linky, etc.
			uint32_t	buttons = buttonGet () ;
			if ((buttons & 1u) != 0u)
			{
				pageNext (PAGE_UP) ;
				buttonAck (1) ;
			}
			if ((buttons & 2u) != 0u)
			{
				pageNext (PAGE_DOWN) ;
				buttonAck (2) ;
			}

			// Because this updates the date used by this task, call it in this task
			// So no need for synchronization/exclusion
			if (ISOPT_LINKY)
			{
				ticNext () ;	// Linky receive
			}

			aaTaskDelay (5) ;
			continue ;
		}

		// Command received: extract command and parameters
		pCmd = strtok_r (cmdBuffer, " \t", & savePtr) ;
		arg1 = 0 ;
		arg2 = 0 ;
		arg3 = 0 ;
		pArg3 = NULL ;
		pArg2 = NULL ;
		pArg1 = strtok_r (NULL, " \t", & savePtr) ;
		if (pArg1 != NULL)
		{
			// Some commands doesn't want to get tokens
			if (0 != strcmp (pCmd, "cdr")  &&  0 != strcmp (pCmd, "cfr"))
			{
				arg1 = strtol (pArg1, (char **) NULL, 0) ;

				pArg2 = strtok_r (NULL, " \t", & savePtr) ;
				if (pArg2 != NULL)
				{
					arg2 = strtol (pArg2, (char **) NULL, 0) ;

					pArg3 = strtok_r (NULL, " \t", & savePtr) ;
					if (pArg3 != NULL)
					{
						arg3 = strtol (pArg3, (char **) NULL, 0) ;
					}
				}
			}
		}

		// Execute the command
		if (0 == strcmp ("?", pCmd))
		{
		 	arg1 = AASunVersion () ;
		 	aaPrintf ("AASun version %u.%02u\n", arg1 >> 16, arg1 & 0xFFFFu) ;

			aaPrintf ("dn         Enable/disable In data display every second\n") ;
			aaPrintf ("de         Display energies\n") ;
			aaPrintf ("dpc n      Display pulse counter data\n") ;
			aaPrintf ("ds         Display samples (dss for samples around 0 crossing)\n") ;
			aaPrintf ("dd         Display diverting RT data (%u)\n", displayWTest (DPYW_DISPLAY_DIV_DATA)) ;
			aaPrintf ("dp         Synchro PID trace toggle\n") ;
			aaPrintf ("sd [1|0]   Toggle/Start/Stop diverting (%u)\n", statusWTest (STSW_DIV_ENABLED)) ;

			aaPrintf ("c?         Display calibration values\n") ;
			aaPrintf ("cv v       Set voltCal\n") ;
			aaPrintf ("cph v      Set phaseCal\n") ;
			aaPrintf ("cio n v    Set current adc offset\n") ;
			aaPrintf ("ci  n v    Set iCal for sensor n\n") ;
			aaPrintf ("cp  n v    Set powerCal for sensor n\n") ;
			aaPrintf ("cpo n v    Set power offset\n") ;
			aaPrintf ("cpc n v    Set Pulse counter pulse/kWh\n") ;
			aaPrintf ("cpm n v    Set power margin\n") ;
			aaPrintf ("cpmax n w [v] Max power to divert\n") ;
			aaPrintf ("crd        Read configuration from flash\n") ;
			aaPrintf ("cwr        Write configuration to flash\n") ;
			aaPrintf ("cdef       Restore default configuration\n") ;
			aaPrintf ("ckd [p i]  Set diverting factor\n") ;
			aaPrintf ("cks [p i]  Synchro PID factors x10000\n") ;
			aaPrintf ("clip       Set lan IP address\n") ;
			aaPrintf ("clmask     Set lan subnet mask\n") ;
			aaPrintf ("clgw       Set lan gateway address\n") ;
			aaPrintf ("cldns      Set lan DNS address\n") ;
			aaPrintf ("cldhcp v   Set lan mode 1=static 2=DHCP\n") ;

			aaPrintf ("cfp v      Favorite display page\n") ;
			aaPrintf ("cdr n txt  Set diverting rule n\n") ;
			aaPrintf ("cfr n txt  Set forcing rules n\n") ;
			aaPrintf ("cdre n v   Diverting rule 0:off, 1:on\n") ;
			aaPrintf ("cfre n v   Forcing   rule 0:off, 1:on\n") ;

			aaPrintf ("cal ?s v    Anti legionella: sensor value, ? help \n") ;

			aaPrintf ("e ?RWZzd   Energy: read write zero display\n") ;
			aaPrintf ("cen n name Set name of the energy counter n\n") ;
			aaPrintf ("Sp % [n]   SSR n power [0,100%]\n") ;
			aaPrintf ("Si v [n)   SSR n index [0,511]\n") ;
			aaPrintf ("Sv v|+|-   SSR 2 value [0,511]\n") ;

			if (ISOPT_LINKY)
			{
				aaPrintf ("li         Linky display (mode %c)\n", (ticMode ==  MODE_HIS) ? 'H' : 'S') ;
			}

			if (ISOPT_TEMPERATURE)
			{
				aaPrintf ("tss        Start temperature sensors search\n") ;
				aaPrintf ("tsr v v v  Set   temperature sensors ranks (1..4)\n") ;
				aaPrintf ("tsc        Check temperature sensors\n") ;
				aaPrintf ("dts        Display temperature\n") ;
			}

			aaPrintf ("fman n     Set manual order for the forcing\n") ;

			aaPrintf ("h dyw      History power dump-today dump-yesterday write\n") ;

			aaPrintf ("time [h m s y m d] Set time (%d)\n", (statusWTest (STSW_TIME_OK) != 0) ? 1 : 0) ;
			aaPrintf ("ltime [1]  Set time from LAN, optional power histo sync\n") ;
			aaPrintf ("reset [boot] Reset MCU [and go bootloader]\n") ;
aaPrintf ("in         Get input values\n") ;
aaPrintf ("out n v    Set output n to value v\n") ;
aaPrintf ("f x        Flash test: i z w r m\n") ;
aaPrintf ("df  a      Dump flash\n") ;
			aaPrintf ("q         Quit\n") ;
		}

		else if (0 == strcmp ("q", pCmd))
		{
			// Aborted by user
			diverterStop () ;
			return ;
		}

		// -------------- Display commands (calibration help) ----------------------

		// d1 to d4: Enable/disable current sensor data display every second
		else if (pCmd [0] == 'd'  &&  pCmd [2] == 0  &&  pCmd [1] > '0'  &&  pCmd [1] < '5')
		{
			ii = pCmd [1] - '0' - 1 ;	// Index: 0 to 3
			ii = 1u << (ii + 0) ;		// DPYW_DISPLAY_DATA1 to DPYW_DISPLAY_DATA4
			displayWToggle (ii) ;
		}

		else if (0 == strcmp ("de", pCmd))		// Display energies every second
		{
			displayWToggle (DPYW_DISPLAY_WH) ;
		}

		else if (0 == strcmp ("dpc", pCmd))		// Display pulse counters data
		{
			if (pArg1 != NULL)
			{
				if (arg1 == 1)
				{
					displayWToggle (DPYW_DISPLAY_PULSE1) ;
				}
				else if (arg1 == 2)
				{
					displayWToggle (DPYW_DISPLAY_PULSE2) ;
				}
				else
				{
					aaPuts ("Bad pulse counter number (1, 2)\n") ;
				}
			}
			aaPrintf ("dpc  %d  %d\n", displayWTest (DPYW_DISPLAY_PULSE1), displayWTest (DPYW_DISPLAY_PULSE2)) ;
		}

		else if (0 == strcmp ("ds", pCmd) ||  0 == strcmp ("dss", pCmd))		// Display samples
		{
			// ds		Displays all the samples of the period
			// dss		Displays only the samples around the 0 crossing
			if (! statusWTest (STSW_NOT_SYNC))
			{
				int16_t		* pSample ;
				uint32_t	nn ;

				samplesState = samplesStateStart ;
				while (samplesState != samplesStateDone)
				{
					aaTaskDelay (1u) ;
				}

				// Displays the ADC values + phase corrected voltage
				pSample = (int16_t *) samples ;
				nn = MAIN_SAMPLE_COUNT ;
				if (0 == strcmp ("dss", pCmd))
				{
					// Displays only the values in the center of the period
					ii = (MAIN_SAMPLE_COUNT / 2u) - 10u ;
					nn = 20u ;
					pSample = (int16_t *) & samples [((MAIN_SAMPLE_COUNT / 2u) - 10u) * (ADC_CHANCOUNT + 1)] ;
				}
				for (ii = 0 ; ii < nn ; ii++)
				{
					for (uint32_t jj = 0 ; jj < ADC_CHANCOUNT + 1 ; jj++)
					{
						aaPrintf (" %4d", * pSample++) ;
					}
					aaPutChar ('\n') ;
				}
				samplesState = samplesStateIdle ;
				aaPrintf ("adcOffset : %d\n", adcOffset) ;
			}
			else
			{
				aaPrintf ("Not synchronized\n") ;
			}
		}

		else if (* pCmd ==  'c')
		{
			// All configuration commands goes here

			// -------------- Print calibration values ----------------------
			if (0 == strcmp ("c?", pCmd))
			{
				// Diverter configuration
				// This displays the commands as they should be entered by the user

				aaPrintf ("cv      %d\n", voltCal) ;
				aaPrintf ("cph     %d\n", phaseCal) ;
				for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
				{
					aaPrintf ("cio     %d %d\n", ii+1, aaSunCfg.iSensor [ii].iAdcOffset) ;
					aaPrintf ("ci      %d %d\n", ii+1, aaSunCfg.iSensor [ii].iCal) ;
					aaPrintf ("cp      %d %d\n", ii+1, aaSunCfg.iSensor [ii].powerCal) ;
					aaPrintf ("cpo     %d %d\n", ii+1, aaSunCfg.iSensor [ii].powerOffset >> POWER_SHIFT) ;
				}

				aaPrintf ("cpc     1 %d\n", pulseCounter [0].pulsepkWh) ;
				aaPrintf ("cpc     2 %d\n", pulseCounter [1].pulsepkWh) ;

				aaPrintf ("cpm     1 %d\n", powerDiv [0].powerMargin >> POWER_SHIFT) ;
				aaPrintf ("cpmax   1 %d\n", powerDiv [0].powerDiverter230) ;
				aaPrintf ("cpm     2 %d\n", powerDiv [1].powerMargin >> POWER_SHIFT) ;
				aaPrintf ("cpmax   2 %d\n", powerDiv [1].powerDiverter230) ;

				aaPuts   ("cdr     1 ") ;
				divRulePrint (& aaSunCfg.diverterRule[0], cmdBuffer, sizeof (cmdBuffer)) ;
				aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
				aaPuts   ("cdr     2 ") ;
				divRulePrint (& aaSunCfg.diverterRule[1], cmdBuffer, sizeof (cmdBuffer)) ;
				aaPuts (cmdBuffer) ; aaPutChar ('\n') ;

				for (ii = 0 ; ii < FORCE_MAX ; ii++)
				{
					if (0 != forceRulePrint (& aaSunCfg.forceRules [ii], true, cmdBuffer, sizeof (cmdBuffer)))
					{
						// This force is valid
						aaPrintf ("cfr     %d ", ii+1) ;
						aaPuts (cmdBuffer) ; aaPuts (" / ") ;
						forceRulePrint (& aaSunCfg.forceRules [ii], false, cmdBuffer, sizeof (cmdBuffer)) ;
						aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
					}
				}

				for (ii = 0 ; ii < energyCountersCount ; ii++)
				{
					aaPrintf ("cen     %u %s\n", ii, aaSunCfg.eName [ii]) ;
				}

				aaPrintf ("cks     %u %u\n",
									(syncPropFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR,
									(syncIntFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR) ;
				aaPrintf ("ckd     %u %u\n", powerPropFactor, powerIntFactor) ;

				// Ethernet configuration
				aaPuts ("clip   ") ;
				for (ii = 0 ; ii < 4 ; ii++)
				{
					aaPrintf (" %3d", aaSunCfg.lanCfg.ip [ii]) ;
				}
				aaPuts ("\nclmask ") ;
				for (ii = 0 ; ii < 4 ; ii++)
				{
					aaPrintf (" %3d", aaSunCfg.lanCfg.sn [ii]) ;
				}
				aaPuts ("\nclgw   ") ;
				for (ii = 0 ; ii < 4 ; ii++)
				{
					aaPrintf (" %3d", aaSunCfg.lanCfg.gw [ii]) ;
				}
				aaPuts ("\ncldns  ") ;
				for (ii = 0 ; ii < 4 ; ii++)
				{
					aaPrintf (" %3d", aaSunCfg.lanCfg.dns [ii]) ;
				}
				aaPutChar('\n') ;
				aaPrintf ("cldhcp  %c\n", (aaSunCfg.lanCfg.dhcp) == 1 ? 's' : 'd') ;

				// Miscellaneous configuration
				aaPrintf ("cfp     %d\n", aaSunCfg.favoritePage) ;

				// Anti-legionella
				if ((aaSunCfg.alFlag & AL_FLAG_EN) == 0)
				{
					aaPuts ("cal     0\n") ;
				}
				else
				{
					aaPrintf ("cal     %c%d %d\n",
							((aaSunCfg.alFlag & AL_FLAG_INPUT) != 0) ? 'I' : 'T',
							(aaSunCfg.alFlag & AL_FLAG_NUMMASK) + 1,
							aaSunCfg.alValue) ;
				}

				// Not part of the configuration
				aaPuts ("\nMAC address  ") ;
				const uint8_t * pMac = wGetMacAddress () ;
				for (ii = 0 ; ii < 6 ; ii++)
				{
					aaPrintf (" 0x%02X", pMac [ii]) ;
				}
				aaPuts ("\n") ;
			}

			else if (0 == strcmp ("cv", pCmd))		// Set voltCal
			{
				if (pArg1 != NULL  &&  arg1 != 0)
				{
					voltCal = arg1 ;
					aaPuts ("New ") ;
				}
				aaPrintf ("voltCal %d\n", voltCal) ;
			}

			else if (0 == strcmp ("cio", pCmd))		// Set I adc offset
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= I_SENSOR_COUNT  &&  pArg2 != NULL)
				{
					arg1-- ;
					aaSunCfg.iSensor [arg1].iAdcOffset = arg2 ;
					aaPrintf ("New offset I%d: %d\n", arg1+1, aaSunCfg.iSensor [arg1].iAdcOffset) ;
				}
				else
				{
					for (ii = 0 ; ii < I_SENSOR_COUNT ; ii++)
					{
						aaPrintf ("Offset I%d: %d\n", ii+1, aaSunCfg.iSensor [ii].iAdcOffset) ;
					}
				}
			}

			else if (0 == strcmp ("ci", pCmd))		// Set iCal
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= I_SENSOR_COUNT)
				{
					if (pArg2 != NULL)
					{
						aaSunCfg.iSensor [arg1-1].iCal = arg2 ;
						aaPuts ("New ") ;
					}
					aaPrintf ("iCal I%d: %d\n", arg1, aaSunCfg.iSensor [arg1-1].iCal) ;
				}
			}

			else if (0 == strcmp ("cp", pCmd))		// Set pCal
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= I_SENSOR_COUNT)
				{
					if (pArg2 != NULL)
					{
						aaSunCfg.iSensor [arg1-1].powerCal = arg2 ;
						aaPuts ("New ") ;
					}
					aaPrintf ("powerCal I%d: %d\n", arg1, aaSunCfg.iSensor [arg1-1].powerCal) ;
				}
			}

			else if (0 == strcmp ("cpo", pCmd))		// Set p1Cal offset
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= I_SENSOR_COUNT)
				{
					if (pArg2 != NULL)
					{
						aaSunCfg.iSensor [arg1-1].powerOffset = arg2  << POWER_SHIFT;
						aaPuts ("New ") ;
					}
					aaPrintf ("powerOffset I%d: %d\n", arg1, aaSunCfg.iSensor [arg1-1].powerOffset >> POWER_SHIFT) ;
				}
			}


			else if (0 == strcmp ("cph", pCmd))		// Set phaseCal
			{
				if (pArg1 != NULL  &&  arg1 != 0)
				{
					phaseCal = arg1 ;
					aaPuts ("New ") ;
				}
				aaPrintf ("phaseCal %d\n", phaseCal) ;
			}

			else if (0 == strcmp ("cks", pCmd))	// synchronization PI factors
			{
				//	cks c			Restore default PI factors
				//	cks 1000 40		Set new P and I factors
				if (pArg1 == NULL)
				{
					// Print factors as integer (multiplied by 10000)
					aaPrintf ("p=%u  i=%u\n",
							(syncPropFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR,
							(syncIntFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR) ;
				}
				else if (pArg1 != NULL && * pArg1 == 'c')
				{
					// Set default synchronization PI controller factors
					syncPropFactor = SPID_P_FACTOR ;
					syncIntFactor = SPID_I_FACTOR ;
				}
				else if (pArg1 != NULL  &&  pArg2 != NULL)
				{
					// Set new synchronization PI controller factors
					syncPropFactor = (arg1 * SPID_SCALE_FACTOR + 5000) / 10000 ;
					syncIntFactor = (arg2 * SPID_SCALE_FACTOR + 5000) / 10000 ;
					aaCriticalEnter () ;
					sPidFactors (syncPropFactor, syncIntFactor) ;
					aaCriticalExit () ;
				}
			}

			else if (0 == strcmp ("cen", pCmd))		// Set energy counter name
			{
				if (pArg1 != NULL  &&  pArg2 != NULL  &&  arg1 < (int32_t) energyCountersCount)
				{
					int32_t len = strlen (pArg2) ;
					if (len >= ENAME_MAX)
					{
						pArg2 [ENAME_MAX] = 0 ;
					}
					strcpy (aaSunCfg.eName [arg1], pArg2) ;
					aaPrintf ("energy [%d] : %s\n", arg1, aaSunCfg.eName [arg1]) ;
				}
				else
				{
					aaPuts ("Error\n") ;
				}
			}

			else if (0 == strcmp ("cpc", pCmd))		// Set pulse counters pulse/kWh
			{
				// cpc 1 2000		Set counter 1 to 2000 pulse/kWh
				// cpc 2 0			Pulse counter 2 not used
				if (pArg1 != NULL  &&  pArg2 != NULL)
				{
					if (arg1 == 1  ||  arg1 == 2)
					{
						arg1-- ;

						pulseCounter [arg1].pulsepkWh       = arg2 ;
						pulseCounter [arg1].pulseEnergyCoef = (1000 << PULSE_E_SHIFT) / arg2 ;
						pulseCounter [arg1].pulsePowerCoef  = ((3600000 << PULSE_P_SHIFT) / PULSE_P_PERIOD) / arg2 ;
						pulseCounter [arg1].pulseMaxCount   = (PULSE_E_MAX / 1000) * arg2 ;
						aaPrintf ("Pulse/kWh %d:  %d\n", arg1, pulseCounter [arg1].pulsepkWh) ;
						pHistoHeader->powerPulse[arg1] = arg2 != 0 ;	// To enable/disable counter display in power history
					}
				}
				else
				{
					aaPrintf ("Pulse/kWh 1:  %d\n", pulseCounter [0].pulsepkWh) ;
					aaPrintf ("          2:  %d\n", pulseCounter [1].pulsepkWh) ;
				}
			}

			else if (0 == strcmp ("cpm", pCmd))		// Set power margin
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 < 2  &&  pArg2 != NULL)
				{
					powerDiv [arg1].powerMargin = arg2  << POWER_SHIFT ;
				}
				aaPrintf ("Power Margin: %u  %u\n", powerDiv [0].powerMargin >> POWER_SHIFT, powerDiv [1].powerMargin >> POWER_SHIFT) ;
			}

			else if (0 == strcmp ("cpmax", pCmd))	// Set max diverted power
			{
				if (pArg1 != NULL &&  arg1 > 0   &&  arg1 < 3)	// arg1 = diverter index
				{
					arg1 -- ;
					if (pArg2 != NULL &&  arg2 > 0)				// arg2 is diverter power
					{
						if (pArg3 != NULL &&  arg3 > 0)			// arg3 is optional voltage
						{
							// arg2 is the max power at the voltage specified by arg3
							powerDiv [arg1].powerDiverter230 = (230 * 230 * arg2) / (arg3 * arg3) ;
						}
						else
						{
							// arg2 is the max power at 230V
							powerDiv [arg1].powerDiverter230 = arg2 ;
						}
						powerDiv [arg1].powerDiverterMax = powerDiv [arg1].powerDiverter230 << POWER_DIVERTER_SHIFT ;
						powerDiv [arg1].powerDiverterRref = ((230 * 230 + (powerDiv [arg1].powerDiverter230 / 2)) << POWER_DIVRES_SHIFT) / powerDiv [arg1].powerDiverter230 ;
					}
				}
				aaPrintf ("Div 1 power Max: %4d (230V)  Resistor %3d.%02d\n"
	 					  "Div 2 power Max: %4d (230V)  Resistor %3d.%02d\n",
						  powerDiv [0].powerDiverter230,
						  powerDiv [0].powerDiverterRref >> POWER_DIVRES_SHIFT,
						  ((powerDiv [0].powerDiverterRref & POWER_DIVRES_SHIFT_M) * 100) / POWER_DIVRES_SHIFT_V,
						  powerDiv [1].powerDiverter230,
						  powerDiv [1].powerDiverterRref >> POWER_DIVRES_SHIFT,
						  ((powerDiv [1].powerDiverterRref & POWER_DIVRES_SHIFT_M) * 100) / POWER_DIVRES_SHIFT_V) ;
			}

			else if (0 == strcmp ("ckd", pCmd))		// Set power diverter PI factors
			{
				if (pArg1 == NULL)
				{
					// Print factors
					aaPrintf ("p=%u  i=%u\n", powerPropFactor, powerIntFactor) ;
				}
				else if (pArg1 != NULL && * pArg1 == 'c')
				{
					// Set default diverter PI controller factors
					powerPropFactor = PPID_P_FACTOR ;
					powerIntFactor  = PPID_I_FACTOR ;
				}
				else if (pArg1 != NULL  &&  pArg2 != NULL)
				{
					// Set new diverter PI controller factors
					aaCriticalEnter () ;
					powerPropFactor = arg1 ;
					powerIntFactor  = arg2 ;
					aaCriticalExit () ;
				}
			}

			// -------------- Configuration EEPROM management ----------------------

			else if (0 == strcmp ("crd", pCmd))		// Read configuration from flash
			{
				readCfg () ;
				if (statusWTest (STSW_NOT_FLASHCFG) == 0u)
				{
					aaPuts ("Ok\n") ;
				}
				else
				{
					aaPuts ("Error: default cfg\n") ;
				}
			}

			else if (0 == strcmp ("cwr", pCmd))		// Write configuration to flash
			{
				if (! writeCfg ())
				{
					aaPuts ("Error\n") ;
				}
				else
				{
					aaPuts ("Ok\n") ;
				}
			}

			else if (0 == strcmp ("cdef", pCmd))	// Restore default configuration
			{
				defaultCfg () ;
				aaPuts ("Ok\n") ;
			}

			// -------------- Ethernet configuration ----------------------

			else if (0 == strncmp ("cl", pCmd, 2))			// Ethernet interface configuration
			{

				if (0 == strcmp ("cldhcp", pCmd))			// Set lan mode 1=static 2=DHCP
				{
					if (* pArg1 == 's')
					{
						aaSunCfg.lanCfg.dhcp = 1  ;
						aaPuts ("Ok\n") ;
					}
					else if (* pArg1 == 'd')
					{
						aaSunCfg.lanCfg.dhcp = 2  ;
						aaPuts ("Ok\n") ;
					}
					else
					{
						aaPuts ("Error\n") ;
					}
				}

				else	// Lan parameters with 4 values
				{
					uint8_t		val [4] ;
					bool		result = false ;

					val [0] = arg1 ;
					val [1] = arg2 ;
					val [2] = arg3 ;

					pArg1 = strtok_r (NULL, " \t", & savePtr) ;
					if (pArg1 != NULL)
					{
						val [3] = strtoul (pArg1, (char **) NULL, 0) ;
						result = true ;

						if (0 == strcmp ("clip", pCmd))			// Set lan IP address
						{
							memcpy (& aaSunCfg.lanCfg.ip, & val, 4) ;
						}
						else if (0 == strcmp ("clmask", pCmd))	// Set lan subnet mask
						{
							memcpy (& aaSunCfg.lanCfg.sn, & val, 4) ;
						}
						else if (0 == strcmp ("clgw", pCmd))	// Set lan gateway address
						{
							memcpy (& aaSunCfg.lanCfg.gw, & val, 4) ;
						}
						else if (0 == strcmp ("cldns", pCmd))	//  Set lan DNS address
						{
							memcpy (& aaSunCfg.lanCfg.dns, & val, 4) ;
						}
						else
						{
							result = false ;
						}
					}
					if (result == true)
					{
						aaPrintf ("%u %u %u %u\n", 	val [0], val [1], val [2], val [3]) ;
					}
					else
					{
						aaPuts ("Error\n") ;
					}
				}
			}

			else if (0 == strcmp ("cdr", pCmd))		// Set a diverting rule
			{
				if (pArg1 != NULL)
				{
					arg1 = strtol (pArg1, (char **) NULL, 0) ;
					if (arg1 == 1 || arg1 == 2)
					{
						divRule_t	rule ;
						uint32_t	error ;

						// The rule is pointer to by savePtr
						if (divRuleCompile (& rule, savePtr, & error))
						{
							aaSunCfg.diverterRule [arg1-1] = rule ;
							divRulePrint (& rule, cmdBuffer, sizeof (cmdBuffer)) ;
							aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
						}
						else
						{
							aaPrintf ("Error %u\n", error) ;
						}
					}
				}
			}

			else if (0 == strcmp ("cfr", pCmd))		// Set a forcing rule
			{
				if (pArg1 != NULL)
				{
					arg1 = strtol (pArg1, (char **) NULL, 0) ;
					if (arg1 > 0  &&  arg1 <= FORCE_MAX)
					{
						forceRules_t	force ;
						char			* pStart, * pStop ;
						uint32_t		error ;

						// The text after arg1 is pointed to by savePtr
						pStart = strtok_r (NULL, "/", & savePtr) ;		//  '/' is the separator between start and stop rules
						pStop  = savePtr ;
						if (pStart == NULL  ||  pStop == NULL)
						{
							aaPuts ("Bad rule, missing rule separator '/' ?\n") ;
						}
						else
						{
							memset (& force, 0, sizeof (force)) ;
							if (forceRuleCompile (& force, pStart, pStop, & error))
							{
								// The force rules are Ok
								forceRules_t * pForce = & aaSunCfg.forceRules [arg1-1] ;

								// It is mandatory to invalidate the forcing before setting new rules
								forceRuleInvalidate (pForce) ;
								* pForce = force ;
								forceRulePrint (pForce, true, cmdBuffer, sizeof (cmdBuffer)) ;	// Print start rules
								aaPrintf ("Force %u: ", arg1) ;
								aaPuts (cmdBuffer) ; aaPuts (" / ") ;
								forceRulePrint (pForce, false, cmdBuffer, sizeof (cmdBuffer)) ;	// Print stop rules
								aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
							}
							else
							{
								aaPrintf ("Error %u\n", error) ;
							}
						}
					}
				}
			}

			else if (0 == strcmp ("cdre", pCmd))		// Diverting rule enable/disable
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 < 3)
				{
					arg1-- ;
					if (pArg2 != NULL)
					{
						divRuleEnable (& aaSunCfg.diverterRule [arg1], arg2 != 0) ;
					}
					divRulePrint (& aaSunCfg.diverterRule [arg1], cmdBuffer, sizeof (cmdBuffer)) ;	// Print the rule
					aaPrintf ("Diverter %u: ", arg1+1) ;
					aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
				}
			}

			else if (0 == strcmp ("cfre", pCmd))		// Forcing rule enable/disable
			{
				if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= FORCE_MAX)
				{
					forceRules_t	* pForce ;

					pForce = & aaSunCfg.forceRules [arg1-1] ;
					if (pArg2 != NULL)
					{
						forceRuleEnable (pForce, arg2 != 0) ;
					}
					forceRulePrint (pForce, true, cmdBuffer, sizeof (cmdBuffer)) ;	// Print start rules
					aaPrintf ("Force %u: ", arg1) ;
					aaPuts (cmdBuffer) ; aaPuts (" / ") ;
					forceRulePrint (pForce, false, cmdBuffer, sizeof (cmdBuffer)) ;	// Print stop rules
					aaPuts (cmdBuffer) ; aaPutChar ('\n') ;
				}
			}

			// -------------- Miscellaneous configuration ----------------------

			else if (0 == strcmp ("cal", pCmd))		// Anti legionella
			{
				// cal				Display anti-legionella help and configuration
				// cal '0'			Disable AL
				// cal 't' n tt		Enable AL, n is temperature rank 1..TEMP_SENSOR_MAX, tt is temperature threshold
				// cal 'i' n v		Enable AL, n is input number 1..4, v is input value 0 or 1
				// cal 'c'  		Display day counter value
				// cal 'c' v		Set day counter value

				if (pArg1 == NULL  || (pArg1  != NULL  &&  * pArg1 == '?'))
				{
					aaPuts ("cal       Display anti-legionella configuration\n"
					        "cal 0     Disable AL\n"
							"cal tn t  Enable AL, n is temperature rank 1..TEMP_SENSOR_MAX, tt is temperature threshold\n"
							"cal in v  Enable AL, n is input number 1..4, v is input value 0 or 1\n"
							"cal c     Display day counter value\n"
							"cal c v   Set day counter value\n") ;
				}
				else if (pArg1 != NULL)
				{
					aaStrToUpper (pArg1) ;
					if (* pArg1 == 'C')
					{
						if (pArg2 != NULL)
						{
							aaSunVariable [ASV_ANTIL] = arg2 ;	// For test: set AL counter value
						}
						aaPrintf ("Anti-legionella day counter: %u\n", aaSunVariable [ASV_ANTIL])  ;
					}
					else
					{
						if (* pArg1 == '0')
						{
							aaSunCfg.alFlag = 0 ;	// Disable AL
							aaSunCfg.alValue = 0 ;
						}
						else if (pArg2 != NULL  &&  (* pArg1 == 'T' || * pArg1 == 'I'))
						{
							arg1 = strtol (pArg1+1, NULL, 10) ;		// n is the rank of Tn or the index of In
							if (* pArg1 == 'T'  &&  arg1 > 0  &&  arg1 <= TEMP_SENSOR_MAX)
							{
								aaSunCfg.alFlag = AL_FLAG_EN | (arg1 - 1) ;
								aaSunCfg.alValue = arg2 ;
							}
							else if (arg1 > 0  &&  arg1 <= DIVE_INPUT_MAX)
							{
								aaSunCfg.alFlag = AL_FLAG_EN | AL_FLAG_INPUT | (arg1 - 1) ;
								aaSunCfg.alValue = arg2 != 0 ;	// alValue is 0 or 1
							}
						}
					}
				}
				// Display AL configuration
				if ((aaSunCfg.alFlag & AL_FLAG_EN) == 0)
				{
					aaPuts ("cal     0\n") ;
				}
				else
				{
					aaPrintf ("cal     %c%d %d\n",
							((aaSunCfg.alFlag & AL_FLAG_INPUT) != 0) ? 'I' : 'T',
							(aaSunCfg.alFlag & AL_FLAG_NUMMASK) + 1,
							aaSunCfg.alValue) ;
				}
			}

			else if (0 == strcmp ("cfp", pCmd))		// Set favorite display page
			{
				// cfp 1
				if (pArg1 != NULL)
				{
					pageSet (arg1) ;
					aaSunCfg.favoritePage = pageGet () ;
				}
				else
				{
					aaPuts ("Error\n") ;
				}
			}
		}

		else if (0 == strcmp ("e", pCmd))		// Energy counters
		{
			switch (* pArg1)
			{
				default:
				case '?':
					aaPrintf ("?    Print Energy menu\n") ;
					aaPrintf ("R    Read  total energy from FLASH\n") ;
					aaPrintf ("W    Write total energy to FLASH\n") ;
					aaPrintf ("Z n  Zero  total energy counter (a|0:8)\n") ;
					aaPrintf ("z n  Zero  daily energy counter (a|0:8)\n") ;
					aaPrintf ("d n  Display with history rank n\n") ;
					break ;

				case 'R':
					if (readTotalEnergyCounters ())
					{
						aaPuts ("Ok\n")  ;
					}
					else
					{
						aaPuts ("Error\n")  ;
					}
					break ;

				case 'W':
					writeTotalEnergyCounters () ;
					break ;

				case 'Z':		// Zero  total energy
				case 'z':		// Zero  today energy
					{
						// Example:
						//	e z 0		Reset imported today energy counter
						//	e Z 0		Reset imported total energy counter
						//	e Z a		Reset all the total energy counters
						//	e Z d		Only set Total energy date
						if (pArg2 != NULL  &&  (* pArg2 == 'a' || (* pArg2 >= '0'  &&  * pArg2 < energyCountersCount+'0')))
						{
							uint32_t	* pE ;
							uint32_t	 ix ;

							pE = (uint32_t *) ((* pArg1 == 'Z') ? & energyWh : & dayEnergyWh) ;
							((energyCounters_t *) pE)->date = timeGetDayDate (& localTime) ;
							if (* pArg2 == 'a')
							{
								memset (pE, 0, sizeof (energyCounters_t)) ;
								((energyCounters_t *) pE)->date = timeGetDayDate (& localTime) ;

							}
							else
							{
								ix = arg2 + energyCountersOffset ;	// To skip version and date fields
								pE [ix] = 0 ;
							}
						}
						if (* pArg1 == 'Z'  &&  * pArg2 == 'd')
						{
							// Set Total energy date
							// To use on the 1st start of the diverter
							energyWh.date = timeGetDayDate (& localTime) ;
						}
					}
					break ;

				case 'd':
					{
						// e d		Display energy with history rank 0 (yesterday)
						// e d 2	Display energy with history rank 2

						// Display total energy
						displayEnergy ("Total", & energyWh) ;

						// Display today energy
						displayEnergy ("Today", & dayEnergyWh) ;

						// Display history energy at rank arg2
						if (pArg2 == NULL)
						{
							arg2 = 0 ;	// Default is yesterday
						}
						if (histoCheckRank (arg2, NULL))
						{
							energyCounters_t energy ;

							histoRead (arg2, & energy, NULL) ;
							displayEnergy ("History", & energy) ;
						}
					}
					break ;
			}
		}

		else if (0 == strcmp ("dp", pCmd))		// Display PID trace
		{
			displayWToggle (DPYW_DISPLAY_SPID) ;
			sPidTraceEnable (displayWTest (DPYW_DISPLAY_SPID)) ;
		}

		else if (0 == strcmp ("li", pCmd))		// Toggle Linky frame print
		{
			ticToggleDisplay () ;
		}

		// -------------- SSR management ----------------------

		else if (0 == strcmp ("sd", pCmd))		// Start/Stop global diverting
		{
			// BEWARE: bDiverterSet is a request to TOGGLE the diverting state
			// sd		=> toggle the state
			// sd 1		=> toggle the state if the state is disabled
			// sd 0		=> toggle the state if the state is enabled
			if (pArg1 == NULL)
			{
				bDiverterSet = true ;
			}
			else
			{
				if (arg1 == 0  && statusWTest (STSW_DIV_ENABLED))
				{
					bDiverterSet = true ;
				}
				if (arg1 != 0  && ! statusWTest (STSW_DIV_ENABLED))
				{
					bDiverterSet = true ;
				}
			}
		}

		else if (0 == strcmp ("dd", pCmd))		// Enable/disable diverting RT data display
		{
			// Enable data display every 10ms...
			displayWToggle (DPYW_DISPLAY_DIV_DATA) ;
		}

		else if (0 == strcmp ("Sv", pCmd) || 0 == strcmp ("+", pCmd) || 0 == strcmp ("-", pCmd))	// Set SSR 2 delay timer value (test only)
		{
			if (pArg1 != NULL)
			{
				int32_t		val ;

				if (* pCmd == '+')
				{
					val = TIMSSR->CCR1 + atoi (pArg1) ;	// increment
				}
				else if (* pCmd == '-')
				{
					val = TIMSSR->CCR1 - atoi (pArg1) ;	// decrement
				}
				else
				{
					val = arg1 ;	// Absolute value
				}

				if (val >= 0  &&  val < 512)
				{
					aaPrintf ("SSR delay: %u\n", val) ;
					timerOutputChannelSet (TIMSSR, powerDiv[1].ssrChannel, val) ;
				}
			}
		}

		else if (0 == strcmp ("Si", pCmd))		// Set SSR arg2 delay value (test only)
		{
			if (pArg1 != NULL)
			{
				if (arg1 >= 0  &&  arg1 < 512)
				{
					if (pArg2 == NULL  ||  (arg2 != 1  &&  arg2 != 2))
					{
						arg2 = 2 ;
					}

					aaPrintf ("SSR index: %u, v=%u\n", arg1, p2Delay [arg1]) ;
					timerOutputChannelSet (TIMSSR, powerDiv[arg2-1].ssrChannel, p2Delay [arg1]) ;
				}
			}
		}

		else if (0 == strcmp ("Sp", pCmd))		// Set SSR % power (test only)
		{
			// To use this command the diverter must be stopped: sd 0
			if (pArg1 != NULL)
			{
				if (arg1 >= 0  &&  arg1 <= 100)
				{
					if (pArg2 == NULL  ||  (arg2 != 1  &&  arg2 != 2))
					{
						arg2 = 2 ;
					}

					arg3 = ((511 * arg1) + 50) / 100 ;	// index
					if (arg3 > 511)
					{
						arg3 = 511 ;
					}
					aaPrintf ("SSR %d power: %d, i=%d, v=%u\n", arg2, arg1, arg3, p2Delay [arg3]) ;
					timerOutputChannelSet (TIMSSR, powerDiv[arg2-1].ssrChannel, p2Delay [arg3]) ;
				}
			}
		}

		// -------------- Date / Time ----------------------

		else if (0 == strcmp ("time", pCmd))		// Display/Set time manually
		{
			bool	result = false;

			if (pArg1 == NULL)
			{
				aaPrintf ("Time: %02u:%02u:%02u  %02u/%02u/%02u %s\n",
						localTime.hh, localTime.mm, localTime.ss,
						localTime.yy, localTime.mo, localTime.dd, timeDayName[localTime.wd]) ;
				result = true ;
			}
			else if (pArg1 != NULL  &&  pArg2 != NULL  &&  pArg3 != NULL)
			{
				if (arg1 >= 0  &&  arg1 < 24  &&  arg2 >= 0  &&  arg2 < 60  &&  arg3 >= 0  &&  arg3 < 60)
				{
					uint8_t hh = arg1 ;
					uint8_t mm = arg2 ;
					uint8_t ss = arg3 ;

					// Get date parameters
					pArg1 = strtok_r (NULL, " \t", & savePtr) ;
					if (pArg1 != NULL)
					{
						arg1 = strtol (pArg1, (char **) NULL, 0) ;

						pArg2 = strtok_r (NULL, " \t", & savePtr) ;
						if (pArg2 != NULL)
						{
							arg2 = strtol (pArg2, (char **) NULL, 0) ;

							pArg3 = strtok_r (NULL, " \t", & savePtr) ;
							if (pArg3 != NULL)
							{
								arg3 = strtol (pArg3, (char **) NULL, 0) ;
							}
						}
					}
					if (pArg1 != NULL  &&  pArg2 != NULL  &&  pArg3 != NULL)
					{
						if (arg1 > 2000  &&  arg2 > 0  &&  arg2 < 13  &&  arg3 > 0  &&  arg3 < 32)
						{
							localTime.hh = hh ;
							localTime.mm = mm ;
							localTime.ss = ss ;
							localTime.yy = arg1 ;
							localTime.mo = arg2 ;
							localTime.dd = arg3 ;
							statusWSet (STSW_TIME_OK) ;
							histoStart () ;		// Start/synchronize power history
							result = true ;
						}
					}
				}
			}
			if (result == false)
			{
				aaPuts ("Error\n") ;
			}
		}

		else if (0 == strcmp ("ltime", pCmd))		// Set time from LAN or Linky
		{
			// If arg1 is present also synchronize power history
			timeUpdateRequest (pArg1 == NULL ? 0 : 1) ;
		}

		else if (0 == strcmp ("fman", pCmd))		// Set manual order for the forcing
		{
			if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= FORCE_MAX)
			{
				forceSetManualOrder		(arg1-1) ;
			}
		}

		else if (0 == strcmp ("in", pCmd))		// Dump input I/O values
		{
			// Digital input test
			bool		res ;
			uint32_t	value ;

			(void) res ;
			res = inputGet (IO_IN1, & value) ;
			aaPrintf ("1: %d\n", value) ;

			res = inputGet (IO_IN2, & value) ;
			aaPrintf ("2: %d\n", value) ;

			if (pulseCounter[0].pulsepkWh == 0)
			{
				res = inputGet (IO_PULSE1, & value) ;
				aaPrintf ("3: %d\n", value) ;
			}
			else
			{
				aaPuts ("3: pulse counter input\n") ;
			}

			if (pulseCounter[1].pulsepkWh == 0)
			{
				res = inputGet (IO_PULSE2, & value) ;
				aaPrintf ("4: %d\n", value) ;
			}
			else
			{
				aaPuts ("4: is pulse counter input\n") ;
			}
		}

#if ISOPT_TEMPERATURE
		//-------------- Temperature sensors ----------------------

		else if (0 == strcmp ("tss", pCmd))		// Temperature sensors search
		{
			tsRequest (TSREQUEST_SEARCH) ;
		}

		else if (0 == strcmp ("tsc", pCmd))		// Temperature sensors Check
		{
			tsRequest (TSREQUEST_CHECK) ;
		}

		else if (0 == strcmp ("dts", pCmd))		// Display temperature sensors data
		{
			ds18b20_t	* pTemp ;
			aaPrintf ("ts count: %u\n", pTempSensors->count) ;
			for (ii = 0 ; ii < TEMP_SENSOR_MAX ; ii++)
			{
				pTemp = & pTempSensors->sensors [ii] ;
				aaPrintf (" %u %u %c ", ii, pTemp->rank+1, pTemp->present == 0 ? 'A' : 'P') ;
				aaPrintf ("Type 0x%02x ID %02x%02x%02x%02x%02x%02x CRC 0x%02x ",
						pTemp->id[0], pTemp->id[6], pTemp->id[5], pTemp->id[4], pTemp->id[3], pTemp->id[2], pTemp->id[1], pTemp->id[7]);
				iFracPrint (pTemp->rawTemp, TEMP_SENSOR_SHIFT, 4, 1) ;
				aaPutChar ('\n') ;
			}
		}

		else if (0 == strcmp ("tsr", pCmd))		// Set temperature sensors ranks, rank is 1 based on input
		{
			if (pArg1 != NULL  &&  arg1 > 0  &&  arg1 <= TEMP_SENSOR_MAX)
			{
				pTempSensors->sensors[0].rank = arg1-1 ;
				if (pArg2 != NULL  &&  arg2 > 0  &&  arg2 <= TEMP_SENSOR_MAX)
				{
					pTempSensors->sensors[1].rank = arg2-1 ;
					if (pArg3 != NULL  &&  arg3 > 0  &&  arg3 <= TEMP_SENSOR_MAX)
					{
						pTempSensors->sensors[2].rank = arg3-1 ;	// rank is internally 0 based
					}
					for (ii = 3 ; ii < TEMP_SENSOR_MAX ; ii++)
					{
						pArg3 = strtok_r (NULL, " \t", & savePtr) ;
						if (pArg3 != NULL)
						{
							arg3 = strtol (pArg3, (char **) NULL, 0) ;
							if (arg3 > 0  &&  arg3 <= TEMP_SENSOR_MAX)
							{
								pTempSensors->sensors[ii].rank = arg3-1 ;
							}
						}
						else
						{
							break ;
						}
					}
				}
			}
		}
#endif // ISOPT_TEMPERATURE

		else if (0 == strcmp ("reset", pCmd))	//  Reboot or go to system boot loader
		{
			// reset			=> MCU reset
			// reset boot		=> Go to MCU internal downloader
			if (pArg1 != NULL  &&  0 == strcmp ("boot", pArg1))
			{
				bspJumpToBootLoader () ;
			}
			bspResetHardware () ;
			while (1) ; // Just in case...
		}

		else if (0 == strcmp ("SerEL", pCmd))	//  Switch to flash external loader (not a user command)
		{
			diverterStop () ;
			SerEL () ;
			bspResetHardware () ;
		}

		else if (0 == strcmp ("var", pCmd))		//  Display/Set values of aaSunVariable[]
		{
			// var			Display all the array
			// var n v		Set Vn to v
			if (pArg1 == NULL)
			{
				// Display the array
				for (ii = 0 ; ii < AASUNVAR_MAX ; ii++)
				{
					aaPrintf ("V%d = %d\n", ii+1, aaSunVariable[ii]) ;
				}
			}
			else
			{
				// Set one value
				if (pArg2 != NULL  &&  arg1 > 0  &&  arg1 <= AASUNVAR_MAX)
				{
					aaSunVariable[arg2-1] = arg3 ;
					aaPrintf ("V%d = %d\n", arg2, aaSunVariable[arg2-1]) ;
				}
			}
		}

		// -------------- Test and debug only ----------------------

		else if (0 == strcmp ("h", pCmd))		//  History power dump-today dump-yesterday write
		{
			switch (* pArg1)
			{
				default:
				case '?':
					aaPrintf ("h ?   Print help \n") ;
					aaPrintf ("h v   Toggle debug history display\n") ;
					aaPrintf ("h d   Dump today power history\n") ;
					aaPrintf ("h h n Dump power history at rank n\n") ;
					aaPrintf ("h H   Dump flash history summary\n") ;

					aaPrintf ("h Z   Erase flash history area\n") ;
					aaPrintf ("h W   Write today history to flash\n") ;
					break ;

				case 'v':
					displayWToggle (DPYW_DISPLAY_HISTO) ;
					break ;

				case 'd':
					{
						powerH_t	* pPowerHistory = powerHistory ;
						int			hh, mm ;

						aaPrintf ("%4d/%02d/%02d\n", pHistoHeader->date >> 16,
								(pHistoHeader->date >> 8) & 0xFF,
								 pHistoHeader->date & 0xFF) ;
						pPowerHistory++ ;
						hh = mm = 0 ;
						for (ii = 1 ; ii < POWER_HISTO_MAX_WHEADER ; ii++)
						{
							aaPrintf (" %02d:%02d  %9d %9d %9d %9d"
#if (defined IX_I3)
									" %9d"
#endif
#if (defined IX_I4)
									" %9d"
#endif
									" %9d %9d\n",
								hh, mm,
								pPowerHistory->imported, pPowerHistory->exported,
								pPowerHistory->diverted1, pPowerHistory->power2,
#if (defined IX_I3)
								pPowerHistory->power3,
#endif
#if (defined IX_I4)
								pPowerHistory->power4,
#endif
								pPowerHistory->powerPulse [0], pPowerHistory->powerPulse [0]) ;
							pPowerHistory++ ;
							mm += 15 ;
							if (mm == 60)
							{
								mm  = 0 ;
								hh++ ;
							}
						}
					}
					break ;

				case 'h':
					// Display history at rank arg2, from 0 to HISTO_MAX-2
					// This display is lengthy, so delegate it to a lower priority task
					if (pArg2 != NULL)
					{
						displayYesterdayHisto (0, arg2) ;		// 0 => request
					}
					break ;

				case 'W':	// Write current dayEnergyWh and power history (this add an entry to the history memory)
					histoWrite (& dayEnergyWh, powerHistory) ;	// New history
					break ;

				case 'Z':
					histoFlashErase () ;
					aaPuts ("Total energy and history erased\n") ;
					break ;

				case 'H':		// Dump the rank and date of available history
					{
						energyCounters_t	energy ;
						uint32_t			data [2] ;	// MAGIC and date

						for (ii = 0 ; ii < HISTO_MAX - 1 ;ii++)
						{
							if (histoRead (ii, & energy, NULL))
							{
								// Dump energy date
								aaPrintf ("%2u  E %4d/%02d/%02d  %06u\n",
										ii, energy.date >> 16, (energy.date >> 8) & 0xFF, energy.date & 0xFF,
										energy.energyImported) ;

								// Dump power date
								if (histoPowerRead ((uint8_t *) & data, ii, 0, sizeof (data)))
								{
									aaPrintf ("    P %4d/%02d/%02d\n",
											data [1] >> 16, (data [1] >> 8) & 0xFF, data [1] & 0xFF) ;
								}
								else
								{
									aaPrintf ("Slot %u not available\n", ii) ;
								}
							}
							else
							{
								aaPrintf ("Slot %u not available\n", ii) ;
							}

						}
					}
					break ;
			}
		}

		else if (0 == strcmp ("out", pCmd))		// Set output
		{
			if (pArg1 != NULL  &&  pArg2 != NULL)
			{
				outputSet (arg1 - 1, arg2) ;
			}
		}

		else if (0 == strcmp ("df", pCmd))		// Dump 32 bytes from the FLASH
		{
			uint32_t	address ;
			uint32_t	buffer [8] ;

			address = (pArg1 == NULL) ? 0x100000 : strtoul (pArg1, NULL, 0) ;

			W25Q_SpiTake () ;
			W25Q_Read (buffer, address, sizeof (buffer)) ;
			W25Q_SpiGive () ;
			aaDumpEx ((uint8_t *) buffer, sizeof (buffer), & address) ;
		}

		else if (0 == strcmp ("f", pCmd))		// Flash test
		{
			uint32_t	data ;

			if (pArg2 != NULL)
			{
				switch (* pArg1)
				{
					case 'i':		// Read flash device ID
						W25Q_SpiTake () ;
						data = W25Q_ReadDeviceId () ;
						W25Q_SpiGive () ;
						aaPrintf ("%08x\n", data) ;
						break ;

					case 'z':		// Erase sectors from arg2 to arg3
						if (pArg3 == NULL)
						{
							arg3 = arg2 ;	// Only one sector
						}
						for (ii = 0 ; arg2 <= arg3 ; ii++)
						{
							W25Q_SpiTake () ;
							W25Q_EraseSector (arg2 * W25Q_SECTOR_SIZE) ;
							W25Q_SpiGive () ;
							aaPrintf ("Erased %d\n", arg2) ;
							arg2++ ;
						}
						break ;

					case 'w':	// Write something to sector arg2
						data = 0x12345678 ;
						W25Q_SpiTake () ;
						W25Q_WritePage ((uint8_t *) & data, arg2 * W25Q_SECTOR_SIZE, 4) ;
						W25Q_SpiGive () ;
						break ;

					case 'r':	// Read something from sector arg2
						W25Q_SpiTake () ;
						W25Q_Read ((uint8_t *) & data, arg2 * W25Q_SECTOR_SIZE, 4) ;
						W25Q_SpiGive () ;
						aaPrintf ("R %08X: %08x\n", arg2 * W25Q_SECTOR_SIZE, data) ;
						break ;

					default:
						break ;
				}
			}
		}

		else if (0 == strcmp ("ti", pCmd))		// Display task info
		{
			displaytaskInfo (taskInfo) ;
		}
	}
}

//--------------------------------------------------------------------------------

static void displayEnergy (char * text, energyCounters_t * pE)
{
	aaPrintf ("%s\n   %04u/%02u/%02u\n 0 %-12s %9d\n 1 %-12s %9d\n 2 %-12s %9d\n 3 %-12s %9d\n 4 %-12s %9d\n"
#if (defined IX_I3)
			" 5 %-12s %9d\n"
#endif
#if (defined IX_I4)
			" 6 %-12s %9d\n"
#endif
			" 7 %-12s %9d\n 8 %-12s %9d\n",
		text,
		pE->date >> 16, (pE->date >> 8) & 0xFF, pE->date & 0xFF,
		aaSunCfg.eName [0], pE->energyImported,
		aaSunCfg.eName [1], pE->energyExported,
		aaSunCfg.eName [2], pE->energyDiverted1,
		aaSunCfg.eName [3], pE->energyDiverted2,
		aaSunCfg.eName [4], pE->energy2,
#if (defined IX_I3)
		aaSunCfg.eName [5], pE->energy3,
#endif
#if (defined IX_I4)
		aaSunCfg.eName [6], energyWh.energy4,
#endif
		aaSunCfg.eName [7], (pE->energyPulse [0] * pulseCounter [0].pulseEnergyCoef) >> PULSE_E_SHIFT,
		aaSunCfg.eName [8], (pE->energyPulse [1] * pulseCounter [1].pulseEnergyCoef) >> PULSE_E_SHIFT) ;
}

//--------------------------------------------------------------------------------
