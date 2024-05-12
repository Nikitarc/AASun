/**
 * @file	httpUtil.c
 * @brief	HTTP Server Utilities	
 * @version 1.0
 * @date	2014/07/15
 * @par Revision
 *			2014/07/15 - 1.0 Release
 * @author	
 * \n\n @par Copyright (C) 1998 - 2014 WIZnet. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "httpUtil.h"

#include "AASun.h"		// To get access to computedData
#include "jsmn.h"
#include "display.h"

#include "aautils.h"

// To extract POST data
static	const char contentTag [] = "Content-Length: " ;
static	const char endTag [] = "\r\n\r\n" ;

// On 1st call pUri is a pointer to the URI.
// On next calls pUri must be NULL and ppSave must contain the value returned by the previous call

static	bool findParam (char * pUri, char * pName, char * pValue, char ** ppSave)
{
	char	* pStr, * pToken, * pSave ;

	// Get the next pair
	pStr = strtok_r (pUri, "&", ppSave) ;

	if (pStr != NULL)
	{
		// Get the name and the value
		pToken = strtok_r (pStr, "=", & pSave) ;
		if (pToken != NULL)
		{
			strcpy (pName, pToken) ;
			pToken = strtok_r (pSave, "=", & pSave) ;
			if (pToken != NULL)
			{
				strcpy (pValue, pToken) ;
				return true ;
			}
		}
	}
	* pName = 0 ;
	* pValue = 0 ;
	return false ;		// No parameter
}

//------------------------------------------------------------------

uint8_t http_get_cgi_handler_common (uint8_t * uri_name, char * pUriData, uint8_t * buf, uint32_t lenMax, uint32_t * file_len)
{
	uint8_t ret = HTTP_OK;
	uint16_t len = 0;

	char	midName [16] ;
	char	midValue [16] ;
	char	* pSaveParam ;

	// Find 1st parameter which is the message id
	findParam (pUriData, midName, midValue, & pSaveParam) ;

	// Find 2nd parameter
//	findParam (NULL, name2, value2, & pSaveParam) ;

	if (strcmp ((const char *) uri_name, "volt.cgi") == 0)
	{
		len = aaSnPrintf((char*)buf, lenMax,
							"{\"vRms\":\"%ld\","
							"\"i1Rms\":\"%ld\","
							"\"p1Real\":\"%ld\","
							"\"p1App\":\"%ld\","
							"\"cPhi1\":\"%ld\","
							"\"i2Rms\":\"%ld\","
							"\"p2Real\":\"%ld\","
							"\"p2App\":\"%ld\","
							"\"cPhi2\":\"%ld\","
							"\"pDiv\":\"%lu\","
							"\"Counter1\":\"%lu\","
							"\"Counter2\":\"%lu\""
#if (defined IX_I3)
							",\"i3Rms\":\"%ld\","
							"\"p3Real\":\"%ld\","
							"\"p3App\":\"%ld\","
							"\"cPhi3\":\"%ld\""
#endif
#if (defined IX_I4)
							",\"i4Rms\":\"%ld\","
							"\"p4Real\":\"%ld\","
							"\"p4App\":\"%ld\","
							"\"cPhi4\":\"%ld\""
#endif
							"}",
							 	 computedData.vRms >> VOLT_SHIFT,
								 computedData.iData[0].iRms,		// x 2^I_SHIFT
							 	 computedData.iData[0].powerReal >> POWER_SHIFT,
							 	 computedData.iData[0].powerApp  >> POWER_SHIFT,
							 	 computedData.iData[0].cosPhi,					// x 1000
								 computedData.iData[1].iRms,		// x 2^I_SHIFT
							 	 computedData.iData[1].powerReal >> POWER_SHIFT,
							 	 computedData.iData[1].powerApp  >> POWER_SHIFT,
							 	 computedData.iData[1].cosPhi,					// x 1000
							 	 computedData.powerDiverted	>> POWER_DIVERTER_SHIFT,
								 (pulseCounter [0].pulsePeriodCount* pulseCounter [0].pulsePowerCoef) >> PULSE_P_SHIFT,
								 (pulseCounter [1].pulsePeriodCount* pulseCounter [1].pulsePowerCoef) >> PULSE_P_SHIFT
#if (defined IX_I3)
								 , computedData.iData[2].iRms,		// x 2^I_SHIFT
							 	 computedData.iData[2].powerReal >> POWER_SHIFT,
							 	 computedData.iData[2].powerApp  >> POWER_SHIFT,
							 	 computedData.iData[2].cosPhi					// x 1000
#endif
#if (defined IX_I4)
								 , computedData.iData[3].iRms,		// x 2^I_SHIFT
							 	 computedData.iData[3].powerReal >> POWER_SHIFT,
							 	 computedData.iData[3].powerApp  >> POWER_SHIFT,
							 	 computedData.iData[3].cosPhi					// x 1000
#endif
						) ;
		if (len >= lenMax)
		{
			// Buffer too small
			ret = HTTP_FAILED ;
			len = 0 ;
		}
	}
	else if (strcmp ((const char *) uri_name, "energy.cgi") == 0)
	{
		energyCounters_t	energy ;
		energyCounters_t	* pE = NULL ;
		uint32_t			index ;

		switch (midValue [0])
		{
			case 'T':		// Total energy
				pE = & energyWh ;
				break ;

			case 'D':		// Today energy
				pE = & dayEnergyWh ;
				break ;

			case 'H':		// History energy
				// Get index parameter
				findParam (NULL, midName, midValue, & pSaveParam) ;
				index = strtoul (midValue, NULL, 10)  ;
				pE = & energy ;
				pE->date = -1 ;		// Avoid empty response. date 0 signals an error
				if (index < HISTO_MAX-1)
				{
					// On return *pE is ok or -1
					histoRead (index, pE, NULL) ;
				}
				break ;

			default:
				break ;
		}
		if (pE != NULL)
		{
			len = aaSnPrintf((char*)buf, lenMax,
					"{\"date\":\"%ld\""
					",\"e0\":\"%ld\""
					",\"e1\":\"%ld\""
					",\"e2\":\"%ld\""
					",\"e3\":\"%ld\""
					",\"e4\":\"%ld\""
#if (defined IX_I3)
					",\"e5\":\"%ld\""
#endif
#if (defined IX_I4)
					",\"e6\":\"%ld\""
#endif
					",\"e7\":\"%ld\""
					",\"e8\":\"%ld\"}",

					pE->date, pE->energyImported, pE->energyExported,
					pE->energyDiverted1, pE->energyDiverted2,
					pE->energy2,
#if (defined IX_I3)
					pE->energy3,
#endif
#if (defined IX_I4)
					pE->energy4,
#endif
					(pE->energyPulse [0] * pulseCounter [0].pulseEnergyCoef) >> PULSE_E_SHIFT,
					(pE->energyPulse [1] * pulseCounter [1].pulseEnergyCoef) >> PULSE_E_SHIFT) ;
		}
		else
		{
			// Invalid request
			ret = HTTP_FAILED ;
			len = 0 ;
		}
	}
	else if (strcmp ((const char *) uri_name, "meter.cgi") == 0)	// Linky
	{
		len = aaSnPrintf((char*)buf, lenMax,
				"{\"volt\":\"%ld\","
				"\"cnt\":\"%ld\","
				"\"pApp\":\"%ld\"}",
				meterVolt, meterBase, meterPapp) ;
	}
	else if (strcmp ((const char *) uri_name, "statusWord.cgi") == 0)
	{
		len = aaSnPrintf((char*)buf, lenMax,
				"{\"SW\":\"%lu\"}",
				statusWord) ;
	}
	else if (strcmp ((const char *) uri_name, "config.cgi") == 0)
	{
		// Send all the configuration parameters
		len = aaSnPrintf((char*)buf, lenMax,
				"{\"vCal\":\"%ld\""
				",\"phaseCal\":\"%ld\""
				",\"i1Cal\":\"%ld\""
				",\"i1Offset\":\"%ld\""
				",\"i2Cal\":\"%ld\""
				",\"i2Offset\":\"%ld\""
#if (defined IX_I3)
				",\"i3Cal\":\"%ld\""
				",\"i3Offset\":\"%ld\""
#endif
#if (defined IX_I4)
				",\"i4Cal\":\"%ld\""
				",\"i4Offset\":\"%ld\""
#endif
				,
				voltCal,
				phaseCal,
				aaSunCfg.iSensor [0].iCal,
				aaSunCfg.iSensor [0].iAdcOffset,
				aaSunCfg.iSensor [1].iCal,
				aaSunCfg.iSensor [1].iAdcOffset
#if (defined IX_I3)
				,aaSunCfg.iSensor [2].iCal,
				aaSunCfg.iSensor [2].iAdcOffset
#endif
#if (defined IX_I4)
				,aaSunCfg.iSensor [3].iCal,
				aaSunCfg.iSensor [3].iAdcOffset
#endif
				) ;

		len += aaSnPrintf((char*)buf + len, lenMax-len,
				",\"p1Cal\":\"%ld\""
				",\"p1Offset\":\"%ld\""
				",\"p2Cal\":\"%ld\""
				",\"p2Offset\":\"%ld\""
#if (defined IX_I3)
				",\"p3Cal\":\"%ld\""
				",\"p3Offset\":\"%ld\""
#endif
#if (defined IX_I4)
				",\"p4Cal\":\"%ld\""
				",\"p4Offset\":\"%ld\""
#endif
				",\"pMax1\":\"%ld\""
				",\"pVolt1\":\"230\""
				",\"pMargin1\":\"%ld\""
				",\"pMax2\":\"%ld\""
				",\"pVolt2\":\"230\""
				",\"pMargin2\":\"%ld\""
				,
				aaSunCfg.iSensor [0].powerCal,
				aaSunCfg.iSensor [0].powerOffset >> POWER_SHIFT,
				aaSunCfg.iSensor [1].powerCal,
				aaSunCfg.iSensor [1].powerOffset >> POWER_SHIFT,
#if (defined IX_I3)
				aaSunCfg.iSensor [2].powerCal,
				aaSunCfg.iSensor [2].powerOffset >> POWER_SHIFT,
#endif
#if (defined IX_I4)
				aaSunCfg.iSensor [3].powerCal,
				aaSunCfg.iSensor [3].powerOffset >> POWER_SHIFT,
#endif
				powerDiv [0].powerDiverter230,
				powerDiv [0].powerMargin >> POWER_SHIFT,
				powerDiv [1].powerDiverter230,
				powerDiv [1].powerMargin >> POWER_SHIFT) ;

		len += aaSnPrintf((char*)buf + len, lenMax-len,
				",\"cksP\":\"%ld\""
				",\"cksI\":\"%ld\""
				",\"ckdP\":\"%ld\""
				",\"ckdI\":\"%ld\"",
				(syncPropFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR,
				(syncIntFactor * 10000 + SPID_SCALE_FACTOR / 2) / SPID_SCALE_FACTOR,
				powerPropFactor,
				powerIntFactor) ;

		len += aaSnPrintf((char*)buf + len, lenMax-len,
				",\"counter1\":\"%lu\""
				",\"counter2\":\"%lu\""
				",\"cfp\":\"%lu\""
				",\"display\":\"%s\"",
				pulseCounter [0].pulsepkWh,
				pulseCounter [1].pulsepkWh,
				aaSunCfg.favoritePage,
				aaSunCfg.displayController == DISPLAY_NONE ? "None" :
				aaSunCfg.displayController == DISPLAY_SH1106 ? "SH1106" : "SSD1306") ;

		len += aaSnPrintf((char*)buf + len, lenMax-len,
				",\"clip\":\"%lu\""
				",\"clmask\":\"%lu\""
				",\"clgw\":\"%lu\""
				",\"cldns\":\"%lu\"",
				aaSunCfg.lanCfg.ip  [0] << 24 | aaSunCfg.lanCfg.ip  [1] << 16 | aaSunCfg.lanCfg.ip  [2] << 8 | aaSunCfg.lanCfg.ip  [3],
				aaSunCfg.lanCfg.sn  [0] << 24 | aaSunCfg.lanCfg.sn  [1] << 16 | aaSunCfg.lanCfg.sn  [2] << 8 | aaSunCfg.lanCfg.sn  [3],
				aaSunCfg.lanCfg.gw  [0] << 24 | aaSunCfg.lanCfg.gw  [1] << 16 | aaSunCfg.lanCfg.gw  [2] << 8 | aaSunCfg.lanCfg.gw  [3],
				aaSunCfg.lanCfg.dns [0] << 24 | aaSunCfg.lanCfg.dns [1] << 16 | aaSunCfg.lanCfg.dns [2] << 8 | aaSunCfg.lanCfg.dns [3]) ;

		len += aaSnPrintf((char*)buf + len, lenMax-len,
				",\"n0\":\"%s\""
				",\"n1\":\"%s\""
				",\"n2\":\"%s\""
				",\"n3\":\"%s\""
				",\"n4\":\"%s\""
//#if (defined IX_I3)
				",\"n5\":\"%s\""
//#endif
//#if (defined IX_I4)
				",\"n6\":\"%s\""
//#endif
				",\"n7\":\"%s\""
				",\"n8\":\"%s\"}",
				aaSunCfg.eName [0],
				aaSunCfg.eName [1],
				aaSunCfg.eName [2],
				aaSunCfg.eName [3],
				aaSunCfg.eName [4],
//#if (defined IX_I3)
				aaSunCfg.eName [5],
//#endif
//#if (defined IX_I4)
				aaSunCfg.eName [6],
//#endif
				aaSunCfg.eName [7],
				aaSunCfg.eName [8]) ;


		if (len >= lenMax)
		{
			// Buffer too small
			ret = HTTP_FAILED ;
			len = 0 ;
		}
	}
	else if (strcmp ((const char *) uri_name, "temperature.cgi") == 0)	// Temperature sensors
	{
		uint32_t	rank ;
		uint32_t	ii ;
		int32_t		temperature ;

		// Argument: "mid=N" where N is the sensor index (not the rank) '1' to '4' or 'All'
		// Returns raw temperature value: signed binary << TEMP_SENSOR_SHIFT
		// Invalid sensor returns 200°C << TEMP_SENSOR_SHIFT

		// Internally temperature index starts at 0. The user index starts at 1.
		if (midValue [0] >= '1'  &&  midValue [0] <= (0x30 + TEMP_SENSOR_MAX))
		{
			rank = midValue [0] - 0x30 - 1 ;
			temperature = 200 << TEMP_SENSOR_SHIFT ;	// default: invalid temperature
			for (ii = 0 ; ii < TEMP_SENSOR_MAX ; ii++)
			{
				if (pTempSensors->sensors[ii].rank == rank  &&  pTempSensors->sensors[ii].present != 0)
				{
					temperature =  pTempSensors->sensors[ii].rawTemp ;
					break ;
				}
			}
			len = aaSnPrintf((char*)buf, lenMax,
					"\"temp%u\":\"%ld\","
					"\"factor\":\"%ld\"}",
					rank + 1, temperature,
					1 << TEMP_SENSOR_SHIFT) ;
		}
		else if (midValue [0] == 'A')
		{
			buf [0] = '{' ;
			buf [1] = 0 ;
			len = 1 ;
			for (ii = 0 ; ii < TEMP_SENSOR_MAX ; ii++)
			{
				temperature = 200 << TEMP_SENSOR_SHIFT ;
				if (pTempSensors->sensors[ii].present != 0)
				{
					temperature =  pTempSensors->sensors[ii].rawTemp ;
				}
				len += aaSnPrintf((char*)buf+len, lenMax-len,
						"\"temp%u\":\"%ld\",",
						pTempSensors->sensors[ii].rank+1, temperature) ;
			}
			len += aaSnPrintf((char*)buf+len, lenMax-len,
					"\"factor\":\"%ld\"}",
					1 << TEMP_SENSOR_SHIFT) ;
		}
		else
		{
aaPrintf ("Unknown MID '%s'\n", midValue) ;
			ret = HTTP_FAILED ;
			len = 0 ;
		}
	}
	else if (strcmp ((const char *) uri_name, "powerHisto.cgi") == 0)	// Power history data
	{
		uint32_t	mid = strtoul (midValue, NULL, 10) ;
		uint32_t	index ;

		// The mid is 1 to 4
		// This http server have a buffer of only 2 kB, not enough for the full history.
		// Then the server allows to acquire it in 2 parts.
		enum
		{
			todayPart1     = 1,
			todayPart2     = 2,
			yesterdayPart1 = 3,
			yesterdayPart2 = 4
		};

		// Get the history index (useless for today)
		findParam (NULL, midName, midValue, & pSaveParam) ;
		index = strtoul (midValue, NULL, 10)  ;

		switch (mid)
		{
			case todayPart1:
				len = sizeof (powerH_t) * (POWER_HISTO_MAX_WHEADER / 2) ;
				memcpy (buf, powerHistory, len) ;
				break ;

			case todayPart2:
				len = sizeof (powerH_t) * (POWER_HISTO_MAX_WHEADER - (POWER_HISTO_MAX_WHEADER / 2)) ;
				memcpy (buf, & powerHistory [POWER_HISTO_MAX_WHEADER / 2], len) ;
				break ;

			case yesterdayPart1:
				len = sizeof (powerH_t) * (POWER_HISTO_MAX_WHEADER / 2) ;
				if (! histoPowerRead (buf, index, 0, len))
				{
					len = 4 ;	// Data not found
					* (int32_t *) buf = -1 ;	// Fake data: avoid empty response
				}
				break ;

			case yesterdayPart2:
				len = sizeof (powerH_t) * (POWER_HISTO_MAX_WHEADER - (POWER_HISTO_MAX_WHEADER / 2)) ;
				if (! histoPowerRead (buf, index, sizeof (powerH_t) * (POWER_HISTO_MAX_WHEADER / 2), len))
				{
					len = 4 ;	// Data not found
					* (int32_t *) buf = -1 ;	// Fake data: avoid empty response
				}
				break ;

			default:
				len = 0 ;
		}
		if (len == 0)
		{
			ret = HTTP_FAILED ;		// Data not found
		}
	}

	else if (strcmp ((const char *) uri_name, "version.cgi") == 0)
	{
		len = aaSnPrintf((char*)buf, lenMax,
				"{\"soft\":\"%ld\""
				",\"wifi\":\"%ld\""
				",\"wifiAP\":\"%ld\"}",
				AASunVersion (),
				wifiSoftwareVersion,
				wifiModeAP) ;
	}

	else if (strcmp ((const char *) uri_name, "enames.cgi") == 0)
	{
		len = aaSnPrintf ((char*) buf, lenMax,
				"{\"n0\":\"%s\""
				",\"n1\":\"%s\""
				",\"n2\":\"%s\""
				",\"n3\":\"%s\""
				",\"n4\":\"%s\""
#if (defined IX_I3)
				",\"n5\":\"%s\""
#endif
#if (defined IX_I4)
				",\"n6\":\"%s\""
#endif
				",\"n7\":\"%s\""
				",\"n8\":\"%s\"}",

				aaSunCfg.eName [0],
				aaSunCfg.eName [1],
				aaSunCfg.eName [2],
				aaSunCfg.eName [3],
				aaSunCfg.eName [4],
#if (defined IX_I3)
				aaSunCfg.eName [5],
#endif
#if (defined IX_I4)
				aaSunCfg.eName [6],
#endif
				aaSunCfg.eName [7],
				aaSunCfg.eName [8]) ;
	}

	else if (strcmp ((const char *) uri_name, "divrules.cgi") == 0)	// Diverting rules
	{
		char	* pStr = (char *) buf ;

		strcpy (pStr, "{\"rule1\":\"") ;
		len = strlen (pStr) ;
		len += divRulePrint (& aaSunCfg.diverterRule[0], pStr + len, 128) ;

		strcpy (pStr + len, "\",\"rule2\":\"") ;
		len += 11 ;
		len += divRulePrint (& aaSunCfg.diverterRule[1], pStr + len, 128) ;
		strcpy (pStr + len, "\"}") ;
		len += 2 ;
//aaPuts (pStr) ; aaPutChar ('\n') ;
	}

	else if (strcmp ((const char *) uri_name, "forcerules.cgi") == 0)	// Forcing rules
	{
		char			* pStr = (char *) buf ;
		forceRules_t	* pForce = aaSunCfg.forceRules ;

		// Send only valid forcing
		* pStr = '{' ;
		len = 1 ;
		for (uint32_t ii = 0 ; ii < FORCE_MAX ; ii++)
		{
			if (forceRuleIsValid (pForce))
			{
				if (len != 1)
				{
					// This is not the 1st forcing: prepend a comma
					pStr [len] = ',' ;
					len++ ;
				}
				len += aaSnPrintf (pStr + len, 128, "\"start%u\":\"", ii) ;
				len += forceRulePrint (pForce, true, pStr + len, 128) ;
				len += aaSnPrintf (pStr + len, 128, "\",\"stop%u\":\"", ii) ;
				len += forceRulePrint (pForce, false, pStr + len, 128) ;
				pStr [len++] = '"' ;
			}
			pForce++ ;
		}
		pStr[len++] = '}' ;
//aaPrintf (" %d %s\n", len, pStr) ;
	}

	else if (strcmp ((const char *) uri_name, "dfstatus.cgi") == 0)	// All forcing/diverting status
	{
		len = forceJsonStatus ((char *) buf, lenMax) ;
	}

	else if (strcmp ((const char *) uri_name, "variable.cgi") == 0)	// All variable Vx + Anti-legionella
	{
		buf [0] = '{' ;
		len = 1u ;
		for (uint32_t ii = 0 ; ii < AASUNVAR_MAX ; ii++)
		{
			len += aaSnPrintf ((char *) buf + len, 128, "\"V%u\":", ii+1) ;
			len += aaSnPrintf ((char *) buf + len, 128, "\"%d\",", aaSunVariable [ii]) ;
		}
		if (aaSunCfg.alFlag == 0)
		{
			// Anti-legionella OFF
			len += aaSnPrintf ((char *) buf + len, 128, "\"alSensor\":\"Off\",") ;
		}
		else if ((aaSunCfg.alFlag & AL_FLAG_INPUT) != 0)
		{
			// Anti-legionella In
			len += aaSnPrintf ((char *) buf + len, 128, "\"alSensor\":\"I%c\",", '1' + (aaSunCfg.alFlag & AL_FLAG_NUMMASK)) ;
			len += aaSnPrintf ((char *) buf + len, 128, "\"alValue\":\"%d\",", aaSunCfg.alValue) ;
		}
		else
		{
			// Anti-legionella Tn
			len += aaSnPrintf ((char *) buf + len, 128, "\"alSensor\":\"T%c\",", '1' + (aaSunCfg.alFlag & AL_FLAG_NUMMASK)) ;
			len += aaSnPrintf ((char *) buf + len, 128, "\"alValue\":\"%d\",", aaSunCfg.alValue) ;
		}
		buf [len-1] = '}' ;	// Replace the last ',' with '}'
//aaPrintf ("%d %s\n", len, buf) ;
	}

	else
	{
		// CGI file not found
		ret = HTTP_FAILED;
	}

	* file_len = len ;
	return ret;
}

//------------------------------------------------------------------
// To call from W5500 httpServer.c
// On entry  buf is pHTTP_TX which contain the parsed st_http_request, with the full URI, example: /toto.cgi?a=1
// On output buf will contain the response body of size file_len

uint8_t http_get_cgi_handler (uint8_t * uri_name, uint8_t * buf, uint32_t lenMax, uint32_t * file_len)
{
	char	* pUri = (char *)((st_http_request *) buf)->URI ;
	char	* pStr ;

	// skip .cgi name
	pStr = strchr (pUri, '?') ;
	if (pStr == NULL)
	{
		pUri += strlen (pUri) ;		// No parameter
	}
	else
	{
		pUri = pStr + 1 ; // +1 to skip '?'
	}

	return http_get_cgi_handler_common (uri_name, pUri, buf, lenMax, file_len) ;
}

//------------------------------------------------------
//------------------------------------------------------
// Find the start of the data, place a 0 at the end of the data, then return the address of the data.
// The contentTag tag is used to obtain the size of the data.
// Headers end with the tag (\r\n\r\n) and data starts at the next character.

static	char *	getRequestDataPtr (char * uri, uint32_t * pSize)
{
	char		* pStr ;
	uint32_t	size ;

	pStr =  strstr  (uri, contentTag) ;
	pStr += sizeof  (contentTag) - 1 ;
	size =  strtoul (pStr, NULL, 10) ;

	pStr = strstr (pStr, endTag) ;
	pStr += sizeof (endTag) - 1 ;

	pStr [size] = 0 ;
	* pSize = size ;
	return pStr ;
}

//------------------------------------------------------
// Runs through all the tree
// Returns a pointer to the next key item
// NULL if EOF
// -1 on error
// On the 1st call pT is the pointer to the token array,
// on next calls is must be NULL

static	jsmntok_t * jsmnNext (jsmntok_t * pTok, jsmntok_t ** pSave)
{
    jsmntok_t    * pT ;

    if (pTok != NULL)
    {
        // 1st call. Skip the 1st token who is the object
        * pSave = & pTok [1] ;
        return * pSave;
    }
    pT = * pSave ;

    if (pT [1].type == JSMN_ARRAY)
    {
        pT += 2 + pT [1].size ;
    }
    else
    {
        pT += 2 ;
    }

    if (pT->type == JSMN_ENDMARKER)
    {
        return NULL ;
    }
    * pSave = pT ;
    return pT ;
}

//------------------------------------------------------
//	To get a string value from a key
//	Support only simple key/string pair: doesn't use object, primitive, array...
//	So the return value is always a pointer to a string or NULL

static	char *	jsmnGetString (jsmntok_t * pTokens, char * pJson, char * key)
{
	jsmntok_t   * pSave ;
	jsmntok_t   * pT ;

	// Runs through all keys
	pT = jsmnNext (pTokens, & pSave) ;
	while (pT != NULL)
	{
		if ((pT->type == JSMN_STRING  ||  pT->type == JSMN_PRIMITIVE)  && strcmp (pJson + pT->start, key) == 0)
		{
			// Key found, the value is in pT+1
			return (pJson + (pT+1)->start) ;
		}
		pT = jsmnNext (NULL, & pSave) ;
	}
	return NULL ;
}

//------------------------------------------------------
// For test: Dump keys
/*
static	void	jsmnDump (jsmntok_t * pTokens, char * pJson)
{
	jsmntok_t   * pSave ;
	jsmntok_t   * pT ;

	pT = jsmnNext (pTokens, & pSave) ;
	while (pT != NULL)
	{
		aaPrintf ("%d %s%4d %4d, %4d  ", pT->type,
				pT->type == JSMN_UNDEFINED ? " UNDEF" :
				pT->type == JSMN_OBJECT    ? "   OBJ" :
				pT->type == JSMN_ARRAY     ? " ARRAY" :
				pT->type == JSMN_STRING    ? "STRING" :
				pT->type == JSMN_PRIMITIVE ? "  PRIM" : "    ??",
				pT->start, pT->end, pT->size) ;
		if (pT->type == JSMN_STRING)
		{
			aaPrintf ("\"%s = \"%s\"\"", pJson + pT->start,  pJson + (pT+1)->start) ;
		}
		aaPrintf ("\n") ;
		pT = jsmnNext (NULL, & pSave) ;
	}
}
*/
//------------------------------------------------------
//	Get signed value from JSON

