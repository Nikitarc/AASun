/*
 * sntp.c
 *
 *  Created on: 2014. 12. 15.
 *      Author: Administrator
 */

// AdAstra: convert NTP EPOCH 1900 to EPOCH 2000
// Allows to manage the NTP rollover in 2036


#include <string.h>

#include "sntp.h"
#include "socket.h"

static	ntpformat	NTPformat;
static	datetime	Nowdatetime;
static	uint8_t		ntpmessage[48];
static	uint8_t		*data_buf;
static	uint8_t		NTP_SOCKET;
static	uint8_t		time_zone;
//static	uint16_t	ntp_retry_cnt=0; //counting the ntp retry number

/*
00)UTC-12:00 Baker Island, Howland Island (both uninhabited)
01) UTC-11:00 American Samoa, Samoa
02) UTC-10:00 (Summer)French Polynesia (most), United States (Aleutian Islands, Hawaii)
03) UTC-09:30 Marquesas Islands
04) UTC-09:00 Gambier Islands;(Summer)United States (most of Alaska)
05) UTC-08:00 (Summer)Canada (most of British Columbia), Mexico (Baja California)
06) UTC-08:00 United States (California, most of Nevada, most of Oregon, Washington (state))
07) UTC-07:00 Mexico (Sonora), United States (Arizona); (Summer)Canada (Alberta)
08) UTC-07:00 Mexico (Chihuahua), United States (Colorado)
09) UTC-06:00 Costa Rica, El Salvador, Ecuador (Galapagos Islands), Guatemala, Honduras
10) UTC-06:00 Mexico (most), Nicaragua;(Summer)Canada (Manitoba, Saskatchewan), United States (Illinois, most of Texas)
11) UTC-05:00 Colombia, Cuba, Ecuador (continental), Haiti, Jamaica, Panama, Peru
12) UTC-05:00 (Summer)Canada (most of Ontario, most of Quebec)
13) UTC-05:00 United States (most of Florida, Georgia, Massachusetts, most of Michigan, New York, North Carolina, Ohio, Washington D.C.)
14) UTC-04:30 Venezuela
15) UTC-04:00 Bolivia, Brazil (Amazonas), Chile (continental), Dominican Republic, Canada (Nova Scotia), Paraguay,
16) UTC-04:00 Puerto Rico, Trinidad and Tobago
17) UTC-03:30 Canada (Newfoundland)
18) UTC-03:00 Argentina; (Summer) Brazil (Brasilia, Rio de Janeiro, Sao Paulo), most of Greenland, Uruguay
19) UTC-02:00 Brazil (Fernando de Noronha), South Georgia and the South Sandwich Islands
20) UTC-01:00 Portugal (Azores), Cape Verde
21) UTC&#177;00:00 Cote d'Ivoire, Faroe Islands, Ghana, Iceland, Senegal; (Summer) Ireland, Portugal (continental and Madeira)
22) UTC&#177;00:00 Spain (Canary Islands), Morocco, United Kingdom
23) UTC+01:00 Angola, Cameroon, Nigeria, Tunisia; (Summer)Albania, Algeria, Austria, Belgium, Bosnia and Herzegovina,
24) UTC+01:00 Spain (continental), Croatia, Czech Republic, Denmark, Germany, Hungary, Italy, Kinshasa, Kosovo,
25) UTC+01:00 Macedonia, France (metropolitan), the Netherlands, Norway, Poland, Serbia, Slovakia, Slovenia, Sweden, Switzerland
26) UTC+02:00 Libya, Egypt, Malawi, Mozambique, South Africa, Zambia, Zimbabwe, (Summer)Bulgaria, Cyprus, Estonia,
27) UTC+02:00 Finland, Greece, Israel, Jordan, Latvia, Lebanon, Lithuania, Moldova, Palestine, Romania, Syria, Turkey, Ukraine
28) UTC+03:00 Belarus, Djibouti, Eritrea, Ethiopia, Iraq, Kenya, Madagascar, Russia (Kaliningrad Oblast), Saudi Arabia,
29) UTC+03:00 South Sudan, Sudan, Somalia, South Sudan, Tanzania, Uganda, Yemen
30) UTC+03:30 (Summer)Iran
31) UTC+04:00 Armenia, Azerbaijan, Georgia, Mauritius, Oman, Russia (European), Seychelles, United Arab Emirates
32) UTC+04:30 Afghanistan
33) UTC+05:00 Kazakhstan (West), Maldives, Pakistan, Uzbekistan
34) UTC+05:30 India, Sri Lanka
35) UTC+05:45 Nepal
36) UTC+06:00 Kazakhstan (most), Bangladesh, Russia (Ural: Sverdlovsk Oblast, Chelyabinsk Oblast)
37) UTC+06:30 Cocos Islands, Myanmar
38) UTC+07:00 Jakarta, Russia (Novosibirsk Oblast), Thailand, Vietnam
39) UTC+08:00 China, Hong Kong, Russia (Krasnoyarsk Krai), Malaysia, Philippines, Singapore, Taiwan, most of Mongolia, Western Australia
40) UTC+09:00 Korea, East Timor, Russia (Irkutsk Oblast), Japan
41) UTC+09:30 Australia (Northern Territory);(Summer)Australia (South Australia))
42) UTC+10:00 Russia (Zabaykalsky Krai); (Summer)Australia (New South Wales, Queensland, Tasmania, Victoria)
43) UTC+10:30 Lord Howe Island
44) UTC+11:00 New Caledonia, Russia (Primorsky Krai), Solomon Islands
45) UTC+11:30 Norfolk Island
46) UTC+12:00 Fiji, Russia (Kamchatka Krai);(Summer)New Zealand
47) UTC+12:45 (Summer)New Zealand
48) UTC+13:00 Tonga
49) UTC+14:00 Kiribati (Line Islands)
*/
void get_seconds_from_ntp_server(uint8_t *buf, uint16_t idx)
{
	uint32_t seconds = 0;
	uint8_t i=0;
	for (i = 0; i < 4; i++)
	{
		seconds = (seconds << 8) | buf[idx + i];
	}

	switch (time_zone)
	{
	case 0:
		seconds -=  12*3600;
		break;
	case 1:
		seconds -=  11*3600;
		break;
	case 2:
		seconds -=  10*3600;
		break;
	case 3:
		seconds -=  (9*3600+30*60);
		break;
	case 4:
		seconds -=  9*3600;
		break;
	case 5:
	case 6:
		seconds -=  8*3600;
		break;
	case 7:
	case 8:
		seconds -=  7*3600;
		break;
	case 9:
	case 10:
		seconds -=  6*3600;
		break;
	case 11:
	case 12:
	case 13:
		seconds -= 5*3600;
		break;
	case 14:
		seconds -=  (4*3600+30*60);
		break;
	case 15:
	case 16:
		seconds -=  4*3600;
		break;
	case 17:
		seconds -=  (3*3600+30*60);
		break;
	case 18:
		seconds -=  3*3600;
		break;
	case 19:
		seconds -=  2*3600;
		break;
	case 20:
		seconds -=  1*3600;
		break;
	case 21:                            //ï¼?
	case 22:
		break;
	default:
	case 23:
	case 24:
	case 25:
		seconds +=  1*3600;
		break;
	case 26:
	case 27:
		seconds +=  2*3600;
		break;
	case 28:
	case 29:
		seconds +=  3*3600;
		break;
	case 30:
		seconds +=  (3*3600+30*60);
		break;
	case 31:
		seconds +=  4*3600;
		break;
	case 32:
		seconds +=  (4*3600+30*60);
		break;
	case 33:
		seconds +=  5*3600;
		break;
	case 34:
		seconds +=  (5*3600+30*60);
		break;
	case 35:
		seconds +=  (5*3600+45*60);
		break;
	case 36:
		seconds +=  6*3600;
		break;
	case 37:
		seconds +=  (6*3600+30*60);
		break;
	case 38:
		seconds +=  7*3600;
		break;
	case 39:
		seconds +=  8*3600;
		break;
	case 40:
		seconds +=  9*3600;
		break;
	case 41:
		seconds +=  (9*3600+30*60);
		break;
	case 42:
		seconds +=  10*3600;
		break;
	case 43:
		seconds +=  (10*3600+30*60);
		break;
	case 44:
		seconds +=  11*3600;
		break;
	case 45:
		seconds +=  (11*3600+30*60);
		break;
	case 46:
		seconds +=  12*3600;
		break;
	case 47:
		seconds +=  (12*3600+45*60);
		break;
	case 48:
		seconds +=  13*3600;
		break;
	case 49:
		seconds +=  14*3600;
		break;

	}

	// Convert seconds to epoch 2000: manage 1st overrun of NTP
	if (seconds < TS_2000)
	{
		// ERA 1 from 2036 to 2172
		seconds = ((uint32_t)((uint64_t) 0x100000000ll - TS_2000) + seconds) ;
	}
	else
	{
		// ERA 0 from 1900 to 2036
		seconds -= TS_2000 ;
	}
	// Now seconds in is the year range 2000 to 2136

	//calculation for date
	calcdatetime(seconds);
}

