/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	cfgParameters.c		- Configuration parameters save/read
						  These parameters can be modified by the user
						- Energy counter save/read
						- Power history
						- Use Winbond W25Qxx FLASH

	When		Who	What
	08/02/23	ac	Creation
	16/07/23	ac	Add power history

----------------------------------------------------------------------
*/

#include	"AASun.h"
#include	"w25q.h"
#include	"string.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	The default configuration to use when the FLASH configuration isn't available
//	The default values comes from cfgParameters.h

static const configParameters_t cfgDefault =
{
	CFGPARAM_VERSION,

	PHASE_CAL_DEFAULT,
	VOLT_CAL,

	{
		{ I1_CAL, I1_OFFSET, POWER1_CAL, POWER1_OFFSET, },
		{ I2_CAL, I2_OFFSET, POWER2_CAL, POWER2_OFFSET, },
		{ I3_CAL, I3_OFFSET, POWER3_CAL, POWER3_OFFSET, },
		{ I4_CAL, I4_OFFSET, POWER4_CAL, POWER4_OFFSET, },
	},

	{ PULSE_CAL, PULSE_CAL } ,

	POWER_MARGIN,			// Diverter 1
	POWER_DIVERTER1_MAX,
	POWER_MARGIN,			// Diverter 2
	POWER_DIVERTER2_MAX,

	SPID_P_FACTOR,
	SPID_I_FACTOR,
	PPID_P_FACTOR,
	PPID_I_FACTOR,

	{
		.ip   = { 192, 168,   1, 130 },		// IP address
		.sn   = { 255, 255, 255,   0 },		// Subnet mask
		.gw   = { 192, 168,   1, 254 },		// Gateway address
		.dns  = {   1,   1,   1,   1 },		// Domain Name Server
		.mac  = { 0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
		.dhcp = 1				// 1=NETINFO_STATIC or 2=NETINFO_DHCP
	},

	{ 0 },						// Temperature sensors
	{ "Imported", "Exported", "Div1(est)", "Div2(est)", "CT2", "CT3", "CT4", "Cnt1", "Cnt2" },
	0,							// favorite display page
	{							// diverterRule
		{ 0 },					// Diverter 1 is OFF
		{ 3, 0, {{ 0 }} },		// Diverter 2 is ON and VALID
	},
	{{ 0 }},					// forceRules (all OFF)
	0, 0,						// Anti-legionella

	0, 0, { 0 },				// Reserved
	0							// ckSum
} ;

//--------------------------------------------------------------------------------
//	Flash topology: where to write items  (flash sector size: W25Q_SECTOR_SIZE = 4096)

// The configuration offset in FLASH must not cross a sector boundary
#define	FLASH_CFG_ADDR			(0u * W25Q_SECTOR_SIZE)						// Offset off the configuration in FLASH
#define	FLASH_CFG_SECTOR		(FLASH_CFG_ADDR & ~(W25Q_SECTOR_SIZE-1))	// The offset of the sector which contain the cfg

// Total energy counters
#define	FLASH_ENERGY_ADDR		(1u * W25Q_SECTOR_SIZE)						// Offset off the energy total in FLASH
#define	FLASH_ENERGY_SECTOR		(FLASH_ENERGY_ADDR & ~(W25Q_SECTOR_SIZE-1))	// The offset of the sector which contain the total energy counters
#define	FLASH_ENERGY_SLOTSIZE	128u								//
#define	FLASH_ENERGY_SLOTCOUNT	(W25Q_SECTOR_SIZE / FLASH_ENERGY_SLOTSIZE)

// Energy/power history
#define	FLASH_HISTO_ADDR		(8u * W25Q_SECTOR_SIZE)						// Offset off the history data in FLASH
#define	FLASH_HISTO_SECTOR		(FLASH_HISTO_ADDR & ~(W25Q_SECTOR_SIZE-1))	// The offset of the sector which contain the history data

//--------------------------------------------------------------------------------

configParameters_t aaSunCfg ;

static	int32_t		energyNextWriteIx ; 	// The next index to write total energy counters
static	uint32_t	histoNextWriteIx ; 		// The next sector index to write history data

//--------------------------------------------------------------------------------
// Read configuration from flash

static bool	readCfg_ (void)
{
	uint32_t			* pUint ;
	uint32_t			cks, ii ;

	W25Q_Read (& aaSunCfg, FLASH_CFG_ADDR, sizeof (aaSunCfg)) ;

	// Compute the checksum of the cfg
	pUint = (uint32_t *) & aaSunCfg ;
	cks = 0 ;
	for (ii = 0 ; ii < sizeof (aaSunCfg) / 4 ; ii++)
	{
		cks += * pUint++ ;
	}

	if (cks != 0u  ||  aaSunCfg.version != CFGPARAM_VERSION)
	{
		// Cfg in FLASH is invalid
		return false ;
	}
	return true ;
}

//--------------------------------------------------------------------------------
// Dispatch the configuration to working variables
// Distribute the configuration in the working variables.
//	idx:     diverter index, 0 or 1
//	power:   max diverting power
//	voltage: voltage for this power

void	divSetPmax (uint32_t idx, int32_t power, int32_t voltage)
{
	powerDiv_t	* pDiv = & powerDiv [idx] ;

	if (voltage != 230)
	{
		power = (230 * 230 * power) / (voltage * voltage) ;
	}
	pDiv->powerDiverter230  = power ;
	pDiv->powerDiverterMax  = power << POWER_DIVERTER_SHIFT ;
	pDiv->powerDiverterRref = (((230 * 230) << POWER_DIVRES_SHIFT) + (POWER_DIVRES_SHIFT / 2)) / power ;
	pDiv->powerDiverterOpen = (power * POWER_DIVERTER_OPENTHR / 100)  << POWER_DIVERTER_SHIFT ;
}

static void	applyCfg_ (void)
{
	pulseCounter_t	* pPulse ;

	phaseCal         = aaSunCfg.phaseCal ;
	voltCal          = aaSunCfg.voltCal ;

	for (uint32_t ii = 0 ; ii < PULSE_COUNTER_MAX ; ii++)
	{
		pPulse = & pulseCounter [ii] ;
		pPulse->pulsepkWh       = aaSunCfg.pulseCal [ii] ;
		pPulse->pulseEnergyCoef = (1000 << PULSE_E_SHIFT) / pPulse->pulsepkWh ;
		pPulse->pulsePowerCoef  = ((3600000 << PULSE_P_SHIFT) / PULSE_P_PERIOD) / pPulse->pulsepkWh ;
		pPulse->pulseMaxCount   = (PULSE_E_MAX / 1000) * pPulse->pulsepkWh ;
	}

	divSetPmax (0, aaSunCfg.powerDiverter1_230, 230) ;
	powerDiv[0].powerMargin      = aaSunCfg.powerMargin1 ;
	powerDiv[0].ssrChannel       = TIMSSR_CHAN1 ;

	divSetPmax (1, aaSunCfg.powerDiverter2_230, 230) ;
	powerDiv[1].powerMargin      = aaSunCfg.powerMargin2 ;
	powerDiv[1].ssrChannel       = TIMSSR_CHAN2 ;

	syncPropFactor   = aaSunCfg.syncPropFactor ;
	syncIntFactor    = aaSunCfg.syncIntFactor ;

	powerPropFactor  = aaSunCfg.powerPropFactor ;
	powerIntFactor   = aaSunCfg.powerIntFactor ;

	wizSetCfg (& aaSunCfg.lanCfg) ;			// W5500 LAN module configuration
	pTempSensors = & aaSunCfg.tempsensors;	// Temperature sensors

	forceInit () ;		// Initialize diverting and forcing
}

//--------------------------------------------------------------------------------
//	Read configuration from FLASH if available, else from default, then apply

void	readCfg (void)
{
	// Reset the diverting and forcing (put all the I/O off)
	if (statusWTest (STSW_DIV_ENABLED))
	{
		diverterSwitch = DIV_SWITCH_NONE ;
		while (diverterSwitch != DIV_SWITCH_IDLE)
		{
			aaTaskDelay (1) ;
		}
	}
	forceRuleRemoveAll () ;

	W25Q_SpiTake () ;
	if (readCfg_ () == 0u)
	{
		// Cfg in FLASH is invalid, use default values
		aaSunCfg = cfgDefault ;
		statusWSet (STSW_NOT_FLASHCFG) ;
		aaPuts ("Default configuration\n") ;
	}
	else
	{
		statusWClear (STSW_NOT_FLASHCFG) ;
	}
	W25Q_SpiGive () ;

	applyCfg_ () ;
}

//--------------------------------------------------------------------------------
//	Write current configuration to FLASH

bool	writeCfg (void)
{
	uint32_t			* pUint ;
	uint32_t			ii, cks ;
	bool				bOk ;

	// Populate the structure
	aaSunCfg.phaseCal         = phaseCal ;
	aaSunCfg.voltCal          = voltCal ;

	for (ii = 0 ; ii < PULSE_COUNTER_MAX ; ii++)
	{
		aaSunCfg.pulseCal [ii] = pulseCounter [ii].pulsepkWh ;
	}

	aaSunCfg.powerMargin1       = powerDiv[0].powerMargin ;
	aaSunCfg.powerDiverter1_230 = powerDiv[0].powerDiverter230 ;

	aaSunCfg.powerMargin2       = powerDiv[1].powerMargin ;
	aaSunCfg.powerDiverter2_230 = powerDiv[1].powerDiverter230 ;

	aaSunCfg.syncPropFactor   = syncPropFactor ;
	aaSunCfg.syncIntFactor    = syncIntFactor ;

	aaSunCfg.powerPropFactor  = powerPropFactor ;
	aaSunCfg.powerIntFactor   = powerIntFactor ;

	aaSunCfg.version = CFGPARAM_VERSION ;
	aaSunCfg.ckSum = 0 ;

	// Compute the check sum
	cks = 0 ;
	pUint = (uint32_t *) & aaSunCfg ;
	for (ii = 0 ; ii < sizeof (aaSunCfg) / 4 ; ii++)
	{
		cks += * pUint++ ;
	}
	aaSunCfg.ckSum = 0u - cks ;

	// Write the structure
	W25Q_SpiTake () ;
	W25Q_EraseSector (FLASH_CFG_SECTOR) ;
	W25Q_Write (& aaSunCfg, FLASH_CFG_ADDR, sizeof (aaSunCfg)) ;
	W25Q_SpiGive () ;

	// Read the configuration to verify the write
// TODO Do not read to this place (overrides the real cfg)
	bOk = readCfg_ () ;
	if (bOk)
	{
		statusWClear (STSW_NOT_FLASHCFG) ;	// The configuration in the flash is OK
	}
	else
	{
		statusWSet (STSW_NOT_FLASHCFG) ;	// Bad configuration in the flash
	}
	return bOk ;
}

//--------------------------------------------------------------------------------

void		defaultCfg (void)
{
	aaSunCfg = cfgDefault ;
	applyCfg_ () ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Total energy counters read/save in flash

//	We use 1 FLASH sector for the energy structures. In the sector we have 32 slots and write 1 struct per slot
//	When all the slots are written the sector is erased.
//	This is a kind of simple wear leveling:
//	The Flash allows 100000 erase/write cycles.
//	If we write 1 struct every half hour, the life of the Flash is:
//	(100000 * 32) / (24 * 2) = 66666 days, or 182 years

//--------------------------------------------------------------------------------
//	Find the slot index in the FLASH sector of the last written energy struct
//	Returns true  if found, then *pIx contain the slot index of the energy struct
//	Returns false if not found

static	bool energyFindLast (uint32_t * pIx)
{
	uint32_t	ii ;
	uint32_t	data[2] ;	// Date + version
	uint32_t	addr ;
	uint32_t	last;

	// Find the first virgin slot
	// When the flash is erased the read value if FFFFFFFF
	addr = FLASH_ENERGY_ADDR ;
	last = 0 ;
	for (ii = 0 ; ii < FLASH_ENERGY_SLOTCOUNT ; ii++)
	{
		W25Q_Read (& data, addr, sizeof (data)) ;
		if (data [0] == 0xFFFFFFFF)
		{
			break ;
		}
		last = data [1] ;
		addr += FLASH_ENERGY_SLOTSIZE ;
	}
	if (ii == 0)
	{
		// Not found: the sector is empty (freshly erased)
		energyNextWriteIx = 0 ;
		return false ;
	}
	energyNextWriteIx = ii ;
	* pIx = ii - 1 ;
	if (last == ENERGYCNT_VERSION)
	{
		return true ;	// Found
	}
	return false ; 		// Not found: bad version
}

//--------------------------------------------------------------------------------
//	Read total energy counters from flash

bool	readTotalEnergyCounters	(void)
{
	uint32_t	ix ;
	bool		res ;

	W25Q_SpiTake () ;

	// Find the last written energy struct
	res = energyFindLast (& ix) ;
	if (res)
	{
		ix = FLASH_ENERGY_ADDR + (ix * FLASH_ENERGY_SLOTSIZE) ;
		W25Q_Read (& energyWh, ix, sizeof (energyWh)) ;
	}
	else
	{
		// Not found or bad version, clear the energy structure
		memset (& energyWh, 0, sizeof (energyJ)) ;
		energyWh.date = timeGetDayDate (& localTime) ;
	}
	W25Q_SpiGive () ;
	return res ;
}

//--------------------------------------------------------------------------------
//	Write the total energy counters in the next slot of the sector

void	writeTotalEnergyCounters	(void)
{
	uint32_t	addr ;

	W25Q_SpiTake () ;

	if (energyNextWriteIx == FLASH_ENERGY_SLOTCOUNT)
	{
		// The sector is full: erase then write
		W25Q_EraseSector (FLASH_ENERGY_SECTOR) ;
		energyNextWriteIx = 0 ;
	}

	energyWh.version = ENERGYCNT_VERSION ;
	addr = FLASH_ENERGY_ADDR + (energyNextWriteIx * FLASH_ENERGY_SLOTSIZE) ;
	W25Q_Write (& energyWh, addr, sizeof (energyWh)) ;
	energyNextWriteIx++ ;
	W25Q_SpiGive () ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Energy and power history on flash
//	The data size to record is slightly less than 4kB, so we use 1 FLASH sector of 4kB per record
//--------------------------------------------------------------------------------

static	void	powerHistoReset (void)
{
	powerHHeader_t		* pHeader = pHistoHeader ;
						// The header is the 1st element of the powerHistory array

	memset (powerHistory, 0, sizeof (powerHistory)) ;
	memset (& powerHistoryTemp, 0, sizeof (powerHistoryTemp)) ;
	powerHistoIx = 1 ;	// skip the header

	// Fill the header ;
	pHeader->magic = POWER_HISTO_MAGIC ;

	// In the header a non 0 value indicate this power value is available
	pHeader->power2 = 1 ;
	#if (defined IX_I3)
		pHeader->power3 = 1 ;
	#endif
	#if (defined IX_I4)
		pHeader->power4 = 1 ;
	#endif
	if (pulseCounter [0].pulsepkWh != 0)
	{
		pHeader->powerPulse[0] = 1 ;	// This pulse counter is in use
	}
	if (pulseCounter [1].pulsepkWh != 0)
	{
		pHeader->powerPulse[1] = 1 ;	// This pulse counter is in use
	}
}

//--------------------------------------------------------------------------------
// If power history is not running:
// then start power history
// else synchronize power history to updated date/time

void	histoStart (void)
{
	uint32_t	count ;
	uint32_t	ix, ss ;
	bool		bStarted ;

	bStarted = statusWTest (STSW_PWR_HISTO_ON) ;	// true if the power history is already started

	// Compute how many seconds to the end of the period: POWER_HISTO_PERIOD - ((mm*60+ss) % POWER_HISTO_PERIOD)
	ss = (localTime.mm * 60) + localTime.ss ;
	while (ss >= POWER_HISTO_PERIOD)
	{
		ss -= POWER_HISTO_PERIOD ;
	}
	ss = POWER_HISTO_PERIOD - ss ;

	// Start the software timer
	timerStart (TIMER_HISTO_IX, ss) ;					// For the 1st period
	timerNext  (TIMER_HISTO_IX, POWER_HISTO_PERIOD) ;	// For following ones
aaPrintf ("powerH %02d:%02d:%02d ss:%u ", localTime.hh, localTime.mm, localTime.ss, ss) ;

	// Compute the index in powerHistory array for this period
	count = localTime.mm * 60 ;
	ix = 0 ;
	while (count >= POWER_HISTO_PERIOD)
	{
		count -= POWER_HISTO_PERIOD ;
		ix++ ;
	}
	ix += (localTime.hh * (3600 / POWER_HISTO_PERIOD)) ;
if (ix >= POWER_HISTO_MAX)
{
	aaPuts ("powerHisto error\n") ;
	aaTaskDelay (50) ;
	AA_ASSERT (0) ;
}
	ix++ ;	// To skip the header (with the date)

	if (bStarted  &&  ix < powerHistoIx)
	{
		// Jump back, clear the data ((daylight saving: 1 hour history lost)
		memset (& powerHistory [ix], 0, ((powerHistoIx - ix) + 1) * sizeof (powerHistory)) ;
	}
	powerHistoIx  = ix ;
	memset (& powerHistoryTemp, 0, sizeof (powerHistoryTemp)) ;
aaPrintf ("ix:%u\n", ix) ;

	// Set the date in the header
	pHistoHeader->date = timeGetDayDate (& localTime) ;
	dayEnergyWh.date = pHistoHeader->date ;

	statusWSet (STSW_PWR_HISTO_ON) ;
}

//--------------------------------------------------------------------------------
// Calculate the average power for the elapsed period.
// If it is the last period of the day writes the history (daily power and energy)y in the flash.

void	histoNext (void)
{
	uint32_t	ii ;
	uint32_t	powerCoef ;
	powerH_t	* pPowerHistory = & powerHistory [powerHistoIx] ;

	if (statusWTest (STSW_TIME_OK))
	{
		// Convert the power of this history period in Watts to build the history record
		pPowerHistory->imported  = ((powerHistoryTemp.imported  / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		pPowerHistory->exported  = ((powerHistoryTemp.exported  / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		pPowerHistory->diverted1 = ((powerHistoryTemp.diverted1 / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		pPowerHistory->diverted2 = ((powerHistoryTemp.diverted2 / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		pPowerHistory->power2    = ((powerHistoryTemp.power2    / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;

		#if (defined IX_I3)
			pPowerHistory->power3   = ((powerHistoryTemp.power3   / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		#endif
		#if (defined IX_I4)
			pPowerHistory->power4   = ((powerHistoryTemp.power4   / POWER_HISTO_PERIOD) + POWER_HISTO_ROUND) >> POWER_HISTO_SHIFT ;
		#endif

		// Convert pulse count to Watts
		for (ii = 0 ; ii < PULSE_COUNTER_MAX ; ii++)
		{
			// pulsePowerCoef is computed for PULSE_P_PERIOD but here we use POWER_HISTO_PERIOD => compute new power coef
			powerCoef = (pulseCounter [ii].pulsePowerCoef * PULSE_P_PERIOD) / POWER_HISTO_PERIOD ;
			pPowerHistory->powerPulse [ii] = (powerHistoryTemp.powerPulse [ii] * powerCoef) >> PULSE_P_SHIFT ;
		}

		if (displayWTest (DPYW_DISPLAY_HISTO))		// For debug
		{
			aaPrintf ("powerH %02d:%02d:%02d %2u %9d %9d %9d %9d %9d"
			#if (defined IX_I3)
					" %9d"
			#endif
			#if (defined IX_I4)
					" %9d"
			#endif
					"%9d %9d\n",
					localTime.hh, localTime.mm, localTime.ss,
					powerHistoIx,
					pPowerHistory->imported, pPowerHistory->exported,
					pPowerHistory->diverted1, pPowerHistory->diverted2, pPowerHistory->power2,
			#if (defined IX_I3)
					pPowerHistory->power3,
			#endif
			#if (defined IX_I4)
					pPowerHistory->power4,
			#endif
					pPowerHistory->powerPulse [0], pPowerHistory->powerPulse [1]) ;
		}

		// Next history record, if it is the last in the day, then write to flash
		powerHistoIx ++ ;
		if (powerHistoIx == POWER_HISTO_MAX_WHEADER)
		{
			// Write daily power and energy counters then initialize for the next day
			histoWrite (& dayEnergyWh, powerHistory) ;

			// Reset energy daily counters
			memset (& dayEnergyWh, 0, sizeof (dayEnergyWh)) ;
			dayEnergyWh.date = timeGetDayDate (& localTime) ;

			// Reset daily power counters
			powerHistoReset () ;
			pHistoHeader->date = dayEnergyWh.date ; // Set the date in the power header
		}
		else
		{
			memset (& powerHistoryTemp, 0, sizeof (powerHistoryTemp)) ;
		}
	}
}

//--------------------------------------------------------------------------------
//	Find the index in the FLASH of the 1st free sector
//	Returns true  if found, then *pIx contain the sector index
//	Returns false if not found

static	bool histoFindNext (uint32_t * pIx)
{
	uint32_t	ii ;
	uint32_t	data ;
	uint32_t	addr ;

	// Find the first virgin slot
	// When the flash is erased the read value if FFFFFFFF
	addr = FLASH_HISTO_ADDR ;
	for (ii = 0 ; ii < HISTO_MAX ; ii++)
	{
		W25Q_Read (& data, addr, sizeof (data)) ;
		if (data == 0xFFFFFFFF)
		{
			break ;
		}
		addr += W25Q_SECTOR_SIZE ;
	}
	* pIx = ii ;
	return ii != HISTO_MAX ;
}

//--------------------------------------------------------------------------------
//	Check if data is available at this history rank
//	if the rank is OK, on return *pAddrs contains the data address
//	pAddrs may be NULL

bool	histoCheckRank (uint32_t rank, uint32_t * pAddrs)
{
	uint32_t	ix ;
	uint32_t	addrs ;
	int32_t		data = -1 ;

	if (rank < HISTO_MAX - 1)
	{
		ix = (histoNextWriteIx - rank - 1) & (HISTO_MAX-1) ;
		addrs = FLASH_HISTO_ADDR + (ix * W25Q_SECTOR_SIZE) ;

		W25Q_SpiTake () ;
		W25Q_Read (& data, addrs, sizeof (data)) ;
		W25Q_SpiGive () ;
		if (pAddrs != NULL)
		{
			* pAddrs = addrs ;
		}
	}
	return data != -1 ;
}

//--------------------------------------------------------------------------------
//	This function add a record to the flash history
//	It becomes the rank 0 record

void	histoWrite (energyCounters_t * pCounters, powerH_t * pPower)
{
	uint32_t	addrs ;

	// Write only if the time is valid
	if (statusWTest (STSW_TIME_OK))
	{
		addrs = FLASH_HISTO_ADDR + (histoNextWriteIx * W25Q_SECTOR_SIZE) ;
		W25Q_SpiTake () ;

		W25Q_Write (pCounters, addrs, sizeof (energyCounters_t)) ;

		addrs += sizeof (energyCounters_t) ;
		W25Q_Write (pPower, addrs, sizeof (powerHistory)) ;

		// Erase the next slot (the older record)
		histoNextWriteIx = (histoNextWriteIx + 1) & (HISTO_MAX - 1) ;
		addrs = FLASH_HISTO_ADDR + (histoNextWriteIx * W25Q_SECTOR_SIZE) ;
		W25Q_EraseSector (addrs) ;

		W25Q_SpiGive () ;
	}
}

//--------------------------------------------------------------------------------
//	Read one history record from flash
//	rank: 0 to HISTO_MAX-2, 0 is the most recent
//  returns true on success
//	returns false if the rank is invalid or the required record is empty

bool	histoRead (uint32_t rank, energyCounters_t * pCounters, powerH_t * pPower)
{
	uint32_t	addrs ;

	if (! histoCheckRank (rank, & addrs))
	{
		return false ;
	}

	W25Q_SpiTake () ;
	if (pCounters != NULL)
	{
		// Read energy counters
		W25Q_Read (pCounters, addrs, sizeof (energyCounters_t)) ;
	}
	if (pPower != NULL)
	{
		// Read power counters
		addrs += sizeof (energyCounters_t) ;
		W25Q_Read (pPower, addrs, sizeof (powerHistory)) ;
	}
	W25Q_SpiGive () ;
	return true ;
}

//--------------------------------------------------------------------------------
//	Read some part of a power history record from flash
//	rank: 0 to HISTO_MAX-2

bool	histoPowerRead (void * pBuffer, uint32_t rank, uint32_t offset, uint32_t len)
{
	uint32_t	addrs ;

	if (! histoCheckRank (rank, & addrs))
	{
		return false ;
	}

	W25Q_SpiTake () ;
	W25Q_Read (pBuffer, addrs + sizeof (energyCounters_t) + offset, len) ;
	W25Q_SpiGive () ;
	return true ;
}

//--------------------------------------------------------------------------------
// Erase ALL the flash history area

void	histoErase		(void)
{
	uint32_t	ii ;
	uint32_t	addr ;

	W25Q_SpiTake () ;

	addr = FLASH_HISTO_SECTOR ;
	for (ii = 0 ; ii < HISTO_MAX ; ii++)
	{
		W25Q_EraseSector (addr) ;
		addr += W25Q_SECTOR_SIZE ;
	}
	W25Q_SpiGive () ;
	histoNextWriteIx = 0 ;
}

//--------------------------------------------------------------------------------

bool	histoInit (void)
{
	bool	res ;

	res = histoFindNext (& histoNextWriteIx) ;
	if (! res)
	{
		histoNextWriteIx = 0 ;
	}
	powerHistoReset () ;

	return res ;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Erase all flash areas used for data memorization
//	Except configuration that has a checksum
//	To use when the data structure has changed:
//	therefore there is a discrepancy between what is in memory and what is in flash.

void	histoFlashErase (void)
{
	// Erase history area
	histoErase () ;

	// Erase total energy area
	W25Q_SpiTake () ;
	W25Q_EraseSector (FLASH_ENERGY_ADDR) ;
	W25Q_SpiGive () ;
}

//--------------------------------------------------------------------------------