static	bool getIntParameter (jsmntok_t * pTokens, char * pJson, char * pKey, int32_t * pParam)
{
	char		* pStr ;
	bool		bResult ;

	pStr = jsmnGetString (pTokens, pJson, pKey) ;
	if (pStr != NULL)
	{
		* pParam = atoi (pStr) ;
		bResult = true ;
	}
	else
	{
//		aaPrintf ("Not found: %s\n", pKey) ;
		bResult = false ;
	}
	return bResult ;
}

//	Get unsigned value from JSON

static	bool getUintParameter (jsmntok_t * pTokens, char * pJson, char * pKey, uint32_t * pParam)
{
	char		* pStr ;
	bool		bResult ;

	pStr = jsmnGetString (pTokens, pJson, pKey) ;
	if (pStr != NULL)
	{
		* pParam = strtoul (pStr, NULL, 10) ;
		return true ;
	}
	else
	{
//		aaPrintf ("Not found: %s\n", pKey) ;
		bResult = false ;
	}
	return bResult ;
}

//------------------------------------------------------
//------------------------------------------------------

typedef bool (* pMidFn) (jsmntok_t * pTokens, char * pJson, uint32_t * pError) ;

#define	JSON_ERROR	100		// Returned on JSON error!

//------------------------------------------------------
// For unused mid

