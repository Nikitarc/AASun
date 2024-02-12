/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	eadc.c		ADC and timer configuration

	When		Who	What
	10/10/22	ac	Creation

----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"aakernel.h"
#include	"aalogmes.h"
#include	"aaprintf.h"
#include	"spid.h"		// For ADC timer synchronization to main cycle

#include	<stdlib.h>		// For strtoul()

#include	"gpiobasic.h"
#include	"dmabasic.h"
#include	"rccbasic.h"

#include	"stm32g0xx_ll_adc.h"
#include	"stm32g0xx_ll_bus.h"
#include	"stm32g0xx_ll_rcc.h"
#include	"stm32g0xx_ll_tim.h"

#include	"AASun.h"

//--------------------------------------------------------------------------------

#define	ADC_RESOLUTION		LL_ADC_RESOLUTION_10B
#define	ADC_OVERSAMPLING	1u								// 1: with over sampling
#define	ADC_RATIO			LL_ADC_OVS_RATIO_16				// 2 to 256, power of 2
#define	ADC_SHIFT			LL_ADC_OVS_SHIFT_RIGHT_4		// BEWARE: RATIO dependent
#define	ADC_SAMPLINGTIME	LL_ADC_SAMPLINGTIME_7CYCLES_5

// The STM32 ADC channels numbers, in order of acquisition
static const uint8_t adcChannels [ADC_CHANCOUNT] =
{
	8,		// B0 Volt
	7,		// A7 I1
	0,		// A0 I2
#if (defined IX_I3)
	1,		// A1 I3
#endif
#if (defined IX_I4)
	6,		// A6 I4
#endif
//  4,		// A4 (ADC/DAC) on expansion connector
} ;

// The analog GPIO inputs configuration
static	const gpioPinDesc_t	anaInDesc  [ADC_CHANCOUNT] =
{
	{	'B',	0,	0,	AA_GPIO_MODE_ADC },
	{	'A',	7,	0,	AA_GPIO_MODE_ADC },
	{	'A',	0,	0,	AA_GPIO_MODE_ADC },
#if (defined IX_I3)
	{	'A',	1,	0,	AA_GPIO_MODE_ADC },
#endif
#if (defined IX_I4)
	{	'A',	6,	0,	AA_GPIO_MODE_ADC },
#endif
} ;

static const gpioPinDesc_t	timerSyncOutput	= {	'B',	1,	AA_GPIO_AF_1,	AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL } ;

// Output channels of SSR timer
// Timer channel 2 is used for the on board relay
static	const gpioPinDesc_t	timerSsrOutput1 = {	'A',	8,	AA_GPIO_AF_2,	AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL } ;
static	const gpioPinDesc_t	timerSsrOutput2 = {	'A',	11,	AA_GPIO_AF_2,	AA_GPIO_MODE_ALTERNATE | AA_GPIO_PUSH_PULL } ;

// The task to signal at the end of the ADC DMA
static	aaTaskId_t			meterTaskId ;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Only the STM32G071 have a reference output. For STM32G070 use an external reference.

#if (defined VREFBUF)

#define	VREFBUF_MODE_OFF	0u	//	VREFBUF buffer off:
								//	VREF+ pin pulled-down to VSSA

#define	VREFBUF_MODE_EXT	1u	//	External voltage reference mode (default value):
								//	VREFBUF buffer off
								//	VREF+ pin input mode

#define	VREFBUF_MODE_INT	2u	//	Internal voltage reference mode:
								//	VREFBUF buffer ON
								//	VREF+ pin connected to VREFBUF buffer output

#define	VREFBUF_MODE_HOLD	3u	//	Hold mode:
								//	VREFBUF buffer off
								//	VREF+ pin floating. The voltage is held with the external capacitor
								//	VRR detection disabled and VRR bit keeps last state

#define	VREFBUF_SCALE0		0u	//	VREF_OUT1 : 2.048V
#define	VREFBUF_SCALE1		1u	//	VREF_OUT2 : 2.5V

//----------------------------------------------------------------------
// Set the analog reference buffer trim value

__ALWAYS_STATIC_INLINE void	vrefbufSetTrim (uint32_t trim)
{
	VREFBUF->CCR = trim & VREFBUF_CCR_TRIM ;
}

//----------------------------------------------------------------------
// Get the analog reference buffer trim value

__ALWAYS_STATIC_INLINE uint32_t	vrefbufGetTrim (void)
{
	return VREFBUF->CCR & VREFBUF_CCR_TRIM ;
}

//----------------------------------------------------------------------
//	Configure the analog reference buffer