extern	uint32_t get_httpServer_timecount(void) ;
static	uint32_t	sntpStartTime ;

void SNTP_init(uint8_t s, uint8_t *ntp_server, uint8_t tz, uint8_t *buf)
{
	uint8_t Flag;

	NTP_SOCKET = s;

	NTPformat.dstaddr[0] = ntp_server[0];
	NTPformat.dstaddr[1] = ntp_server[1];
	NTPformat.dstaddr[2] = ntp_server[2];
	NTPformat.dstaddr[3] = ntp_server[3];

	time_zone = tz;

	data_buf = buf;

	NTPformat.leap = 0;           /* leap indicator */
	NTPformat.version = 4;        /* version number */
	NTPformat.mode = 3;           /* mode */
#if 0
	NTPformat.stratum = 0;        /* stratum */
	NTPformat.poll = 0;           /* poll interval */
	NTPformat.precision = 0;      /* precision */
	NTPformat.rootdelay = 0;      /* root delay */
	NTPformat.rootdisp = 0;       /* root dispersion */
	NTPformat.refid = 0;          /* reference ID */
	NTPformat.reftime = 0;        /* reference time */
	NTPformat.org = 0;            /* origin timestamp */
	NTPformat.rec = 0;            /* receive timestamp */
	NTPformat.xmt = 1;            /* transmit timestamp */
#endif
	Flag = (NTPformat.leap<<6)+(NTPformat.version<<3)+NTPformat.mode; //one byte Flag
	memcpy(ntpmessage,(void const*)(&Flag),1);

	socket(NTP_SOCKET,Sn_MR_UDP,ntp_port,0);
	sendto(NTP_SOCKET,ntpmessage,sizeof(ntpmessage),NTPformat.dstaddr,ntp_port);
	sntpStartTime = get_httpServer_timecount();
}