static	bool	midResFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	(void) pTokens ;
	(void) pJson ;
	(void) pError ;
	return false ;
}

//------------------------------------------------------
// Write the current configuration to flash

static	bool	midWriteFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = false ;

	(void) pTokens ;
	(void) pJson ;

	* pError = 1 ;

	// The exclusive use is ensured by the SPI semaphore
	if (1u == writeCfg ())
	{
		bResult = true ;
	}

	return bResult ;
}

//------------------------------------------------------

static	bool	midDivOnOffFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = false ;
	char		* pStr ;

	* pError = 0 ;

	// bDiverterSet is a request to TOGGLE the diverter state
	// Set bDiverterSet only if the diverter isn't in the requested state
	pStr = jsmnGetString (pTokens, pJson, "div") ;
	if (pStr != NULL)
	{
		if (statusWTest (STSW_DIV_ENABLED) != (pStr [1] == 'n'))
		{
			bDiverterSet = true ;
		}
		bResult = true ;
	}
	return bResult ;
}

//------------------------------------------------------

static	bool	midVoltFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "vCal",     & voltCal) ;
	bResult &= getIntParameter (pTokens, pJson, "phaseCal", & phaseCal) ;

	return bResult ;
}

//------------------------------------------------------

static	bool	midCurrentFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "i1Cal",    & aaSunCfg.iSensor [0].iCal) ;
	bResult &= getIntParameter (pTokens, pJson, "i1Offset", & aaSunCfg.iSensor [0].iAdcOffset) ;

	bResult &= getIntParameter (pTokens, pJson, "i2Cal",    & aaSunCfg.iSensor [1].iCal) ;
	bResult &= getIntParameter (pTokens, pJson, "i2Offset", & aaSunCfg.iSensor [1].iAdcOffset) ;