void	vrefbufConfig (uint32_t vrefbufMode, uint32_t scale)
{
	uint32_t	reg;

	reg = ((vrefbufMode & 1u) << 1u) | ((vrefbufMode & 2u) >> 1u) ;
	reg |= (scale << VREFBUF_CSR_VRS_Pos) & VREFBUF_CSR_VRS ;
	VREFBUF->CSR = reg ;

	if (vrefbufMode == VREFBUF_MODE_INT)
	{
		while ((VREFBUF->CSR & VREFBUF_CSR_VRR) == 0u)
		{
		}
	}
}

#endif	// VREFBUF

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	ADC DMA configuration

// Interrupt handler for DMA1+Stream1
// Manage only Transfer Complete and Error interrupts

void DMA1_Channel1_IRQHandler (void)
{
	dma_t			* pDma = (dma_t *) DMA1 ;
	uint32_t		isr ;
	uint32_t		mask ;

	aaIntEnter () ;

	isr = pDma->ISR ;	// Get interrupt status register only once

	// Check transfer complete
	mask = 1u << DMA_ISR_TCIF1_Pos ;
	if ((isr & mask) != 0u)
	{
		// Clear flag
		pDma->IFCR = mask ;

		// Signal ADC processing task
tst0_1 () ;		// Rising: End of ADC conversion
		if (meterTaskId != 0)
		{
			aaSignalSend (meterTaskId, 1u) ;
		}
	}

	// Check transfer error
	mask = 1 <<  DMA_ISR_TEIF1_Pos ;
	if ((isr & mask) != 0u)
	{
		// Clear flag
		pDma->IFCR = mask ;

		AA_ASSERT (0) ;
//		aaLogMes ("DMA Error\n", 0, 0, 0, 0, 0) ;
	}

	aaIntExit () ;
}

//--------------------------------------------------------------------------------
//	bufferSize: Count of items in pBuffer
//	pBuffer:    Pointer to DMA buffer