//	SNTP_run() modified : no retry
//	Return: 1 success, 0 retry, -1 timeout

int8_t SNTP_run (datetime * pTime)
{
	uint16_t RSR_len;
	uint32_t destip = 0;
	uint16_t destport;
	uint16_t startindex = 40; //last 8-byte of data_buf[size is 48 byte] is xmt, so the startindex should be 40

	switch(getSn_SR(NTP_SOCKET))
	{
	case SOCK_UDP:
		if ((RSR_len = getSn_RX_RSR(NTP_SOCKET)) > 0)
		{
			if (RSR_len > MAX_SNTP_BUF_SIZE) RSR_len = MAX_SNTP_BUF_SIZE;	// if Rx data size is lager than TX_RX_MAX_BUF_SIZE
			recvfrom(NTP_SOCKET, data_buf, RSR_len, (uint8_t *)&destip, &destport);

			get_seconds_from_ntp_server(data_buf,startindex);
			* pTime = Nowdatetime ;
/*
			time->yy = Nowdatetime.yy;
			time->mo = Nowdatetime.mo;
			time->dd = Nowdatetime.dd;
			time->hh = Nowdatetime.hh;
			time->mm = Nowdatetime.mm;
			time->ss = Nowdatetime.ss;
			time->ws = Nowdatetime.wd;
*/
//			ntp_retry_cnt=0;
			// AdAstra: close changed to closesocket
			closesocket (NTP_SOCKET) ;
//			close(NTP_SOCKET);

			return 1 ;	// success
		}
		if ((get_httpServer_timecount () - sntpStartTime) > 3)
		{
			closesocket (NTP_SOCKET) ;
			return -1 ;	// Timeout
		}

/*
		if(ntp_retry_cnt<0xFFFF)
		{
			if(ntp_retry_cnt==0)//first send request, no need to wait
			{
				sendto(NTP_SOCKET,ntpmessage,sizeof(ntpmessage),NTPformat.dstaddr,ntp_port);
				ntp_retry_cnt++;
			}
			else // send request again? it should wait for a while
			{
				if((ntp_retry_cnt % 0xFFF) == 0) //wait time
				{
					sendto(NTP_SOCKET,ntpmessage,sizeof(ntpmessage),NTPformat.dstaddr,ntp_port);
#ifdef _SNTP_DEBUG_
					printf("ntp retry: %d\r\n", ntp_retry_cnt);
#endif
					ntp_retry_cnt++;
				}
			}
		}
		else //ntp retry fail
		{
			ntp_retry_cnt=0;
#ifdef _SNTP_DEBUG_
			printf("ntp retry failed!\r\n");
#endif
			// AdAstra: close changed to closesocket
			closesocket(NTP_SOCKET);
//			close(NTP_SOCKET);
		}
		break;
	case SOCK_CLOSED:
		socket(NTP_SOCKET,Sn_MR_UDP,ntp_port,0);
		break;
*/
	default:
		break ;
	}

	// Return value
	// 0 - failed / 1 - success
	return 0;
}