#if (defined IX_I3)
	bResult &= getIntParameter (pTokens, pJson, "i3Cal",    & aaSunCfg.iSensor [2].iCal) ;
	bResult &= getIntParameter (pTokens, pJson, "i3Offset", & aaSunCfg.iSensor [2].iAdcOffset) ;
#endif

#if (defined IX_I4)
	bResult &= getIntParameter (pTokens, pJson, "i4Cal",    & aaSunCfg.iSensor [3].iCal) ;
	bResult &= getIntParameter (pTokens, pJson, "i4Offset", & aaSunCfg.iSensor [3].iAdcOffset) ;
#endif
	return bResult ;
}

//------------------------------------------------------

static	bool	midPowerFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	int32_t		value ;

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "p1Cal",    & aaSunCfg.iSensor [0].powerCal) ;
	bResult &= getIntParameter (pTokens, pJson, "p1Offset", & value) ;
	if (bResult)
	{
		aaSunCfg.iSensor [0].powerOffset = value << POWER_SHIFT ;
	}

	bResult &= getIntParameter (pTokens, pJson, "p2Cal",    & aaSunCfg.iSensor [1].powerCal) ;
	bResult &= getIntParameter (pTokens, pJson, "p2Offset", & value) ;
	if (bResult)
	{
		aaSunCfg.iSensor [1].powerOffset = value << POWER_SHIFT ;
	}