void	adcDmaInit (uint32_t bufferSize, uint16_t * pBuffer)
{
	// Configure NVIC to enable DMA interruptions
	NVIC_SetPriority (DMA1_Channel1_IRQn, 1) ;
	NVIC_EnableIRQ   (DMA1_Channel1_IRQn) ;

	LL_AHB1_GRP1_EnableClock (LL_AHB1_GRP1_PERIPH_DMA1) ;

	LL_DMA_ConfigTransfer  (DMA1, LL_DMA_CHANNEL_1,
							LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
							LL_DMA_MODE_CIRCULAR              |
							LL_DMA_PERIPH_NOINCREMENT         |
							LL_DMA_MEMORY_INCREMENT           |
							LL_DMA_PDATAALIGN_HALFWORD        |
							LL_DMA_MDATAALIGN_HALFWORD        |
							LL_DMA_PRIORITY_HIGH) ;

	// Select ADC as DMA transfer request
	LL_DMAMUX_SetRequestID (DMAMUX1, LL_DMAMUX_CHANNEL_0, LL_DMAMUX_REQ_ADC1) ;

	// Set DMA transfer addresses of source and destination
	LL_DMA_ConfigAddresses  (DMA1,
	                         LL_DMA_CHANNEL_1,
							 (uint32_t) & ADC1->DR,		// LL_ADC_DMA_GetRegAddr (ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
	                         (uint32_t) pBuffer,
	                         LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

	// Set DMA transfer size
	LL_DMA_SetDataLength (DMA1, LL_DMA_CHANNEL_1, bufferSize) ;

	// Enable DMA transfer complete interruption
	LL_DMA_EnableIT_TC (DMA1, LL_DMA_CHANNEL_1) ;

	// Enable DMA transfer error interruption
	LL_DMA_EnableIT_TE (DMA1, LL_DMA_CHANNEL_1) ;

	// Enable the DMA transfer
	LL_DMA_EnableChannel (DMA1, LL_DMA_CHANNEL_1) ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Set the ADC to scan a sequence of analog inputs

void	adcInit (void)
{
	uint32_t	ii ;
	uint32_t	reg ;

	// Configure GPIO analog inputs
	for (ii = 0 ; ii < ADC_CHANCOUNT ; ii++)
	{
		gpioConfigurePin (& anaInDesc [ii]) ;
	}

	// Enable ADC clock (core clock)
	LL_APB2_GRP1_EnableClock (LL_APB2_GRP1_PERIPH_ADC) ;
	rccResetPeriph (ADC1, 0) ;

	// Set ADC clock (conversion clock)
	LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);
    LL_ADC_SetClock (ADC1, LL_ADC_CLOCK_SYNC_PCLK_DIV2) ;

	// Set ADC data resolution
	LL_ADC_SetResolution			(ADC1, ADC_RESOLUTION) ;

	// Set ADC conversion data alignment
	LL_ADC_SetDataAlignment			(ADC1, LL_ADC_DATA_ALIGN_RIGHT) ;

	// Set ADC low power mode
	LL_ADC_SetLowPowerMode			(ADC1, LL_ADC_LP_MODE_NONE) ;

	// Set the sampling time value common to all channels
    LL_ADC_SetSamplingTimeCommonChannels (ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, ADC_SAMPLINGTIME) ;

	// Set ADC continuous mode
	LL_ADC_REG_SetContinuousMode	(ADC1, LL_ADC_REG_CONV_SINGLE) ;

    // Set ADC overrun behavior
	LL_ADC_REG_SetOverrun			(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN) ;


	// Configure the sequencer ----------------------------------------------------
	// Use the configurable sequencer mode, so we can choose the channel acquisition order

	// Beware CCRDY is set only if CHSELRMOD value is changed
	LL_ADC_ClearFlag_CCRDY (ADC1) ;
	if (LL_ADC_REG_GetSequencerConfigurable (ADC1) != LL_ADC_REG_SEQ_CONFIGURABLE)
	{
		LL_ADC_REG_SetSequencerConfigurable (ADC1, LL_ADC_REG_SEQ_CONFIGURABLE) ;
		while (LL_ADC_IsActiveFlag_CCRDY (ADC1) == 0)
		{
		}
		LL_ADC_ClearFlag_CCRDY (ADC1) ;
	}

	// Configure the sequencer and over sampling
	LL_ADC_REG_SetSequencerDiscont		(ADC1, LL_ADC_REG_SEQ_DISCONT_DISABLE) ;
	if (ADC_OVERSAMPLING != 0u)
	{
		LL_ADC_SetOverSamplingScope			(ADC1, ADC_CFGR2_OVSE) ;
		LL_ADC_ConfigOverSamplingRatioShift	(ADC1, ADC_RATIO, ADC_SHIFT) ;
		LL_ADC_SetOverSamplingDiscont		(ADC1, LL_ADC_OVS_REG_CONT) ;
	}

	// Configure the channel sequence
	LL_ADC_ClearFlag_CCRDY (ADC1) ;	// Use CHSELR so test CCRDY
	reg = 0u ;
	for (ii = 0 ; ii < ADC_CHANCOUNT ; ii++)
	{
		reg |= adcChannels [ii] << (ii * 4) ;

		// Set ADC channels sampling time (default: LL_ADC_SAMPLINGTIME_COMMON_1)
//		ADC1->SMPR |= 1u << (adcChannels [ii] + ADC_SMPR_SMPSEL_Pos) ;

	}
	if (ADC_CHANCOUNT < 8u)
	{
		reg |= 0xFu << (ADC_CHANCOUNT * 4) ;		// To stop the sequence
	}
	ADC1->CHSELR = reg ;
	while (LL_ADC_IsActiveFlag_CCRDY (ADC1) == 0)
	{
	}
	LL_ADC_ClearFlag_CCRDY (ADC1) ;


	// Configure the trigger ----------------------------------------------------

	// Auto rearm on each trigger (trigger period > 100us)
	LL_ADC_SetTriggerFrequencyMode (ADC1, LL_ADC_CLOCK_FREQ_MODE_LOW) ;

	// Set ADC trigger source
	LL_ADC_REG_SetTriggerSource (ADC1, LL_ADC_REG_TRIG_EXT_TIM3_TRGO) ;

	// Set ADC trigger polarity (unused for SW trigger)
	LL_ADC_REG_SetTriggerEdge	(ADC1, LL_ADC_REG_TRIG_EXT_RISING) ;

	// Set internal ADC reference to 2.5V
	// STM32G071 only. For STM32G070 use an external reference
#if (defined VREFBUF)
	vrefbufConfig (VREFBUF_MODE_INT, VREFBUF_SCALE1) ;
#endif

	LL_ADC_EnableInternalRegulator (ADC1) ;
	bspDelayUs (LL_ADC_DELAY_INTERNAL_REGUL_STAB_US + 1u) ;
}

//--------------------------------------------------------------------------------

void	adcStart (void)
{
	// ADC Calibration is mandatory before start
	LL_ADC_StartCalibration (ADC1) ;
	while (LL_ADC_IsCalibrationOnGoing (ADC1) == 1u)
	{
	}

	// Delay after calibration (2 fADC clock)
	bspDelayUs (LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES) ;		// Enough is fADC >= 1 MHz

	// Configure ADC DMA after calibration
	LL_ADC_REG_SetDMATransfer (ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED) ;

	// Enable ADC
	LL_ADC_Enable (ADC1) ;
	while (LL_ADC_IsActiveFlag_ADRDY (ADC1) == 0)
	{
	}
	LL_ADC_ClearFlag_ADRDY (ADC1) ;

	// ADC is now ready to start conversion

	// This enable the conversion but doesn't start conversion (because trigger source is external)
	// The conversion will be started by the TIMSYNC timer update
	LL_ADC_REG_StartConversion (ADC1) ;
}

//--------------------------------------------------------------------------------
//	Set the output compare value of a timer channel

void	timerOutputChannelSet (TIM_TypeDef * pTim, uint32_t channel, uint32_t ocValue)
{
	if (channel == LL_TIM_CHANNEL_CH1)
	{
		LL_TIM_OC_SetCompareCH1	(pTim, ocValue) ;
	}
	else if (channel == LL_TIM_CHANNEL_CH2)
	{
		LL_TIM_OC_SetCompareCH2	(pTim, ocValue) ;
	}
	else if (channel == LL_TIM_CHANNEL_CH3)
	{
		LL_TIM_OC_SetCompareCH3	(pTim, ocValue) ;
	}
	else if (channel == LL_TIM_CHANNEL_CH4)
	{
		LL_TIM_OC_SetCompareCH4	(pTim, ocValue) ;
	}
	else
	{
		AA_ASSERT (0) ;		// Other channels not managed
	}
}

//--------------------------------------------------------------------------------
//	This timer starts ADC conversion

void	adcTimerInit (void)
{
	LL_TIM_InitTypeDef	TIM_InitStruct = {0} ;
	uint32_t			timFreq ;		// Timer clock frequency in MHz

	rccEnableTimClock (TIMSYNC) ;
	rccResetTim       (TIMSYNC) ;

	timFreq = rccGetTimerClockFrequency (TIMSYNC) / 1000000u ;

	TIM_InitStruct.Prescaler         = (timFreq / TIMSYNC_CLK_MHZ) - 1u ;
	TIM_InitStruct.CounterMode       = LL_TIM_COUNTERMODE_UP ;
	TIM_InitStruct.Autoreload        = (TIMSYNC_PER_US * TIMSYNC_CLK_MHZ) - 1u ;
	TIM_InitStruct.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1 ;
	TIM_InitStruct.RepetitionCounter = 0 ;
	LL_TIM_Init (TIMSYNC, & TIM_InitStruct) ;

//	LL_TIM_EnableARRPreload			(TIMSYNC) ;		// By default ARPE is 0
	LL_TIM_SetClockSource			(TIMSYNC, LL_TIM_CLOCKSOURCE_INTERNAL) ;
	LL_TIM_SetTriggerOutput			(TIMSYNC, LL_TIM_TRGO_UPDATE) ;			// To start the one pulse SSR timer on UPDATE event

	// Configure the output channel (for test only)
	gpioConfigurePin (& timerSyncOutput) ;
	LL_TIM_OC_SetMode (TIMSYNC, TIMSYNC_CHAN, LL_TIM_OCMODE_PWM1) ;

	// Set an output pulse to allow an oscilloscope inspection, 50 us
	timerOutputChannelSet (TIMSYNC, TIMSYNC_CHAN, 50u * TIMSYNC_CLK_MHZ) ;

	LL_TIM_CC_EnableChannel	(TIMSYNC, TIMSYNC_CHAN) ;

	// The ARPE bit is not set, so the timer shadow registers are already initialized, no need to set the UG bit
	// The timer is ready for start
/*
	// Start the counter
	// Before TIM start we must generate UG event to initialize the registers
	// BUT this send an TRGO output and this triggers an ADC acquisition.
	// So the ADC start conversion must be set after LL_TIM_GenerateEvent_UPDATE(), and before LL_TIM_EnableCounter()

	LL_TIM_GenerateEvent_UPDATE (TIMSYNC) ;	// Force update generation: Clear CNT and prescaler
	TIMSYNC->SR = 0u ;						// Clear all flags, because LL_TIM_GenerateEvent_UPDATE sets UIF
*/
}

//--------------------------------------------------------------------------------

void	adcTimerStart (void)
{
//	LL_TIM_EnableCounter (TIMSSR) ;
	LL_TIM_EnableCounter (TIMSYNC) ;
}

//--------------------------------------------------------------------------------

void	adcStop		(void)
{
	LL_TIM_DisableCounter (TIMSSR) ;
	LL_TIM_DisableCounter (TIMSYNC) ;
	LL_DMA_DisableChannel (DMA1, LL_DMA_CHANNEL_1) ;
	meterTaskId = 0 ;
}

//--------------------------------------------------------------------------------

void		adcSetTaskId		(aaTaskId_t taskId)
{
	meterTaskId = taskId ;
}

//--------------------------------------------------------------------------------
//	This timer generate the SSR command pulses

void	ssrTimerInit	(void)
{
	LL_TIM_InitTypeDef	TIM_InitStruct = {0} ;
	uint32_t			timFreq ;		// Timer clock frequency in MHz

	rccEnableTimClock (TIMSSR) ;
	rccResetTim       (TIMSSR) ;

	timFreq = rccGetTimerClockFrequency (TIMSSR) ;

	// The SSR timer period is set to TIMSYNC_PER_US/2 us less than the main period

	TIM_InitStruct.Prescaler         = (timFreq / TIMSSR_CLK_HZ) - 1u ;
	TIM_InitStruct.CounterMode       = LL_TIM_COUNTERMODE_UP ;
	TIM_InitStruct.Autoreload        = TIMSSR_ARR ;
	TIM_InitStruct.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1 ;
	TIM_InitStruct.RepetitionCounter = 0 ;
	LL_TIM_Init (TIMSSR, & TIM_InitStruct) ;

//	LL_TIM_EnableARRPreload			(TIMSSR) ;		// By default ARPE is 0
	LL_TIM_SetClockSource			(TIMSSR, LL_TIM_CLOCKSOURCE_INTERNAL) ;
	LL_TIM_SetOnePulseMode			(TIMSSR, LL_TIM_ONEPULSEMODE_SINGLE) ;		// One pulse mode
	LL_TIM_SetTriggerInput			(TIMSSR, LL_TIM_TS_ITR2) ;					// ITR2 is connected to TIM3 TRGO
	LL_TIM_EnableAllOutputs			(TIMSSR) ;					// Set MOE, mandatory for timers with BREAK capability

	// Do not set the timer in trigger slave mode now. This will start the timer at random place.
	// The SSR timer will be started (with ssrTimerEnable()) when the synchro timer is synchronized to the main cycle
	// at the end of the synchronization function
//	LL_TIM_SetSlaveMode				(TIMSSR, LL_TIM_SLAVEMODE_TRIGGER) ;

	// Configure the output channels
	gpioConfigurePin (& timerSsrOutput1) ;
	gpioConfigurePin (& timerSsrOutput2) ;

	LL_TIM_OC_SetMode (TIMSSR, TIMSSR_CHAN1, LL_TIM_OCMODE_PWM2) ;	// Use PWM2 so the delay is at level 0, then the pulse at level 1
	LL_TIM_OC_SetMode (TIMSSR, TIMSSR_CHAN2, LL_TIM_OCMODE_PWM2) ;

	// Set a delay >= ARR to not generate a pulse
	timerOutputChannelSet (TIMSSR, TIMSSR_CHAN1, TIMSSR->ARR) ;
	timerOutputChannelSet (TIMSSR, TIMSSR_CHAN2, TIMSSR->ARR) ;

	LL_TIM_CC_EnableChannel	(TIMSSR, TIMSSR_CHAN1) ;
	LL_TIM_CC_EnableChannel	(TIMSSR, TIMSSR_CHAN2) ;

	// The ARPE bit is not set, so the timer shadow registers are already initialized, no need to set the UG bit
	// The timer is ready to start
/*
	// Start the counter
	// Before TIM start we must generate UG event to initialize the registers
	// BUT this send an TRGO output and this triggers an ADC acquisition.
	// So the ADC start conversion must be set after LL_TIM_GenerateEvent_UPDATE(), and before LL_TIM_EnableCounter()


	LL_TIM_GenerateEvent_UPDATE (TIMSSR) ;	// Force update generation: Clear CNT and prescaler
	TIMSSR->SR = 0u ;						// Clear all flags, because LL_TIM_GenerateEvent_UPDATE sets UIF
*/
}

//--------------------------------------------------------------------------------
// Set the SSR timer in slave mode triggered by a master timer:
// The timer will start at the next update of the master
// The timer is in One Pulse Mode: it will stop at its next update event, waiting for another trigger

// TIMSSR is no longer used in slave mode: too many desynchronized triggers

void	ssrTimerEnable	(void)
{
	LL_TIM_SetSlaveMode	 (TIMSSR, LL_TIM_SLAVEMODE_TRIGGER) ;
}

//--------------------------------------------------------------------------------