void calcdatetime (uint32_t seconds)
{
	uint32_t yf=0;
	uint32_t n=0,d=0,total_d=0,rz=0;
	uint32_t y=0,yr=0;
	int32_t  yd=0;
	const uint32_t p_year_total_sec=SECS_PERDAY*365;
	const uint32_t r_year_total_sec=SECS_PERDAY*366;

	//calculation for time
	n = seconds % SECS_PERDAY ;
	Nowdatetime.hh = n / 3600 ;
	n = n % 3600 ;
	Nowdatetime.mm = n / 60 ;
	Nowdatetime.ss = n % 60 ;

	// Week day
	Nowdatetime.wd = (uint8_t) (((seconds / SECS_PERDAY) + 6u) % 7u) ;

	n = seconds;
	total_d = seconds/(SECS_PERDAY);
	d=0;
	y = EPOCH_2000 ;
	while(n>=p_year_total_sec)
	{
		if(y%400==0 || (y%100!=0 && y%4==0))
		{
			n = n -(r_year_total_sec);
			d = d + 366;
		}
		else
		{
			n = n - (p_year_total_sec);
			d = d + 365;
		}
		y++;
	}

	Nowdatetime.yy = y;

	yd=0;
	yd = total_d - d;

	yf=1;
	while(yd>=28)
	{

		if(yf==1 || yf==3 || yf==5 || yf==7 || yf==8 || yf==10 || yf==12)
		{
			yd -= 31;
			if(yd<0)break;
			rz += 31;
		}

		if (yf==2)
		{
			if (y%400==0 || (y%100!=0 && y%4==0))
			{
				yd -= 29;
				if(yd<0)break;
				rz += 29;
			}
			else
			{
				yd -= 28;
				if(yd<0)break;
				rz += 28;
			}
		}
		if(yf==4 || yf==6 || yf==9 || yf==11 )
		{
			yd -= 30;
			if(yd<0)break;
			rz += 30;
		}
		yf += 1;

	}
	Nowdatetime.mo=yf;

	yr = total_d-d-rz;
	yr += 1;
	Nowdatetime.dd=yr;
}

uint32_t changedatetime_to_seconds (datetime * pDatetime)
{
	uint32_t seconds=0;
	uint32_t total_day=0;
	uint32_t i=0,run_year_cnt=0,l=0;

	l = pDatetime->yy;//low

	for(i=EPOCH_2000;i<l;i++)
	{
		if((i%400==0) || ((i%100!=0) && (i%4==0)))
		{
			run_year_cnt += 1;
		}
	}

	total_day=(l-EPOCH_2000-run_year_cnt)*365+run_year_cnt*366;

	for(i=1;i<=pDatetime->mo;i++)
	{
		if(i==5 || i==7 || i==10 || i==12)
		{
			total_day += 30;
		}
		if (i==3)
		{
			if (l%400==0 && l%100!=0 && l%4==0)
			{
				total_day += 29;
			}
			else
			{
				total_day += 28;
			}
		}
		if(i==2 || i==4 || i==6 || i==8 || i==9 || i==11)
		{
			total_day += 31;
		}
	}

	seconds = (total_day+pDatetime->dd-1)*24*3600;
	seconds += pDatetime->ss;//seconds
	seconds += pDatetime->mm*60;//minute
	seconds += pDatetime->hh*3600;//hour

	return seconds;
}