#if (defined IX_I3)
	bResult &= getIntParameter (pTokens, pJson, "p3Cal",    & aaSunCfg.iSensor [2].powerCal) ;
	bResult &= getIntParameter (pTokens, pJson, "p3Offset", & value) ;
	if (bResult)
	{
		aaSunCfg.iSensor [2].powerOffset = value << POWER_SHIFT ;
	}
#endif

#if (defined IX_I4)
	bResult &= getIntParameter (pTokens, pJson, "p4Cal",    & aaSunCfg.iSensor [3].powerCal) ;
	bResult &= getIntParameter (pTokens, pJson, "p4Offset", & value) ;
	if (bResult)
	{
		aaSunCfg.iSensor [3].powerOffset = value << POWER_SHIFT ;
	}
#endif
	return bResult ;
}

//------------------------------------------------------

static	bool	midDiverterFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	int32_t		power1 ;
	int32_t		power2 ;
	int32_t		volt1 ;
	int32_t		volt2 ;
	int32_t		margin1 ;
	int32_t		margin2 ;

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "pMax1",    & power1) ;
	bResult &= getIntParameter (pTokens, pJson, "pVolt1",   & volt1) ;
	bResult &= getIntParameter (pTokens, pJson, "pMargin1", & margin1) ;
	bResult &= getIntParameter (pTokens, pJson, "pMax2",    & power2) ;
	bResult &= getIntParameter (pTokens, pJson, "pVolt2",   & volt2) ;
	bResult &= getIntParameter (pTokens, pJson, "pMargin2", & margin2) ;

	if (bResult)
	{
		divSetPmax (0, power1, volt1) ;
		divSetPmax (1, power2, volt2) ;
		powerDiv [0].powerMargin = margin1 << POWER_SHIFT ;
		powerDiv [1].powerMargin = margin2 << POWER_SHIFT ;
	}
	return bResult ;
}

//------------------------------------------------------

static	bool	midCoefFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	int32_t		kp, ki ;

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "cksP", & kp) ;
	bResult &= getIntParameter (pTokens, pJson, "cksI", & ki) ;
	if (bResult)
	{
		// Set new synchro PI controller factors
		syncPropFactor = (kp * SPID_SCALE_FACTOR + 5000) / 10000 ;
		syncIntFactor  = (ki * SPID_SCALE_FACTOR + 5000) / 10000 ;
		aaCriticalEnter () ;
		sPidFactors (syncPropFactor, syncIntFactor) ;
		aaCriticalExit () ;
	}

	bResult &= getIntParameter (pTokens, pJson, "ckdP", & kp) ;
	bResult &= getIntParameter (pTokens, pJson, "ckdI", & ki) ;
	if (bResult)
	{
		// Set new diverter PI controller factors
		aaCriticalEnter () ;
		powerPropFactor = kp ;
		powerIntFactor  = ki ;
		aaCriticalExit () ;
	}

	return bResult ;
}

//------------------------------------------------------

static	bool	midCounterFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool			bResult = true ;
	pulseCounter_t	* pCounter ;
	int32_t			count ;			// pulse count / kWh

	(void) pError ;
	bResult &= getIntParameter (pTokens, pJson, "counter1", & count) ;
	if (bResult)
	{
		pCounter = & pulseCounter [0] ;
		aaCriticalEnter () ;
		pCounter->pulsepkWh       = count ;
		pCounter->pulseEnergyCoef = (1000 << PULSE_E_SHIFT) / count ;
		pCounter->pulsePowerCoef  = ((3600000 << PULSE_P_SHIFT) / PULSE_P_PERIOD) / count ;
		pCounter->pulseMaxCount   = (PULSE_E_MAX / 1000) * count ;
		powerHistory[0].powerPulse[0] = count != 0 ;	// To enable/disable counter display in power history
		aaCriticalExit () ;
	}

	bResult &= getIntParameter (pTokens, pJson, "counter2", & count) ;
	if (bResult)
	{
		pCounter = & pulseCounter [1] ;
		aaCriticalEnter () ;
		pCounter->pulsepkWh       = count ;
		pCounter->pulseEnergyCoef = (1000 << PULSE_E_SHIFT) / count ;
		pCounter->pulsePowerCoef  = ((3600000 << PULSE_P_SHIFT) / PULSE_P_PERIOD) / count ;
		pCounter->pulseMaxCount   = (PULSE_E_MAX / 1000) * count ;
		powerHistory[0].powerPulse[1] = count != 0 ;	// To enable/disable counter display in power history
		aaCriticalExit () ;
	}

	return bResult ;
}

//------------------------------------------------------

static	bool	midLanFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	uint32_t	addr ;

	(void) pError ;
	bResult &= getUintParameter (pTokens, pJson, "clip", & addr) ;
	if (bResult)
	{
		aaSunCfg.lanCfg.ip [0] = (addr >> 24) ;
		aaSunCfg.lanCfg.ip [1] = (addr >> 16) & 0xFF ;
		aaSunCfg.lanCfg.ip [2] = (addr >>  8) & 0xFF ;
		aaSunCfg.lanCfg.ip [3] = addr         & 0xFF ;
	}

	bResult &= getUintParameter (pTokens, pJson, "clmask", & addr) ;
	if (bResult)
	{
		aaSunCfg.lanCfg.sn [0] = (addr >> 24) ;
		aaSunCfg.lanCfg.sn [1] = (addr >> 16) & 0xFF ;
		aaSunCfg.lanCfg.sn [2] = (addr >>  8) & 0xFF ;
		aaSunCfg.lanCfg.sn [3] = addr         & 0xFF ;
	}

	bResult &= getUintParameter (pTokens, pJson, "clgw", & addr) ;
	if (bResult)
	{
		aaSunCfg.lanCfg.gw [0] = (addr >> 24) ;
		aaSunCfg.lanCfg.gw [1] = (addr >> 16) & 0xFF ;
		aaSunCfg.lanCfg.gw [2] = (addr >>  8) & 0xFF ;
		aaSunCfg.lanCfg.gw [3] = addr         & 0xFF ;
	}

	bResult &= getUintParameter (pTokens, pJson, "cldns", & addr) ;
	if (bResult)
	{
		aaSunCfg.lanCfg.dns [0] = (addr >> 24) ;
		aaSunCfg.lanCfg.dns [1] = (addr >> 16) & 0xFF ;
		aaSunCfg.lanCfg.dns [2] = (addr >>  8) & 0xFF ;
		aaSunCfg.lanCfg.dns [3] = addr         & 0xFF ;
	}

	return bResult ;
}

//------------------------------------------------------

static	bool	midFpFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;

	(void) pError ;
	bResult &= getUintParameter (pTokens, pJson, "cfp", & aaSunCfg.favoritePage) ;
	return bResult ;
}

//------------------------------------------------------

static	bool	midEnergyNamesFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	char		* pStr ;
	char		key [8] ;

	* pError = 0 ;
	for (uint32_t ii = 0 ; ii < energyCountersCount ; ii++)
	{
		aaSnPrintf (key, sizeof (key), "n%u", ii) ;
aaPrintf ("key %s\n", key) ;
		pStr = jsmnGetString (pTokens, pJson, key) ;
		if (pStr != NULL)
		{
aaPrintf ("val %s\n", pStr) ;
			if (strlen (pStr) < ENAME_MAX)
			{
				strcpy (aaSunCfg.eName [ii], pStr) ;
			}
		}
	}
	return true ;
}

//------------------------------------------------------
// Set one diverting rule

static	bool	midDivertingRuleFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	char		* pRule ;
	bool		bResult = true ;
	uint32_t	index ;

	bResult &= getUintParameter (pTokens, pJson, "index", & index) ;
	bResult &= index < 2 ;

	pRule = jsmnGetString (pTokens, pJson, "rule") ;
	bResult &= pRule != NULL ;

	if (bResult )
	{
		divRule_t	rule ;

		if (divRuleCompile (& rule, pRule, pError))
		{
			bspDisableIrq () ;		// Because the rules are used by AASun task
			aaSunCfg.diverterRule [index] = rule ;
			bspEnableIrq () ;
		}
		else
		{
			bResult = false ;
		}

	}
	return bResult ;
}

//------------------------------------------------------
// Set one forcing rule

static	bool	midForcingRulesFn (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	uint32_t	index ;
	char		* pStart, * pStop ;

	bResult &= getUintParameter (pTokens, pJson, "index", & index) ;
	bResult &= index < FORCE_MAX ;

	pStart = jsmnGetString (pTokens, pJson, "start") ;
	bResult &= pStart != NULL ;
	pStop = jsmnGetString (pTokens, pJson, "stop") ;
	bResult &= pStop != NULL ;

	if (bResult )
	{
		forceRules_t	force ;

		memset (& force, 0, sizeof (force)) ;
		if (forceRuleCompile (& force, pStart, pStop, pError))
		{
			// It is mandatory to invalidate the forcing before setting new rules
			bspDisableIrq () ;		// Because the rules are used by AASun task
			forceRuleInvalidate (& aaSunCfg.forceRules [index]) ;
			bspEnableIrq () ;

			bspDisableIrq () ;
			aaSunCfg.forceRules [index] = force ;
			bspEnableIrq () ;
		}
		else
		{
			bResult = false ;
		}
	}
	return bResult ;
}

//------------------------------------------------------

static bool	midForcingManualFn  (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	bool		bResult = true ;
	uint32_t	index ;

	bResult &= getUintParameter (pTokens, pJson, "index", & index) ;
	if (bResult)
	{
		* pError  = 0 ;
		bResult = forceSetManualOrder (index) ;
	}

	return bResult ;
}

//------------------------------------------------------

static bool	midResetEnergyTotalFn  (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	(void)  pTokens ;
	(void)  pJson ;
	(void)  pError ;

	bspDisableIrq () ;		// Because the data is also used by AASun task
	memset (& energyWh, 0, sizeof (energyWh)) ;
	energyWh.date = timeGetDayDate (& localTime) ;
	bspEnableIrq () ;
	return true ;
}

//------------------------------------------------------

static bool	midSetVarFn  (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	uint32_t	ii ;
	char		name [4] ;
	int32_t		value ;

	(void)  pError ;

	name [0] = 'V' ;
	name [2] = 0 ;

	for (ii = 0 ; ii < AASUNVAR_MAX ; ii++)
	{
		name [1] = '1' + ii ;
		if (getIntParameter (pTokens, pJson, name, & value))
		{
			aaSunVariable [ii] = value ;
		}
	}
	return true ;
}

//------------------------------------------------------
//	Anti-legionella setting

static bool	midSetAlFn  (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	char		* pSensor ;
	uint8_t		alFlag ;
	int32_t		number ;
	int32_t		value ;

	(void)  pError ;

	// Get sensor: Off, Tn, In
	pSensor = jsmnGetString (pTokens, pJson, "sensor") ;
	if (pSensor == NULL)
	{
		return false ;
	}
	if (! getIntParameter (pTokens, pJson, "value", & value))
	{
		return false ;
	}

	number = pSensor [1] - '1' ;
	switch (* pSensor)
	{
		case 'O':		// Off
			aaSunCfg.alFlag = 0 ;
			return true ;
			break ;

		case 'T':		// Use temperature
			if (number >= 0  &&  number < TEMP_SENSOR_MAX)
			{
				alFlag = AL_FLAG_EN | number ;
			}
			break ;

		case 'I':		// Use input
			if (number >= 0  &&  number < DIVE_INPUT_MAX)
			{
				alFlag = AL_FLAG_EN | AL_FLAG_INPUT | number ;
				value = value != 0 ;	// Convert value to 0 or 1
			}
			break ;

		default:
			return false ;
			break ;
	}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	bspDisableIrq () ;		// Because the data is also used by AASun task
	aaSunCfg.alFlag  = alFlag ;
	aaSunCfg.alValue = value ;
	bspEnableIrq () ;
#pragma GCC diagnostic pop

	return true ;
}

//------------------------------------------------------

static bool	midSetDisplayFn  (jsmntok_t * pTokens, char * pJson, uint32_t * pError)
{
	char		* pDisplay ;

	(void)  pError ;

	pDisplay = jsmnGetString (pTokens, pJson, "display") ;
	if (pDisplay == NULL)
	{
		return false ;
	}

	if (strcmp (pDisplay, "SH1106") == 0)
	{
		aaSunCfg.displayController = DISPLAY_SH1106 ;
	}
	else if (strcmp (pDisplay, "SSD1306") == 0)
	{
		aaSunCfg.displayController = DISPLAY_SSD1306 ;
	}
	else if (strcmp (pDisplay, "None") == 0)
	{
		aaSunCfg.displayController = DISPLAY_NONE ;
	}
	else
	{
		return false ;
	}
	return true ;
}

//------------------------------------------------------
// Mandatory same order in enum and midFnArray

// setConfig.cgi message ID
enum
{
	midWrite         	= 0,
	midDivOnOff      	= 1,
	midRes1          	= 2,
	midRes2          	= 3,
	midVolt          	= 4,
	midCurrent       	= 5,
	midPower         	= 6,
	midDiverter      	= 7,
	midCoef          	= 8,
	midCounter       	= 9,
	midLan           	= 10,
	midFp            	= 11,
	midEnergyNames   	= 12,
	midDivertingRule 	= 13,
	midForcingRules  	= 14,
	midForcingManual 	= 15,
	midResetEnergyTotal = 16,
	midSetVar			= 17,
	midSetAl			= 18,
	midSetDisplay		= 19,
	midMax,					// Must be the last item
} ;

static	pMidFn	midFnArray [midMax] =
{
	midWriteFn,
	midDivOnOffFn,
	midResFn,
	midResFn,
	midVoltFn,
	midCurrentFn,
	midPowerFn,
	midDiverterFn,
	midCoefFn,
	midCounterFn,
	midLanFn,
	midFpFn,
	midEnergyNamesFn,
	midDivertingRuleFn,
	midForcingRulesFn,
	midForcingManualFn,
	midResetEnergyTotalFn,
	midSetVarFn,
	midSetAlFn,
	midSetDisplayFn
};

//------------------------------------------------------

// Rewrite http_post_cgi_handler to be common to WIFI and W5500

uint8_t http_post_cgi_handler_common (char * uri_name, postCgiParam_t * pParam)
{
	uint8_t		ret = HTTP_FAILED;
	uint16_t	len = 0;
	int32_t		nt ;
	jsmn_parser	* pParser = (jsmn_parser *) pParam->respBuffer ;
	jsmntok_t	* pTokens = (jsmntok_t *) (pParam->respBuffer + sizeof (jsmn_parser)) ;

	if(strcmp ((const char *) uri_name, "setConfig.cgi") == 0)
	{
		char		* pMid ;
		uint32_t	mid ;
		uint32_t	error = JSON_ERROR ;	// Default error: JSON tag not found

		// Build JSON array
		jsmn_init (pParser) ;
		nt = jsmn_parse (pParser, pParam->data, pParam->contentSize, pTokens, (DATA_BUF_SIZE - sizeof (jsmn_parser)) / sizeof (jsmntok_t)) ;
		if (nt < 0)
		{
		    aaPrintf ("Failed to parse JSON: %d\n", nt);
		}
		else
		{
// For test
//jsmnDump (pTokens, pJson) ;

			// Get the message ID value
			pMid = jsmnGetString (pTokens, pParam->data, "mid") ;
			if (pMid != NULL)
			{
				char	* pEnd ;
				mid = strtoul (pMid, & pEnd, 10) ;
//aaPrintf ("%d\n", mid) ;
				if ((pMid != pEnd)  &&  (* pEnd == 0)  &&  (mid < midMax))
				{
					// Call the function for this mid
					if ((* (midFnArray[mid]))(pTokens, pParam->data, & error))
					{
						// HTML request return value
						len = aaSnPrintf  (pParam->respBuffer, 100, "OK %u", mid) ;
						len = strlen (pParam->respBuffer) ;
						ret = HTTP_OK ;
					}
					else
					{
						// Fail
						aaSnPrintf  (pParam->respBuffer, 100, "Error %u %u", mid, error) ;
						len = strlen (pParam->respBuffer) ;
						ret = HTTP_OK ;
					}
				}
			}
		}
	}

	else
	{
		// CGI file not found
	}

	if (ret == HTTP_OK)
		* pParam->respLen = len ;
	return ret ;
}

// Call from W5500 library
// Input parameter buf is pHTTP_RX, with size DATA_BUF_SIZE
uint8_t http_post_cgi_handler (uint8_t * uri_name, st_http_request * p_http_request, uint8_t * buf, uint32_t * file_len)
{
	postCgiParam_t	param ;

	param.respBuffer = (char *) buf ;
	param.respLen    = file_len ;
	param.data       = getRequestDataPtr ((char *) p_http_request->URI, & param.contentSize) ;

	return http_post_cgi_handler_common ((char *) uri_name, & param) ;
}

//------------------------------------------------------
/*
uint8_t predefined_get_cgi_processor(uint8_t * uri_name, uint8_t * buf, uint16_t * len)
{
	(void) uri_name ;
	(void) buf ;
	(void) len ;
	return 0 ;
}

uint8_t predefined_set_cgi_processor(uint8_t * uri_name, uint8_t * uri, uint8_t * buf, uint16_t * en)
{
	(void) uri_name ;
	(void) uri ;
	(void) buf ;
	(void) en ;
	return 0 ;
}
*/
