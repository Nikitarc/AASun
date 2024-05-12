/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	diverter.c		Diverting and forcing processing

	When		Who	What
	29/10/23	ac	Creation

----------------------------------------------------------------------
*/

#include	"AASun.h"
#include	"temperature.h"
#include	"display.h"		// For IO access

#include	"aautils.h"

#include	<stdio.h>
#include	<stdlib.h>		// strtol
#include	<string.h>
#include	<ctype.h>

//--------------------------------------------------------------------------------
//	Power diverting

// A conversion array to get the SSR delay from an amount of power
// The amount of power is normalized in [0, 511]
// This is the formula of sin(x) integration from point x to the end of the 1/2 period: 0.5 + cos(x)/2

#define	P2DELAY_SHIFT			9
#define	P2DELAY_SHIFT_ROUND		(1 << (P2DELAY_SHIFT - 1)
#define	P2DELAY_MAX				(1 << P2DELAY_SHIFT)

#if (0)
// Theoretical curve for power to SSR delay conversion
const uint16_t	p2Delay [P2DELAY_MAX] =
{
// The maximum delay is 502. So if the delay is > 502 then there is no pulse
//	511,	511,	511,	511,	511,	511,	511,	511,	// 0
//	511,	511,	511,	510,	510,	510,	510,	510,	// 8
//	510,	510,	509,	509,	509,	509,	509,	508,	// 16
//	508,	508,	508,	508,	507,	507,	507,	506,
//	506,	506,	505,	505,	505,	504,	504,	504,	// 32
//	503,	503,	503,	502,	502,	501,	501,	500,

	511,	511,	511,	511,	511,	511,	511,	511,	// 0
	511,	511,	511,	511,	511,	511,	511,	511,	// 8
	511,	511,	511,	511,	511,	511,	511,	511,	// 16
	511,	511,	511,	511,	511,	511,	511,	511,
	511,	511,	511,	511,	511,	511,	511,	511,	// 32
	511,	511,	511,	502,	502,	501,	501,	500,

	500,	500,	499,	499,	498,	498,	497,	497,	// 48
	496,	496,	495,	494,	494,	493,	493,	492,
	492,	491,	490,	490,	489,	488,	488,	487,	// 64
	486,	486,	485,	484,	484,	483,	482,	482,
	481,	480,	479,	479,	478,	477,	476,	475,	// 80
	475,	474,	473,	472,	471,	471,	470,	469,
	468,	467,	466,	465,	464,	463,	463,	462,	// 96
	461,	460,	459,	458,	457,	456,	455,	454,
	453,	452,	451,	450,	449,	448,	447,	446,	// 112
	445,	444,	443,	442,	441,	439,	438,	437,
	436,	435,	434,	433,	432,	431,	429,	428,	// 128
	427,	426,	425,	424,	422,	421,	420,	419,
	418,	416,	415,	414,	413,	411,	410,	409,	// 144
	408,	406,	405,	404,	403,	401,	400,	399,
	397,	396,	395,	394,	392,	391,	390,	388,	// 160
	387,	386,	384,	383,	381,	380,	379,	377,
	376,	375,	373,	372,	370,	369,	368,	366,	// 176
	365,	363,	362,	360,	359,	358,	356,	355,
	353,	352,	350,	349,	347,	346,	345,	343,	// 192
	342,	340,	339,	337,	336,	334,	333,	331,
	330,	328,	327,	325,	324,	322,	321,	319,	// 208
	318,	316,	315,	313,	311,	310,	308,	307,
	305,	304,	302,	301,	299,	298,	296,	295,	// 224
	293,	291,	290,	288,	287,	285,	284,	282,
	281,	279,	277,	276,	274,	273,	271,	270,	// 240
	268,	266,	265,	263,	262,	260,	259,	257,
	256,	254,	252,	251,	249,	248,	246,	245,	// 256
	243,	241,	240,	238,	237,	235,	234,	232,
	230,	229,	227,	226,	224,	223,	221,	220,	// 272
	218,	216,	215,	213,	212,	210,	209,	207,
	206,	204,	203,	201,	200,	198,	196,	195,	// 288
	193,	192,	190,	189,	187,	186,	184,	183,
	181,	180,	178,	177,	175,	174,	172,	171,	// 304
	169,	168,	166,	165,	164,	162,	161,	159,
	158,	156,	155,	153,	152,	151,	149,	148,	// 320
	146,	145,	143,	142,	141,	139,	138,	136,
	135,	134,	132,	131,	130,	128,	127,	125,	// 336
	124,	123,	121,	120,	119,	117,	116,	115,
	114,	112,	111,	110,	108,	107,	106,	105,	// 352
	103,	102,	101,	100,	 98,	 97,	 96,	 95,
	93,		 92,	 91,	 90,	 89,	 87,	 86,	 85,	// 368
	84,		 83,	 82,	 80,	 79,	 78,	 77,	 76,
	75,		 74,	 73,	 72,	 70,	 69,	 68,	 67,	// 384
	66,		 65,	 64,	 63,	 62,	 61,	 60,	 59,
	58,		 57,	 56,	 55,	 54,	 53,	 52,	 51,	// 400
	50,		 49,	 48,	 48,	 47,	 46,	 45,	 44,
	43,		 42,	 41,	 40,	 40,	 39,	 38,	 37,	// 416
	36,		 36,	 35,	 34,	 33,	 32,	 32,	 31,
	30,		 29,	 29,	 28,	 27,	 27,	 26,	 25,	// 432
	25,		 24,	 23,	 23,	 22,	 21,	 21,	 20,
	19,		 19,	 18,	 18,	 17,	 17,	 16,	 15,	// 448
	15,		 14,	 14,	 13,	 13,	 12,	 12,	 11,
	11,		 11,	 10,	 10, 	 9, 	 9, 	 8, 	 8,		// 464
	 8, 	 7, 	 7, 	 7, 	 6, 	 6, 	 6, 	 5,

// The minimum allowed delay is 3 (~60us), so the array is modified accordingly
//	 5, 	 5, 	 4, 	 4, 	 4, 	 3, 	 3, 	 3,		// 480
//	 3, 	 3, 	 2, 	 2, 	 2, 	 2, 	 2, 	 1,		// 488
//	 1, 	 1, 	 1, 	 1, 	 1, 	 1, 	 0, 	 0,		// 496
//	 0, 	 0, 	 0, 	 0, 	 0, 	 0, 	 0, 	 0		// 504

	 5, 	 5, 	 4, 	 4, 	 4, 	 3, 	 3, 	 3,		// 480
	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3,		// 488
	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3,		// 496
	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3, 	 3		// 504

} ;
#endif // 0

#if (1)
// Radiator curve for power to SSR delay conversion
const uint16_t	p2Delay [P2DELAY_MAX] =
{
		511, 499, 488, 476, 465, 453, 450, 447, 444, 441, 438, 436, 434, 431, 429, 427, 	// 0
		426, 424, 423, 421, 420, 418, 417, 415, 414, 412, 411, 410, 409, 407, 406, 405, 	// 16
		404, 403, 401, 400, 399, 398, 397, 396, 394, 393, 392, 391, 390, 389, 388, 387, 	// 32
		385, 384, 383, 382, 381, 380, 380, 379, 378, 377, 377, 376, 375, 374, 374, 373, 	// 48
		372, 371, 371, 370, 369, 368, 367, 367, 366, 365, 364, 364, 363, 362, 361, 361, 	// 64
		360, 359, 358, 358, 357, 356, 355, 354, 354, 353, 352, 351, 351, 350, 349, 348, 	// 80
		348, 347, 346, 345, 345, 344, 343, 342, 342, 341, 341, 340, 339, 339, 338, 338, 	// 96
		337, 336, 336, 335, 334, 334, 333, 333, 332, 331, 331, 330, 330, 329, 328, 328, 	// 112
		327, 327, 326, 325, 325, 324, 324, 323, 322, 322, 321, 321, 320, 319, 319, 318, 	// 128
		317, 317, 316, 316, 315, 314, 314, 313, 313, 312, 311, 311, 310, 309, 309, 308, 	// 144
		308, 307, 306, 306, 305, 304, 304, 303, 303, 302, 301, 301, 300, 299, 299, 298, 	// 160
		298, 297, 296, 296, 295, 294, 294, 293, 293, 292, 291, 291, 290, 289, 289, 288, 	// 176
		288, 287, 286, 286, 285, 284, 284, 283, 283, 282, 281, 281, 280, 280, 279, 279, 	// 192
		278, 278, 277, 277, 276, 276, 275, 275, 274, 274, 273, 273, 272, 272, 271, 271, 	// 208
		270, 270, 269, 269, 268, 268, 267, 267, 266, 266, 265, 265, 264, 264, 263, 263, 	// 224
		262, 262, 261, 261, 260, 260, 259, 259, 258, 258, 257, 257, 256, 256, 255, 255, 	// 240
		254, 254, 253, 253, 252, 252, 251, 251, 250, 250, 249, 249, 249, 248, 248, 247, 	// 256
		247, 246, 246, 245, 245, 245, 244, 244, 243, 243, 242, 242, 241, 241, 240, 240, 	// 272
		240, 239, 239, 238, 238, 237, 237, 236, 236, 236, 235, 235, 234, 234, 233, 233, 	// 288
		232, 232, 231, 231, 230, 230, 229, 229, 228, 228, 227, 227, 226, 226, 225, 225, 	// 304
		224, 224, 223, 223, 222, 221, 221, 220, 220, 219, 219, 218, 218, 217, 217, 216, 	// 320
		216, 215, 215, 214, 214, 213, 212, 212, 211, 211, 210, 210, 209, 209, 208, 208, 	// 336
		207, 207, 206, 206, 205, 205, 204, 203, 203, 202, 202, 201, 200, 200, 199, 199, 	// 352
		198, 197, 197, 196, 195, 195, 194, 194, 193, 192, 192, 191, 191, 190, 189, 189, 	// 368
		188, 188, 187, 186, 186, 185, 185, 184, 183, 183, 182, 182, 181, 180, 180, 179, 	// 384
		178, 178, 177, 177, 176, 175, 175, 174, 174, 173, 172, 171, 171, 170, 169, 168, 	// 400
		168, 167, 166, 165, 165, 164, 163, 162, 162, 161, 160, 159, 158, 158, 157, 156, 	// 416
		155, 155, 154, 153, 152, 152, 151, 150, 149, 149, 148, 147, 146, 145, 145, 144, 	// 432
		143, 142, 142, 141, 140, 139, 139, 138, 137, 136, 136, 135, 134, 133, 132, 131, 	// 448
		130, 129, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 116, 115, 113, 	// 464
		112, 110, 109, 107, 106, 104, 103, 101, 100,  98,  97,  95,  94,  92,  91,  89, 	// 480
		 88,  86,  84,  83,  81,  79,  71,  63,  55,  47,  40,  32,  24,  16,   8,   3, 	// 496
} ;
#endif // 0

#if (0)
// Halogen lamp curve for power to SSR delay conversion
const uint16_t	p2Delay [P2DELAY_MAX] =
{
		511, 511, 499, 492, 486, 480, 478, 475, 473, 470, 468, 466, 465, 463, 462, 460, 	// 0
		459, 457, 456, 455, 454, 452, 451, 450, 449, 447, 446, 445, 443, 442, 440, 439, 	// 16
		438, 436, 435, 433, 432, 431, 430, 429, 428, 427, 426, 425, 424, 423, 422, 421, 	// 32
		420, 419, 418, 417, 416, 415, 415, 414, 413, 412, 411, 410, 410, 409, 408, 407, 	// 48
		406, 405, 405, 404, 403, 402, 401, 401, 400, 399, 398, 397, 396, 396, 395, 394, 	// 64
		393, 392, 391, 391, 390, 389, 388, 387, 387, 386, 385, 384, 383, 382, 382, 381, 	// 80
		380, 379, 378, 377, 377, 376, 375, 374, 374, 373, 372, 372, 371, 370, 370, 369, 	// 96
		368, 367, 367, 366, 365, 365, 364, 363, 363, 362, 361, 361, 360, 359, 359, 358, 	// 112
		357, 356, 356, 355, 354, 354, 353, 352, 352, 351, 350, 350, 349, 348, 348, 347, 	// 128
		346, 345, 345, 344, 343, 343, 342, 341, 341, 340, 339, 339, 338, 338, 337, 337, 	// 144
		336, 336, 335, 335, 334, 334, 333, 333, 332, 332, 331, 331, 330, 330, 329, 329, 	// 160
		328, 328, 327, 327, 326, 326, 325, 325, 324, 324, 323, 323, 322, 322, 321, 321, 	// 176
		320, 320, 319, 319, 318, 318, 317, 317, 316, 316, 315, 315, 314, 314, 313, 313, 	// 192
		312, 312, 311, 311, 310, 310, 309, 309, 308, 308, 307, 307, 306, 306, 305, 305, 	// 208
		304, 304, 303, 303, 302, 302, 301, 301, 300, 300, 299, 299, 298, 298, 297, 297, 	// 224
		296, 296, 295, 295, 294, 294, 293, 293, 292, 292, 291, 291, 290, 290, 289, 289, 	// 240
		288, 287, 287, 286, 286, 285, 285, 284, 284, 283, 283, 282, 282, 281, 281, 280, 	// 256
		280, 279, 279, 278, 278, 277, 277, 276, 276, 275, 275, 274, 274, 273, 273, 272, 	// 272
		272, 271, 271, 270, 270, 269, 269, 268, 268, 267, 267, 266, 266, 265, 265, 264, 	// 288
		264, 263, 263, 262, 261, 261, 260, 260, 259, 258, 258, 257, 257, 256, 255, 255, 	// 304
		254, 253, 253, 252, 252, 251, 250, 250, 249, 249, 248, 247, 247, 246, 246, 245, 	// 320
		244, 244, 243, 243, 242, 241, 241, 240, 240, 239, 238, 238, 237, 236, 236, 235, 	// 336
		235, 234, 233, 233, 232, 232, 231, 230, 230, 229, 228, 227, 227, 226, 225, 225, 	// 352
		224, 223, 223, 222, 221, 220, 220, 219, 218, 218, 217, 216, 215, 215, 214, 213, 	// 368
		213, 212, 211, 211, 210, 209, 208, 208, 207, 206, 206, 205, 204, 203, 203, 202, 	// 384
		201, 201, 200, 199, 199, 198, 197, 196, 196, 195, 194, 193, 193, 192, 191, 190, 	// 400
		189, 188, 188, 187, 186, 185, 184, 183, 183, 182, 181, 180, 179, 179, 178, 177, 	// 416
		176, 175, 174, 174, 173, 172, 171, 170, 169, 169, 168, 167, 166, 165, 165, 164, 	// 432
		163, 162, 161, 160, 160, 159, 158, 157, 156, 155, 155, 154, 153, 152, 151, 150, 	// 448
		149, 148, 147, 146, 144, 143, 142, 141, 140, 139, 138, 137, 135, 133, 132, 130, 	// 464
		128, 126, 124, 123, 121, 119, 118, 116, 115, 113, 112, 110, 109, 107, 106, 104, 	// 480
		103, 101,  99,  97,  95,  93,  84,  74,  65,  56,  47,  37,  28,  19,   9,   3, 	// 496
} ;
#endif // 0

static	int32_t		powerDiverterITerm ; 	// Power diverted in the previous half cycle
static	int32_t		powerRawLast ;			// Low pass filter

//--------------------------------------------------------------------------------

int32_t	divProcessing (uint32_t meterStep)
{
	int32_t		powerD = 0 ;	// Initialize to avoid compiler warning

	if (bDiverterSet)
	{
		// This is a request to toggle the diverter global state On or Off
		statusWToggle (STSW_DIV_ENABLED) ;
		statusWClear (STSW_DIVERTING) ;
		bDiverterSet = false ; 	// Request done
		powerRawLast = 0 ;
		timerOutputChannelSet (TIMSSR, powerDiv[0].ssrChannel, p2Delay [0]) ;	// SSR in known state: off
		timerOutputChannelSet (TIMSSR, powerDiv[1].ssrChannel, p2Delay [0]) ;
		powerDiverterITerm = powerDiv [diverterIndex].powerMargin >> (POWER_SHIFT - POWER_DIVERTER_SHIFT) ;	// Initialize PID Iterm
	}
	if (statusWTest (STSW_DIV_ENABLED))
	{
		int32_t 	error ;
		int32_t		ix ;
		int32_t		powerRaw ;
		int32_t 	power ;
		powerDiv_t	* pDiv ;

		// Is there is a request to switch to the other diverting channel ?
		if (diverterSwitch != DIV_SWITCH_IDLE)
		{
			if (diverterSwitch == DIV_SWITCH_NONE)
			{
				timerOutputChannelSet (TIMSSR, powerDiv[0].ssrChannel, p2Delay [0]) ;	// SSR in known state: off
				timerOutputChannelSet (TIMSSR, powerDiv[1].ssrChannel, p2Delay [0]) ;
aaPuts ("Div switch none\n") ;
			}
			else
			{
				diverterIndex = diverterSwitch - 1u ;	// This in the index of the channel to switch to
aaPrintf ("Div switch %d\n", diverterIndex+1) ;

				// Stop the other channel
				timerOutputChannelSet (TIMSSR, powerDiv [diverterIndex ^ 1u].ssrChannel, p2Delay [0]) ;

				powerDiverterITerm = powerDiv [diverterIndex].powerMargin >> (POWER_SHIFT - POWER_DIVERTER_SHIFT) ;	// Initialize PID Iterm
			}
			diverterChannel = diverterSwitch ;
			diverterSwitch  = DIV_SWITCH_IDLE ;					// Done
		}
		pDiv = & powerDiv [diverterIndex] ;

		if (diverterChannel != DIV_SWITCH_NONE)
		{
			// Weighted power calculation: average with previous half period power
			// This averages the differences of the positive and negative 1/2 periods (phase shift correction)
			powerRaw = aaSunCfg.iSensor [0].powerCal * (powerSumHalfCycle / ((int32_t) MAIN_SAMPLE_COUNT / 2)) - aaSunCfg.iSensor [0].powerOffset ;
			power = (powerRaw + powerRawLast) / 2 ;
			powerRawLast = powerRaw ;

			// Power is in Watt and left shifted by POWER_SHIFT
			// Power < 0 if there is export, so there is power to divert on next half period

			error = pDiv->powerMargin - power ;
			error >>= (POWER_SHIFT - POWER_DIVERTER_SHIFT) ;	// Error in W left shifted POWER_DIVERTER_SHIFT

			powerDiverterITerm += (error * powerIntFactor) / POWER_DIVERTER_FACTOR ;
			// Avoid an infinite growing of powerDiverterITerm :
			//    negative when there is nothing to divert
			//    positive when the power to divert is > powerDiverterMax
			// The diverted power can't be negative so clamp powerDiverterITerm to 0
			// The diverted power can't be > powerDiverterMax so clamp powerDiverterITerm to powerDiverterMax
			if (powerDiverterITerm < 0)
			{
				powerDiverterITerm = 0 ;
			}
			if (powerDiverterITerm > pDiv->powerDiverterMax)
			{
				powerDiverterITerm = pDiv->powerDiverterMax ;
			}

			powerD = powerDiverterITerm + ((error * powerPropFactor) / POWER_DIVERTER_FACTOR) ;
			if (powerD < 0)
			{
				powerD = 0 ; // Nothing to divert
			}
			// powerD is POWER_DIVERTER_SHIFT left shifted

			// Convert diverted power to index in p2Delay
			// Go from power in W to power*2^P2DELAY_SHIFT, then divide by the max diverted power.
			// This gives a normalized value between 0 and 511.
			// Compute ix with power in W
/*
TODO Detecter coupure routage (température chauffe eau: https://forum-photovoltaique.fr/viewtopic.php?f=110&t=55244&start=1100#p684954
Routage a PMAX et (PuissancePV + Pimportée < powerDiverterMax) est impossible
On ne peut pas router au max si la somme des puissances disponibles est inférieure a powerDiverterMax
Mais il faut connaitre PuissancePV...

TODO Routage si powerD au  dessus d'un seuil (evite gresillement CES a très petite puissance)
*/
			ix = ((powerD >> POWER_DIVERTER_SHIFT) << P2DELAY_SHIFT)  / (pDiv->powerDiverterMax >> POWER_DIVERTER_SHIFT) ;

			// Avoid out of range index
			if (ix > 511)
			{
				ix = 511 ;		// Very high power to divert
			}
			if (p2Delay [ix] > TIMSSR_MAX)
			{
				// Very large delay, so very low power to divert
				// To avoid very short SSR pulse, nothing to divert
				ix = 0 ;
				powerD = 0 ;
			}

			// Set the timer delay for the next 1/2 period
			// The timer will be started on next meterStep, at the very beginning of the 1/2 period
			timerOutputChannelSet (TIMSSR, pDiv->ssrChannel, p2Delay [ix]) ;

			if (displayWTest (DPYW_DISPLAY_DIV_DATA))
			{
				char	str [12] ;
				static	volatile uint32_t free;

				// Avoid UART TX buffer overflow and task blocking
				free = aaCheckPutChar () ;
				if (free > 200)
				{
tst2_1 () ;
					inttoa (powerRaw >> POWER_SHIFT, str, sizeof (str), 5) ;	aaPuts (str) ;
					inttoa (power >> POWER_SHIFT,    str, sizeof (str), 5) ;	aaPuts (str) ;
					inttoa (powerD,                  str, sizeof (str), 8) ;	aaPuts (str) ;
//							inttoa (ix,                      str, sizeof (str), 5) ;	aaPuts (str) ;
//							inttoa (p2Delay [ix],            str, sizeof (str), 5) ;	aaPuts (str) ;
					inttoa (powerDiverterITerm,      str, sizeof (str), 8) ;	aaPuts (str) ;
					aaPutChar (' ') ;
					aaPutChar ((meterStep == (MAIN_SAMPLE_COUNT - 1u)) ? '+' : '-') ;
					aaPutChar ('\n') ;
tst2_0 () ;
				}
				else
				{
					{
						aaPuts ("??\n") ;
					}
				}
			}
		}
	}
	else
	{
		// Diverting is disabled (for debug only)
		if (displayWTest (DPYW_DISPLAY_DIV_DATA))
		{
			char	str [12] ;
			int32_t	powerRaw = aaSunCfg.iSensor [0].powerCal * (powerSumHalfCycle / ((int32_t) MAIN_SAMPLE_COUNT / 2)) - aaSunCfg.iSensor [0].powerOffset ;
			int32_t power = (powerRaw + powerRawLast) / 2 ;
			powerRawLast = powerRaw ;
			inttoa (powerRaw >> POWER_SHIFT, str, sizeof (str), 5) ;	aaPuts (str) ;
			inttoa (power    >> POWER_SHIFT, str, sizeof (str), 5) ;	aaPuts (str) ;
			aaPutChar ('\n') ;
		}
	}
	powerSumHalfCycle = 0 ;
	return powerD ;		// The routed power in W << POWER_DIVERTER_SHIFT
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Diverting rules processing
//--------------------------------------------------------------------------------

// divRule_t.flags
#define	DIV_FLAG_VALID		1
#define	DIV_FLAG_ON			2

#define	PD_NUMBER			8		// On compiling rule PD becomes P8

//--------------------------------------------------------------------------------
//	Set/test flags bits

static inline bool divFlagTest (divRule_t * pDiv, uint32_t flagBit)
{
	return ((pDiv->flags & flagBit) != 0) ;
}

static inline void divFlagSet (divRule_t * pDiv, uint32_t flagBit)
{
	pDiv->flags |= flagBit ;
}

static inline void divFlagClear (divRule_t * pDiv, uint32_t flagBit)
{
	pDiv->flags &= ~flagBit ;
}

//--------------------------------------------------------------------------------

static	bool	divRuleCompile_ (divRule_t * pRule, char * pText, uint32_t * pError)
{
	int32_t		number ;
	divExpr_t	* pExpr ;
	char		* pStr, * pItem, * pSaveExpr, * pSaveItem ;

	pRule->count = 0 ;
	aaStrToUpper (pText) ;
	pItem = strtok_r (pText, "&", & pSaveExpr) ;
	if (pItem == NULL)
	{
		// An empty string is allowed: the condition is always true
		return true ;
	}

	while (1)
	{
//aaPrintf (pExpr) ; aaPutChar ('\n') ;

		// Extract left argument
		pStr = strtok_r (pItem, " \t\r\n", & pSaveItem) ;
		if (pStr == NULL)
		{
			if (pRule->count == 0)
			{
				// A string with only separators is allowed, this is an empty string: the condition is always true
				return true ;
			}
			* pError = 1 ;		// & not followed by a parameter
			return false ;
		}
		// Parameter without expression
		if (* pStr == 'O')
		{
			if (pStr [1] == 'N')
			{
				// The condition is always true
				divFlagSet (pRule, DIV_FLAG_ON) ;
			}
			if (pStr [1] == 'F')
			{
				// The condition is always false
				divFlagClear (pRule, DIV_FLAG_ON) ;
			}
		}
		else
		{
			// Expression with operator and value
			// Parameter with number
			if (* pStr != 'T'  &&  * pStr != 'I'  &&  * pStr != 'P')
			{
				* pError = 2 ;	// Invalid parameter name
				return false ;
			}
			if (pStr [1] == 0)
			{
				* pError = 3 ;
				return false ;	// No n in Tn or In or Pn
			}
			if (* pStr == 'P'  &&  pStr [1] == 'D')
			{
				pStr [1] = '1' + PD_NUMBER ;		// Replace 'D' by '8' for diverted power
			}
			if (pStr [1] < '1'  ||  pStr [1] > '9')
			{
				* pError = 4 ;	// Invalid n in Tn or In or Pn
				return false ;
			}
			number = pStr [1] - '0' ;
			switch (pStr [0])
			{
				case 'T':
					if (number > DIVE_TEMP_MAX)
					{
						* pError = 5 ;		// Invalid n in Tn
						return false ;
					}
					number-- ;	// 1:4  becomes  0:3
					break ;

				case 'I':
					if (number > DIVE_INPUT_MAX)
					{
						* pError = 6 ;		// Invalid n in In
						return false ;
					}
					number-- ;	// 1:4  becomes  0:3
					break ;

				case 'P':
					if (number > DIVE_POWER_MAX  &&  number != (PD_NUMBER+1))
					{
						* pError = 7 ;		// Invalid n in Pn
						return false ;
					}
					number-- ;	// 1:4  becomes  0:3
					break ;

				default:
					break ;
			}
			pExpr = & pRule->expr[pRule->count] ;
			pExpr->type   = pStr [0] ;
			pExpr->number = number ;

			// Extract operator
			pStr = strtok_r (NULL, " \t\r\n", & pSaveItem) ;
			if (pStr == NULL ||  pStr [1] != 0)
			{
				* pError = 8 ;		// Missing operator
				return false ;
			}
			if ((* pStr != '<'  &&  * pStr != '>'  &&  * pStr != '=') ||
				(pExpr->type != 'I'  &&  * pStr == '='))
			{

				* pError = 9 ;		// Only < > = are allowed, = is allowed only for input
				return false ;
			}
			pExpr->comp = * pStr ;

			// Extract the value
			pStr = strtok_r (NULL, " \t\r\n", & pSaveItem) ;
			if (pStr == NULL)
			{
				* pError = 10 ;		// Missing value
				return false ;
			}
			pExpr->value = strtol (pStr, & pStr, 0) ;
			if (* pStr != 0)
			{
				* pError = 11 ;		// Not a numerical value
				return false ;
			}

			// The rule is valid
			pRule->count++ ;
		}

		// Extract next expression
		pItem = strtok_r (NULL, "&", & pSaveExpr) ;
		if (pItem == NULL)
		{
			break ;	// End of line
		}
		if (pRule->count == DIVE_EXPR_MAX)
		{
			* pError = 12 ;	// Too many expression in the rule
			return false ;
		}
	}

	return true ;
}

// Convert a string version of the rule to a divRule_t structure
// On fail returns false and the error is set
// BEWARE: the string pointed to by pText is modified: can't be in flash

bool	divRuleCompile (divRule_t * pRule, char * pText, uint32_t * pError)
{
	bool	bOk ;

	* pError = 0 ;
	pRule->flags = 0 ;
	bOk = divRuleCompile_ (pRule, pText, pError) ;
	if (bOk)
	{
		divFlagSet (pRule, DIV_FLAG_VALID) ;
	}
	return bOk ;
}

//--------------------------------------------------------------------------------
//	Returns a string version of the rule
// It is mandatory that ON or OFF be the 1st word in the string

uint32_t	divRulePrint (divRule_t * pRule, char * pText, uint32_t size)
{
	uint32_t	len, ii ;

	len = 0 ;
	* pText = 0 ;
	if (divFlagTest (pRule, DIV_FLAG_ON))
	{
		strcpy (pText, "ON  &  ") ;
		len = 7 ;
	}
	else
	{
		strcpy (pText, "OFF  &  ") ;	// The rule is always false
		len = 8 ;
	}

	for (ii = 0 ; ii < pRule->count  &&  len < size ; ii++)
	{
		len += aaSnPrintf (pText+len, size - len, "%c%d %c %d  &  ",
						pRule->expr[ii].type,
						pRule->expr[ii].number+1,
						pRule->expr[ii].comp,
						pRule->expr[ii].value) ;
	}

	// Remove the 5 last chars: "  &  "
	len -= 5 ;
	pText [len] = 0 ;
	return len  ;
}

//--------------------------------------------------------------------------------
// Returns true if the rule evaluation is true

bool	divRuleCheck (divRule_t * pRule)
{
	divExpr_t		* pExpr ;
	bool			result = true ;
	uint32_t			ii ;

	if (! divFlagTest (pRule, DIV_FLAG_ON)  ||  ! divFlagTest (pRule, DIV_FLAG_VALID))
	{
		return false ;	// This diverting channel is inhibited: always false
	}

	for (ii  = 0 ; ii < pRule->count ; ii++)
	{
		pExpr = & pRule->expr [ii] ;
		switch (pExpr->type)
		{
			case 'T':
				{
					int32_t			temperature ;
					int32_t			number ;

					if (! tsGetTemp (pExpr->number, & temperature))
					{
						return false ;	// Can't get temperature from this TS
					}
					temperature >>= TEMP_SENSOR_SHIFT ;
					number = pExpr->value ;
					if (pRule->expr [ii].comp == '<')
					{
						if (! (temperature < number + pExpr->tempHyst))
						{
							pExpr->tempHyst = -DIVE_TEMP_HYST ;
//aaPrintf (" > %3d %2d ", temperature, pExpr->tempHyst) ;
							return false ;
						}
						pExpr->tempHyst = 0 ;
//aaPrintf (" < %3d %2d ", temperature, pExpr->tempHyst) ;
					}
					else
					{
						// Use >= to set the temperature number in the true range
						if (! (temperature >= number + pExpr->tempHyst))
						{
							pExpr->tempHyst = +DIVE_TEMP_HYST ;
//aaPrintf (" > %3d %2d ", temperature, pExpr->tempHyst) ;
							return false ;
						}
//aaPrintf (" < %3d %2d ", temperature, pExpr->tempHyst) ;
						pExpr->tempHyst = 0 ;
					}
					// Test OK
				}
				break ;

			case 'I':
				{
					uint32_t		value ;

					if (inputGet (pExpr->number, & value))
					{
						if ((int32_t) value != pExpr->value)
						{
							return false ;
						}
					}
					else
					{
						return false ;	// Invalid input number
					}
				}
				break ;

			case 'P':
				{
					int32_t			value ;
					int32_t			power ;

					if (pExpr->number == PD_NUMBER)
					{
						// This is for estimated diverted power
						power = computedData.powerDiverted >> POWER_DIVERTER_SHIFT ;
					}
					else
					{
						// This is for CT power
						power = computedData.iData [pExpr->number].powerReal >> POWER_SHIFT ;
					}
					value = pExpr->value ;
					if (pExpr->comp == '<')
					{
						if ( power >= value)
						{
							return false ;
						}
					}
					else
					{
						if ( power <= value)
						{
							return false ;
						}
					}
				}
				break ;

			default:
				AA_ASSERT (0) ;
				break ;
		}
	}

	return result ;
}

//--------------------------------------------------------------------------------
//	Enable or disable a diverter
//	The next call to diverterNext() will update the diverting status

void	divRuleEnable (divRule_t * pRule, bool bOn)
{
	if (bOn)
	{
		divFlagSet (pRule, DIV_FLAG_ON) ;
	}
	else
	{
		divFlagClear (pRule, DIV_FLAG_ON) ;
	}
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//	Forcing processing
//--------------------------------------------------------------------------------

#define	FORCE_CHAN_MAX			(POWER_DIV_MAX + 2)	// Count of forcing channels : 2 SSR + 1 relay + Digital output

#define	FORCE_BURST_PERIOD		240		// Burst cycle duration, seconds

#define	FORCE_AUTO_X			1					// On debug set this to 1 for faster test
#define	FORCE_AUTO_ON			(20*FORCE_AUTO_X)	// Default value: min time ON  (or time before OFF)
#define	FORCE_AUTO_OFF			(20*FORCE_AUTO_X)	// Default value: min time OFF (or time before ON)
#define	FORCE_AUTO_KF			(10*FORCE_AUTO_X)	// Default value: min time OFF (or time before ON)

// Bits values for forceRules_t status and flags

#define	FORCE_STS_VALID			0x01	// Contain valid rules, eligible for activation
#define	FORCE_STS_RUNNING		0x02	// Active (manage an output)
#define	FORCE_STS_DMAX_WAITING	0x04	// DMAX has elapsed but start rule is still true
#define	FORCE_STS_BURST_ON		0x08	// Burst ratio is in the ON phase
#define	FORCE_STS_START_RULE	0x10	// The start rule is true
#define	FORCE_STS_STOP_RULE		0x20	// The stop  rule is true
#define	FORCE_STS_MANUAL_ORDER	0x40	// Manual order pending

#define	FORCE_FLAG_ON			0x01	// This forcing is enabled
#define	FORCE_FLAG_DMIN			0x02	// A DMIN is present in the rule
#define	FORCE_FLAG_DMAX			0x04	// A DMAX is present in the rule
#define	FORCE_FLAG_D_MASK		(FORCE_FLAG_DMIN | FORCE_FLAG_DMAX)
#define	FORCE_FLAG_MODE_AUTO	0x08
#define	FORCE_FLAG_MODE_STD		0x10
#define	FORCE_FLAG_MODE_MASK	(FORCE_FLAG_MODE_AUTO | FORCE_FLAG_MODE_STD)
#define	FORCE_FLAG_RATIO		0x20	// Use power ratio
#define	FORCE_FLAG_RATIOBURST	0x40	// Use burst duty cycle

// For forceRules_t.autoState
#define	AUTO_STATE_IDLE			0
#define	AUTO_STATE_MIN_ON		1
#define	AUTO_STATE_ON			2
#define	AUTO_STATE_MIN_OFF		3
#define	AUTO_STATE_OFF			4

#define	PD_NUMBER				8		// On compiling rule PD becomes P8

//--------------------------------------------------------------------------------
//	Bits values for dfStatus (status of the output channels)

#define	DF_TYPE_FORCE			0x20	// The channel is in forcing mode
#define	DF_TYPE_DIV				0x40	// The channel is in diverting mode
#define	DF_EMPTY				0x80	// The channel is unused
#define	DF_TYPE_MASK			0x60	// To extract the type
#define	DF_PRIO_MASK			0x0F	// To extract the priority
										// The index of the forcing (0 to FORCE_MAX-1) is also its priority
										// Lower number is higher priority

// The current status of diverting/forcing channels
// Each slot contains a DF_TYPE_xx flag and the index of the running diverting/forcing of this output
static	uint8_t		dfStatus [FORCE_CHAN_MAX] ;

static	void		forceIoOn	(forceRules_t * pForce) ;
static	void		forceIoOff	(forceRules_t * pForce) ;

//--------------------------------------------------------------------------------
//	Set/test flags bits

static inline bool forceFlagTest (forceRules_t * pForce, uint32_t flagBit)
{
	return ((pForce->flags & flagBit) != 0) ;
}

static inline void forceFlagSet (forceRules_t * pForce, uint32_t flagBit)
{
	pForce->flags |= flagBit ;
}

static inline void forceFlagClear (forceRules_t * pForce, uint32_t flagBit)
{
	pForce->flags &= ~flagBit ;
}

//--------------------------------------------------------------------------------
//	Set/test status bits

static inline bool forceStsTest (forceRules_t * pForce, uint32_t stsBit)
{
	return ((pForce->status & stsBit) != 0) ;
}

static inline void forceStsSet (forceRules_t * pForce, uint32_t stsBit)
{
	pForce->status |= stsBit ;
}

static inline void forceStsClear (forceRules_t * pForce, uint32_t stsBit)
{
	pForce->status &= ~stsBit ;
}

//--------------------------------------------------------------------------------
// Convert a rule from string to struct

static	bool	forceRuleCompile_ (forceRules_t * pForce, divRule_t * pRule, char * pText, uint32_t * pError)
{
	int32_t		number ;
	divExpr_t	* pExpr ;
	char		* pStr, * pItem, * pSaveExpr, * pSaveItem ;

	pRule->count = 0 ;
	aaStrToUpper (pText) ;
	pItem = strtok_r (pText, "&", & pSaveExpr) ;
	if (pItem == NULL)
	{
		// An empty string is forbidden
		* pError = 1 ;
		return false ;
	}

	while (1)
	{
		// Extract left expression argument
		pStr = strtok_r (pItem, " \t\r\n", & pSaveItem) ;
		if (pStr == NULL)
		{
			if (pRule->count == 0)
			{
				* pError = 2 ;	// A string with only separators is forbidden
				return false ;
			}
			* pError = 3 ;		// Separator not followed by an argument
			return false ;
		}

		// Check if this is an argument without operator and value
		if (* pStr == 'O')
		{
			// OUTx ON and OFF are not an expression
			if (pStr [1] == 'U')
			{
				// OUTx : define the output of the forcing
				number = pStr [3] - '0' ;
				if (number < 1 || number > FORCE_CHAN_MAX)
				{
					* pError = 4 ;	// It is not  OUT1 to OUT4
					return false ;
				}
				pForce->channel = number - 1 ;
			}
			else if (pStr [1] == 'N')
			{
				// Initial state is enabled
				forceFlagSet (pForce, FORCE_FLAG_ON) ;
			}
			else if (pStr [1] == 'F')
			{
				// Initial state is disabled
				forceFlagClear (pForce, FORCE_FLAG_ON) ;
			}
			else
			{
				* pError = 5 ;	// Unknown operator Oxxx
				return false ;
			}
		}
		else if (pStr[0] == 'A'  &&  pStr[1] == 'U')
		{
			// Mode AUTO: This is not an expression
			forceFlagSet   (pForce, FORCE_FLAG_MODE_AUTO) ;
			forceFlagClear (pForce, FORCE_FLAG_MODE_STD) ;
		}
		else if (pStr[0] == 'S'  &&  pStr[1] == 'T')
		{
			// Mode STD: This is not an expression
			forceFlagSet   (pForce, FORCE_FLAG_MODE_STD) ;
			forceFlagClear (pForce, FORCE_FLAG_MODE_AUTO) ;
		}
		else if (isdigit ((int) * pStr))
		{
			// Perhaps a ratio like 50% or 50%B
			char		* pEnd ;
			uint32_t	ratio ;

			ratio = strtoul (pStr, & pEnd, 0) ;
			if (* pEnd == '%')
			{
				if (ratio > 100)
				{
					* pError = 6 ;	// Ratio over 100%
					return false ;
				}
				if (pEnd [1] == 'B')
				{
					// This is a burst duty cycle: xx%B
					forceFlagSet (pForce, FORCE_FLAG_RATIOBURST) ;
					ratio = (FORCE_BURST_PERIOD * ratio) / 100 ;
				}
				else
				{
					// This is a normal duty cycle: xx%
					forceFlagSet (pForce, FORCE_FLAG_RATIO) ;
				}
				pForce->ratio = (uint8_t) ratio ;
			}
			else
			{
				* pError = 7 ;	// Unknown numeric parameter
				return false ;
			}
		}

		else	// This must be an expression with operator and value
		{
			if (* pStr == 'T'  ||  * pStr == 'I'  ||  * pStr == 'P'  ||  * pStr == 'V')
			{
				if (* pStr == 'P'  &&  pStr [1] == 'D')
				{
					pStr [1] = '1' + PD_NUMBER ;		// Replace 'PD' by 'P8' for diverted power
				}
				if (pStr [1] < '1'  ||  pStr [1] > '9' )
				{
					* pError = 8 ;	// n is invalid in Tn or In or Pn
					return false ;
				}
			}
			else if (pStr[0] == 'D'  &&  pStr[1] == 'M')
			{
				if (pStr [2] == 'A')
				{
					forceFlagSet (pForce, FORCE_FLAG_DMAX) ;
				}
				else if (pStr [2] == 'I')
				{
					forceFlagSet (pForce, FORCE_FLAG_DMIN) ;
				}
				else
				{
					* pError = 9 ; 	// Unknown parameter DMxx
					return false ;
				}
				pStr[0] = 'd' ;
			}
			else if (pStr [0] == 'A')
			{
				// AUTO mode parameter ?
				if (pStr [2] == 'F')
				{
					pStr [0] = 'f' ;	// AOFF
				}
				else if (pStr [1] == 'O')
				{
					pStr [0] = 'n' ;	// AON
				}
				else if (pStr [1] == 'K')
				{
					pStr [0] = 'k' ;	// AKF
				}
				else
				{
					* pError = 10 ; 	// Unknown parameter Axxx
					return false ;
				}
			}
			else if (pStr[0] == 'W'  &&  pStr[1] == 'D')
			{
				// Week Day, nothing to do
			}
			else if (pStr[0] != 'H'  ||  pStr[1] != 'M')
			{
				* pError = 11 ;
				return false ;	 	// Unknown parameter
			}

			// Process parameter with a digit in the name like T1, P2, V3...
			pExpr = & pRule->expr[pRule->count] ;
			number = pStr [1] - '0' ;
			switch (pStr [0])
			{
				case 'T':
					if (number < 1 || number > DIVE_TEMP_MAX)
					{
						* pError = 12 ;		// Invalid Tn number
						return false ;
					}
					number-- ;		// 1:4  becomes  0:3
					pExpr->tempHyst = 0 ;
					break ;

				case 'I':
					if (number < 1 || number > DIVE_INPUT_MAX)
					{
						* pError = 13 ;		// Invalid In number
						return false ;
					}
					number-- ;		// 1:4  becomes  0:3
					break ;

				case 'P':
					if ((number < 1 || number > DIVE_POWER_MAX)  &&  number != (PD_NUMBER+1))
					{
						* pError = 14 ;		// Invalid Pn number
						return false ;
					}
					number-- ;		// 1:4 becomes 0:3, and (PD_NUMBER+1) becomes PD_NUMBER
					break ;

				case 'V':
					if (number < 1 || number > AASUNVAR_MAX)
					{
						* pError = 15 ;		// Invalid Var number
						return false ;
					}
					number-- ;		// 1:4  becomes  0:3
					break ;

				default:
					number = 0 ;
					break ;
			}
			pExpr->type   = pStr [0] ;
			pExpr->number = number ;

			// Extract operator
			pStr = strtok_r (NULL, " \t\r\n", & pSaveItem) ;
			if (pStr == NULL ||  pStr [1] != 0)
			{
				* pError = 16 ;		// Operator not found
				return false ;
			}
			// Operator: only < > = # are allowed,
			// = not allowed for T and P
			// # only allowed for V
			if ((* pStr != '<'  &&  * pStr != '>'  &&  * pStr != '='  &&  * pStr != '#') ||
				(* pStr == '='  &&  (pExpr->type == 'T'  ||  pExpr->type == 'P'))  ||
				(* pStr == '#'  &&  pExpr->type != 'V'))
			{
				* pError = 17 ;
				return false ;
			}
			pExpr->comp = * pStr ;
			if (pExpr->type == 'D')
			{
				pExpr->comp = '=' ;
			}

			// Extract the value
			pStr = strtok_r (NULL, " \t\r\n", & pSaveItem) ;
			if (pStr == NULL)
			{
				* pError = 18 ;		// Value not found
				return false ;
			}
			if (pExpr->type == 'W')
			{
				// Special value for Week Day: a string of 7 chars
				int32_t value = 0 ;
				for (int32_t ii = 0 ; ii < 7 ; ii++)
				{
					if (* pStr == 0)
					{
						* pError = 19 ;		// Bad Week Day string
						return false ;
					}
					if (* pStr != '.')
					{
						value |=  0x80 ;	// If '.' ignore this day, else keep it
					}
					pStr++ ;
					value >>= 1 ;
				}
				pExpr->value = value ;		// Bit0=Sunday, bit1= Monday...
aaPrintf ("WD %X\n", value) ;
			}
			else
			{
				// Numeric value
				pExpr->value = strtol (pStr, & pStr, 0) ;
				if (pExpr->type != 'd'  &&  * pStr != 0)
				{
					* pError = 20 ;		// Bad char at the end of the value (except DMIN/DMAX which allows m)
					return false ;
				}
			}

			// The rule is good

			// pStr is a pointer to the char following the value
			if (0 != islower (pRule->expr [pRule->count].type))
			{
				// This is not an expression, set the parameter from the value
				if (* pStr ==  'M')
				{
					pExpr->value *= 60 ;	// The value is expressed in minutes
				}
				switch (pExpr->type)
				{
					case 'd':
						// DMIN or DMAX delay for this rule
						pForce->delay = pExpr->value ;
						break ;

					case 'n':
						pForce->autoOn = pExpr->value ;
						break ;

					case 'f':
						pForce->autoOff = pExpr->value ;
						break ;

					case 'k':
						pForce->autoKf = pExpr->value ;
						break ;

					default:
						break ;
				}
			}
			else
			{
				// This is a valid expression
				pRule->count++ ;
			}
		}

		// Extract next expression
		pItem = strtok_r (NULL, "&", & pSaveExpr) ;
		if (pItem == NULL)
		{
			break ;	// End of line
		}
		if (pRule->count == DIVE_EXPR_MAX)
		{
			* pError = 21 ;	// Too many expression in the rule
			return false ;
		}
	}

	if ((pForce->flags & FORCE_FLAG_MODE_MASK) == 0)
	{
		* pError = 22 ; 	// Missing mode: AUTO or STD
		return false ;
	}

	if ((pForce->flags & FORCE_FLAG_D_MASK) == FORCE_FLAG_D_MASK)
	{
		* pError = 23 ; 	// DMIN and DMAX are exclusive
		return false ;
	}
	return true ;
}

//--------------------------------------------------------------------------------
// Convert a string version of the rule to a forceRules_t structure
// On fail: returns false and an error number in pError. Error < 100 in start rule, error > 100 in stop rule
// BEWARE: the string pointed to by pStrStart/pStrStop is modified: can't be in flash

bool	forceRuleCompile (forceRules_t * pForce, char * pStrStart, char * pStrStop, uint32_t * pError)
{
	pForce->status    = 0 ;				// Default is invalid
	pForce->flags     = FORCE_FLAG_ON ;	// Default is ON
	pForce->channel   = 0xFF ;			// To spot a lack of OUTx parameter
	pForce->autoState = AUTO_STATE_IDLE ;

	pForce->autoOn    = FORCE_AUTO_ON ;		// AUTO mode default values
	pForce->autoOff   = FORCE_AUTO_OFF ;
	pForce->autoKf    = FORCE_AUTO_KF ;

	* pError = 0 ;
	if (! forceRuleCompile_ (pForce, & pForce->startRule, pStrStart, pError))
	{
		pForce->status = 0 ;	// invalid => check will always return false
		return false ;
	}
	if (pForce->channel == 0xFF)
	{
		* pError = 21 ;
		return false ; 	// Missing OUTx
	}

	if (! forceRuleCompile_ (pForce, & pForce->stopRule, pStrStop, pError))
	{
		pForce->status = 0 ;	// invalid => check will always return false
		pError += 100 ;			// Error in stop rule
		return false ;
	}
	forceStsSet (pForce, FORCE_STS_VALID) ;
	return true ;
}

//--------------------------------------------------------------------------------
//	Returns a string version of the rule
//	if bStart == true then start rule else stop rule
//	It is mandatory that ON or OFF be the 1st word in the string

uint32_t	forceRulePrint (forceRules_t * pForce, bool bStart, char * pText, uint32_t size)
{
	uint32_t	len, ii ;
	int32_t		value ;
	divRule_t	* pRule ;
	char		* pName ;
	char		name [8] ;

	len = 0 ;
	* pText = 0 ;

	if (! forceStsTest (pForce, FORCE_STS_VALID))
	{
		* pText = 0 ;
		return 0 ;	// Invalid rule
	}

	if (bStart)
	{
		pRule = & pForce->startRule ;
		len += aaSnPrintf (pText+len, size - len, "%s", forceFlagTest (pForce, FORCE_FLAG_ON)   ? "ON  &  " : "OFF  &  ") ;
		len += aaSnPrintf (pText+len, size - len, "OUT%d  &  ", pForce->channel + 1) ;
		len += aaSnPrintf (pText+len, size - len, "%s", forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO) ? "AUTO  &  " : "STD  &  ") ;
		if (forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO))
		{
			len += aaSnPrintf (pText+len, size - len, "AON  = %u  &  ",  pForce->autoOn) ;
			len += aaSnPrintf (pText+len, size - len, "AOFF = %u  &  ", pForce->autoOff) ;
			len += aaSnPrintf (pText+len, size - len, "AKF  = %u  &  ", pForce->autoKf) ;
		}
		if (forceFlagTest (pForce, FORCE_FLAG_RATIO))
		{
			len += aaSnPrintf (pText+len, size - len, "%u%%%s  &  ",
					pForce->ratio, forceFlagTest (pForce, FORCE_FLAG_RATIOBURST) ? "B " : " ") ;
		}
	}
	else
	{
		// The delay value of DMIN / DMAX may be expressed in minute
		if (forceFlagTest (pForce, FORCE_FLAG_DMAX | FORCE_FLAG_DMIN))
		{
			uint32_t	delay ;
			char		* pQual ;

			delay = pForce->delay / 60 ;
			if (delay * 60 == pForce->delay)
			{
				// Delay expressed in minute
				pQual = "m" ;
			}
			else
			{
				delay = pForce->delay ;
				pQual = "" ;
			}

			if (forceFlagTest (pForce, FORCE_FLAG_DMAX))
			{
				len += aaSnPrintf (pText+len, size - len, "DMAX = %d%s  &  ", delay, pQual) ;
			}
			if (forceFlagTest (pForce, FORCE_FLAG_DMIN))
			{
				len += aaSnPrintf (pText+len, size - len, "DMIN = %d%s  &  ", delay, pQual) ;
			}
		}
		pRule = & pForce->stopRule ;
	}

	for (ii = 0 ; ii < pRule->count  &&  len < size ; ii++)
	{
		switch (pRule->expr[ii].type)
		{
			case 'W':		// Week day
				pName = "" ;		// Avoid compiler warning
				value = pRule->expr[ii].value ;
				strcpy (pText+len, "WD = ") ; len += 5 ;
				for (int32_t jj = 0 ; jj < 7 ; jj++)
				{
					pText [len++] = ((value & (1 << jj)) == 0) ? '.' : 'X' ;
				}
				strcpy (pText+len, "  &  ") ; len += 5 ;
				break ;

			case 'H':		// Hour Minute
				pName = "HM" ;
				break ;

			default:	// T P I V
				if (pRule->expr [ii].number == 8)	// Not 9, remember number-- in forceRuleCompile_!
				{
					pName = "PD" ;		// Power diverted
				}
				else
				{
					name [0] = pRule->expr[ii].type ;
					name [1] = '1' + pRule->expr[ii].number ;
					name [2] = 0 ;
					pName = name ;
				}
				break ;
		}
		if (pRule->expr[ii].type != 'W')
		{
			len += aaSnPrintf (pText+len, size - len, "%s %c %d  &  ",
								pName,
								pRule->expr[ii].comp,
								pRule->expr[ii].value) ;
		}
	}
	// Remove the 5 last chars: "  &  "
	len -= 5 ;
	pText [len] = 0 ;

	return len ;
}

//--------------------------------------------------------------------------------

bool		forceRuleIsValid		(forceRules_t * pForce)
{
	return forceStsTest (pForce, FORCE_STS_VALID) ;
}

//--------------------------------------------------------------------------------

static	bool	forceRuleCheck_		(forceRules_t * pForce, bool bStart)
{
	divExpr_t		* pExpr ;
	divRule_t		* pRule ;
	uint32_t			ii ;

	if (! forceStsTest (pForce, FORCE_STS_VALID))
	{
		return false ;		// Invalid forcing descriptor
	}

	if (bStart)
	{
		if (! forceFlagTest (pForce, FORCE_FLAG_ON))
		{
			return false ;	// This forcing channel is inhibited
		}
		pRule = & pForce->startRule ;
	}
	else
	{
		pRule = & pForce->stopRule ;
	}

	// Delay processing, only for stop rule
	if (! bStart)
	{
		if (forceFlagTest (pForce, FORCE_FLAG_D_MASK)  &&  pForce->delayCount != 0u)
		{
			pForce->delayCount-- ;
			if (pForce->delayCount == 0)
			{
				if (forceFlagTest (pForce, FORCE_FLAG_DMAX))
				{
					return true ;	// Maximum deadlines reached
				}
			}
			if (forceFlagTest (pForce, FORCE_FLAG_DMIN) && pForce->delayCount != 0u)
			{
				return false ;		// Minimum deadlines not reached
			}
		}

		if (forceFlagTest (pForce, FORCE_FLAG_RATIOBURST))
		{
			pForce->counter-- ;
			if (pForce->counter == 0)
			{
				// Switch phase
				if (forceStsTest (pForce, FORCE_STS_BURST_ON))
				{
					if (pForce->ratio == FORCE_BURST_PERIOD)
					{
						// Ratio is 100% so stay in On phase
						pForce->counter = pForce->ratio ;
					}
					else
					{
						// Go to I/O Off phase
						forceIoOff (pForce) ;
						forceStsClear (pForce, FORCE_STS_BURST_ON) ;
						pForce->counter = FORCE_BURST_PERIOD - pForce->ratio ;
					}
				}
				else
				{
					// Go to I/O On phase
					forceIoOn (pForce) ;
					forceStsSet (pForce, FORCE_STS_BURST_ON) ;
					pForce->counter = pForce->ratio ;
				}
			}
		}
	}

	if (pRule->count == 0)
	{
		return false ;		// No rule: always false
	}
	for (ii  = 0 ; ii < pRule->count ; ii++)
	{
		pExpr = & pRule->expr [ii] ;
		switch (pExpr->type)
		{
			case 'T':
				{
					int32_t			temperature ;
					int32_t			number ;

					// Get the rank of the T sensor
					if (! tsGetTemp (pExpr->number, & temperature))
					{
						return false ;	// Can't get temperature from this TS
					}
//iFracPrint (temperature, TEMP_SENSOR_SHIFT, 4, 1) ; aaPutChar (' ') ;
					temperature >>= TEMP_SENSOR_SHIFT ;
					number = pExpr->value ;
					if (pRule->expr [ii].comp == '<')
					{
						if (! (temperature < number + pExpr->tempHyst))
						{
							pExpr->tempHyst = -DIVE_TEMP_HYST ;
//aaPrintf (" > %3d %2d ", temperature, pExpr->tempHyst) ;
							return false ;
						}
					}
					else
					{
						// Use >= to set the temperature number in the true range
						if (! (temperature >= number + pExpr->tempHyst))
						{
							pExpr->tempHyst = +DIVE_TEMP_HYST ;
//aaPrintf (" > %3d %2d ", temperature, pExpr->tempHyst) ;
							return false ;
						}
					}
					pExpr->tempHyst = 0 ;
//aaPrintf (" < %3d %2d ", temperature, pExpr->tempHyst) ;
					// Test is OK
				}
				break ;

			case 'I':
				{
					uint32_t		value ;

					if (inputGet (pExpr->number, & value))
					{
						if ((int32_t) value != pExpr->value)
						{
							return false ;
						}
					}
					else
					{
						return false ;	// Invalid input number
					}
					// Test is OK
				}
				break ;

			case 'P':
				{
					int32_t			value ;
					int32_t			power ;

					if (pExpr->number == PD_NUMBER)
					{
						// This is for estimated diverted power
						power = computedData.powerDiverted >> POWER_DIVERTER_SHIFT ;
					}
					else
					{
						// This is for CT power
						power = computedData.iData [pExpr->number].powerReal >> POWER_SHIFT ;
					}
					value = pExpr->value ;
					if (pExpr->comp == '<')
					{
//aaPrintf ("%5d < %5d ", power, value) ;
						if (power >= value)
						{
							return false ;
						}
					}
					else
					{
//aaPrintf ("%5d > %5d ", power, value) ;
						if (power <= value)
						{
							return false ;
						}
					}
				}
				break ;

			case 'V':
				{
					int32_t		value ;
					int32_t		var ;

					var   = aaSunVariable [pExpr->number] ;
					value = pExpr->value ;
					if (pExpr->comp == '=')
					{
						if (var != value)
						{
							return false ;
						}
					}
					else if (pExpr->comp == '#')
					{
						if (var == value)
						{
							return false ;
						}
					}
					else if (pExpr->comp == '<')
					{
						if (var >= value)
						{
							return false ;
						}
					}
					else
					{
						if (var <= value)
						{
							return false ;
						}
					}
					// Test is OK
				}
				break ;

			case 'H':
				{
					int32_t	time = (localTime.hh * 100) + localTime.mm ;
					switch (pExpr->comp)
					{
						case '<':
							if (time >= pExpr->value)
							{
								return false ;
							}
							break ;

						case '>':
							if (time <= pExpr->value)
							{
								return false ;
							}
							break ;

						default:
						case '=':
							if (time != pExpr->value)
							{
								return false ;
							}
							break ;
					}
				}
				break ;

			case 'W':
				if ((pExpr->value & (1 << localTime.wd)) == 0)
				{
					return false ;
				}
				break ;

			default:
				AA_ASSERT (0) ;
				break ;
		}
	}

	return true ;
}

// Returns true if the rule evaluation is true

bool	forceRuleCheck		(forceRules_t * pForce, bool bStart)
{
	bool	bOk ;

	// STD mode: check the rule only if it matches the current state
	// AUTO mode: always check the rule (once started the auto forcing is always running)
	if (forceFlagTest (pForce, FORCE_FLAG_MODE_STD)  &&  forceStsTest (pForce, FORCE_STS_RUNNING) == bStart)
	{
		return false ;		// Either start and already running, or stop and already stopped
	}

	bOk = forceRuleCheck_ (pForce, bStart) ;

	// DMAX transition state processing
	if (bStart  &&  forceStsTest (pForce, FORCE_STS_DMAX_WAITING))
	{
		if (bOk)
		{
			bOk = false ;	// Ignore the true value until the rule is false
		}
		else
		{
			// Start rule is false : End of transition state
			forceStsClear (pForce, FORCE_STS_DMAX_WAITING) ;
		}
	}

	// Execute the manual order
	if (forceStsTest (pForce, FORCE_STS_MANUAL_ORDER))
	{
		if (forceStsTest (pForce, FORCE_STS_RUNNING)  &&  ! bStart)
		{
			// Running and stop rule : set the stop rule true to stop the forcing
			bOk = true ;
			if (! forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO))
			{
				// Do not clear the manual order for AUTO mode here.
				// For AUTO mode when the stop rule is true, the forcing doesn't stop but change state.
				forceStsClear (pForce, FORCE_STS_MANUAL_ORDER) ;
			}
		}
		else if (! forceStsTest (pForce, FORCE_STS_RUNNING)  &&  bStart)
		{
			// Not running and start rule : set the start rule true to start the forcing
			bOk = true ;
			forceStsClear (pForce, FORCE_STS_MANUAL_ORDER) ;
		}
		else
		{
			// Nothing
		}
	}
	return bOk ;
}

//--------------------------------------------------------------------------------
//	Called every time the start rule is true

static	void	forceAutoStart (forceRules_t * pForce)
{
	if (pForce->autoState == AUTO_STATE_IDLE)
	{
		if (forceStsTest (pForce, FORCE_STS_START_RULE))
		{
			// The start rule is true, so we go to MIN_ON state
			// We assume that the start condition will last, so we put the full autoOn and autoKf delays
			pForce->autoState = AUTO_STATE_MIN_ON ;
			pForce->counter   = pForce->autoOn ;
			pForce->kfCounter = pForce->autoKf ;
			forceIoOn (pForce) ;
//aaPuts ("AUTO_STATE_MIN_ON\n") ;
		}
		else
		{
			// The start rule is false, so we go to OFF state
			// with full autoKf to be immune to fast changes
			pForce->autoState = AUTO_STATE_OFF ;
			pForce->kfCounter = pForce->autoKf ;
//aaPuts ("AUTO_STATE_OFF\n") ;
		}
	}
}

//--------------------------------------------------------------------------------
// Called only when the forcing is preempted or invalidated

static	void	forceAutoStop (forceRules_t * pForce)
{
	forceIoOff (pForce) ;
	pForce->autoState = AUTO_STATE_IDLE ;
}

//--------------------------------------------------------------------------------
//	Called after start and stop rules have been evaluated and the forcing is running

static void kfCount (forceRules_t * pForce, bool bStart)
{
	bool ok = bStart ? forceStsTest (pForce, FORCE_STS_START_RULE) : forceStsTest (pForce, FORCE_STS_STOP_RULE) ;

	if (ok)
	{
		// The is true
		if (pForce->kfCounter != 0)
		{
			pForce->kfCounter-- ;
		}
	}
	else
	{
		if (pForce->kfCounter < pForce->autoKf)
		{
			pForce->kfCounter++ ;
		}
	}
}

static	void	forceAutoNext (forceRules_t * pForce)
{
	switch (pForce->autoState)
	{
		case AUTO_STATE_MIN_ON:
			kfCount (pForce, false) ;
			pForce->counter-- ;
			if (pForce->counter == 0)
			{
				// End of minimum time on, switch to ON state
				pForce->autoState = AUTO_STATE_ON ;
//aaPuts ("AUTO_STATE_ON\n") ;
			}
			break ;

		case AUTO_STATE_ON:
			kfCount (pForce, false) ;
			if (pForce->kfCounter == 0)
			{
				// Switch to MIN OFF state
				pForce->autoState = AUTO_STATE_MIN_OFF ;
				pForce->counter   = pForce->autoOff ;
				pForce->kfCounter = pForce->autoKf ;
				forceIoOff (pForce) ;
//aaPuts ("AUTO_STATE_MIN_OFF\n") ;
			}
			break ;

		case AUTO_STATE_MIN_OFF:
			kfCount (pForce, true) ;
			pForce->counter-- ;
			if (pForce->counter == 0)
			{
				// End of minimum time off, switch to OFF state
				pForce->autoState = AUTO_STATE_OFF ;
//aaPuts ("AUTO_STATE_OFF\n") ;
			}
			break ;

		case AUTO_STATE_OFF:
			kfCount (pForce, true) ;
			if (pForce->kfCounter == 0)
			{
				// Switch to ON state
				pForce->autoState = AUTO_STATE_MIN_ON ;
				pForce->counter   = pForce->autoOn ;
				pForce->kfCounter = pForce->autoKf ;
				forceIoOn (pForce) ;
//aaPuts ("AUTO_STATE_MIN_ON\n") ;
			}
			break ;

		default:
			break ;
	}
}

//--------------------------------------------------------------------------------
//	Set ON the I/O managed by the forcing

static	void	forceIoOn (forceRules_t * pForce)
{
	uint32_t	ratio ;

	switch (pForce->channel)
	{
		case 0:			// OUT1 : SSR1
			if (forceFlagTest (pForce, FORCE_FLAG_RATIO))
			{
				ratio = pForce->ratio ;
aaPrintf ("TIMSSR 1 Start %u%% ", ratio) ;
				ratio = ((511 * ratio) + 50) / 100 ;	// index
				if (ratio > 511)
				{
				}
aaPrintf ("%u\n ", ratio) ;
				timerOutputChannelSet (TIMSSR, powerDiv [0].ssrChannel, p2Delay [ratio]) ;	// Full power
			}
			else
			{
				timerOutputChannelSet (TIMSSR, powerDiv [0].ssrChannel, 0) ;	// ON
aaPrintf ("TIMSSR 1 Start\n") ;
			}
			break ;

		case 1:			// OUT2 : SSR2
			if (forceFlagTest (pForce, FORCE_FLAG_RATIO))
			{
				ratio = pForce->ratio ;
aaPrintf ("TIMSSR 2 Start %u%% ", ratio) ;
				ratio = ((511 * ratio) + 50) / 100 ;	// index
				if (ratio > 511)
				{
					ratio = 511 ;
				}
aaPrintf ("%u\n ", ratio) ;
				timerOutputChannelSet (TIMSSR, powerDiv [1].ssrChannel, p2Delay [ratio]) ;	// Full power
			}
			else
			{
				timerOutputChannelSet (TIMSSR, powerDiv [1].ssrChannel, 0) ;	// ON
aaPrintf ("TIMSSR 2 Start\n") ;
			}
			break ;

		case 2:			// OUT3 : On board relay
			outputSet (IO_OUT3, 1) ;
			break ;

		case 3:			// OUT4 : Output on expansion connector
			outputSet (IO_OUT4, 1) ;
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//--------------------------------------------------------------------------------
//	Set OFF the I/O managed by the forcing

static	void	forceIoOff (forceRules_t * pForce)
{
	switch (pForce->channel)
	{
		case 0:			// SSR1
			timerOutputChannelSet (TIMSSR, powerDiv [0].ssrChannel, p2Delay [0]) ;
aaPrintf ("TIMSSR 1 Stop\n") ;
			break ;

		case 1:			// SSR2
			timerOutputChannelSet (TIMSSR, powerDiv [1].ssrChannel, p2Delay [0]) ;
aaPrintf ("TIMSSR 2 Stop\n") ;
			break ;

		case 2:			// Relay
			outputSet (IO_OUT3, 0) ;
			break ;

		case 3:			// Expansion
			outputSet (IO_OUT4, 0) ;
			break ;

		default:
			AA_ASSERT (0) ;
			break ;
	}
}

//--------------------------------------------------------------------------------
//	STD mode:  Called when the stop rule is true
//	Auto mode: Called only when the forcing is preempted or invalidated

static	void	forceStop		(forceRules_t * pForce)
{
	if (forceStsTest (pForce, FORCE_STS_RUNNING))
	{
aaPrintf ("forceStop %d\n", pForce - aaSunCfg.forceRules) ;
		if (forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO))
		{
			forceAutoStop (pForce) ;
		}
		else
		{
			forceIoOff (pForce) ;
			if (forceFlagTest (pForce, FORCE_FLAG_DMAX))
			{
				// End of DMAX: go to intermediate state until the start rule is false
				// to avoid to restart immediately the forcing if the start rule is still true
				forceStsSet (pForce, FORCE_STS_DMAX_WAITING) ;
			}
		}
		pForce->status &= ~FORCE_STS_RUNNING ;	// This forcing is stopped
	}
}

//--------------------------------------------------------------------------------
//	The start rule is true, then start the forcing

static	void	forceStart		(forceRules_t * pForce)
{
	if (forceStsTest (pForce, FORCE_STS_RUNNING) == 0)
	{
aaPrintf ("forceStart %d\n", pForce - aaSunCfg.forceRules) ;
		if (forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO))
		{
			forceAutoStart (pForce) ;
		}
		else
		{
			forceIoOn (pForce) ;

			pForce->delayCount = pForce->delay ;	// For DMIN and DMAX delay processing
			if (forceFlagTest (pForce, FORCE_FLAG_RATIOBURST))
			{
				// Initialize the burst counter
				forceStsSet (pForce, FORCE_STS_BURST_ON) ;	// In I/O On phase
				pForce->counter = pForce->ratio ;
			}
		}
		pForce->status |= FORCE_STS_RUNNING ;
	}
}

//--------------------------------------------------------------------------------
//	Stop the forcing and remove it from dfStatus

static	void	forceRemove (forceRules_t * pForce)
{
	uint32_t	ii ;
	uint8_t		sts ;

	if (forceStsTest (pForce, FORCE_STS_RUNNING))
	{
		// Stop
		forceStop (pForce) ;

		// Remove from dfStatus
		sts = DF_TYPE_FORCE + (uint8_t)(pForce - aaSunCfg.forceRules) ;
		for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
		{
			if (dfStatus [ii] == sts)
			{
				dfStatus [ii] = DF_EMPTY ;
			}
		}
	}
}

// This temporarily stops all forcing (until the next diverterNext())

void	forceRuleRemoveAll (void)
{
	forceRules_t	* pForce = aaSunCfg.forceRules ;
	uint32_t		ii ;

	for (ii = 0 ; ii < FORCE_MAX ; ii++, pForce++)
	{
		forceRemove (pForce) ;
	}
}

//--------------------------------------------------------------------------------
// Set or clear the enable flag of the forcing

void	forceRuleEnable (forceRules_t * pForce, bool bEnable)
{
	if (forceStsTest (pForce, FORCE_STS_VALID))
	{
		if (bEnable)
		{
			// Enable this forcing
			forceFlagSet (pForce, FORCE_FLAG_ON) ;
		}
		else
		{
			// Disable this forcing
			forceRemove (pForce) ;
			forceFlagClear (pForce, FORCE_FLAG_ON) ;
		}
	}
}

//--------------------------------------------------------------------------------

void	forceRuleInvalidate (forceRules_t * pForce)
{
	if (forceStsTest (pForce, FORCE_STS_VALID))
	{
		forceRemove (pForce) ;
		forceStsClear (pForce, FORCE_STS_VALID) ;
	}
}


void	forceRuleInvalidateAll (void)
{
	forceRules_t	* pForce = aaSunCfg.forceRules ;
	uint32_t		ii ;

	for (ii = 0 ; ii < FORCE_MAX ; ii++, pForce++)
	{
		forceRuleInvalidate (pForce) ;
	}
}

//--------------------------------------------------------------------------------
// The HTTP server ask for diverting/forcing active on every output

uint32_t	forceJsonStatus (char * pStr, uint32_t maxLen)
{
	uint8_t		tempStatus [FORCE_CHAN_MAX] ;
	uint32_t	len ;
	uint32_t	ii ;
	uint32_t	type ;

	bspDisableIrq () ;		// Because the rules are used by AASun task
	memcpy (tempStatus, dfStatus, sizeof (tempStatus)) ;
	bspEnableIrq () ;

	pStr [0] = '{' ;
	len = 1u ;
	for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
	{
		len += aaSnPrintf (pStr + len, maxLen - len, "\"out%u\":", ii) ;
		type = tempStatus[ii] & DF_TYPE_MASK ;
		len += aaSnPrintf (pStr + len, maxLen - len, "\"%s %u\",",
				type == DF_TYPE_FORCE ? "Forcing" :
				type == DF_TYPE_DIV   ? "Diverting" : "-",
				(tempStatus[ii] & DF_PRIO_MASK)) ;
	}
	pStr [len-1] = '}' ;	// Replace the last ',' with '}'
	return len ;
}

//--------------------------------------------------------------------------------
// Set the manual order for the forcing rule at index
// Index is 0 based

bool	forceSetManualOrder (uint32_t index)
{
	bspDisableIrq () ;		// Because the rules are used by AASun task
	forceStsSet (& aaSunCfg.forceRules [index], FORCE_STS_MANUAL_ORDER) ;
	bspEnableIrq () ;
	return true ;
}

//--------------------------------------------------------------------------------
//	Updates the diverting/forcing by evaluating the rules

#define		FORCE_RULE_STOP		false
#define		FORCE_RULE_START	true

void	diverterNext (void)
{
	forceRules_t	* pForce ;
	uint8_t			tempStatus [FORCE_CHAN_MAX] ;
	uint8_t			sts ;
	uint32_t		ii ;

	memcpy (tempStatus, dfStatus, sizeof (tempStatus)) ;

	// 1) Clear the start/stop bits in forcing status
	pForce = aaSunCfg.forceRules ;
	for (ii = 0 ; ii < FORCE_MAX ; ii++, pForce++)
	{
		pForce->status &= ~(FORCE_STS_STOP_RULE | FORCE_STS_START_RULE) ;
	}

	// 2) If some forcing have true stop rule then stop them
	for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
	{
		if ((tempStatus [ii] & DF_TYPE_MASK) == DF_TYPE_FORCE)
		{
			pForce = & aaSunCfg.forceRules [tempStatus [ii] & DF_PRIO_MASK] ;
			if (forceRuleCheck (pForce, FORCE_RULE_STOP))
			{
				// The stop rule is true
				forceStsSet (pForce, FORCE_STS_STOP_RULE) ;
				if (forceFlagTest (pForce, FORCE_FLAG_MODE_STD))
				{
					// The stop rule of this STD forcing is true: stop it
					forceStop (pForce) ;
					tempStatus [ii] = DF_EMPTY ;
					dfStatus   [ii] = DF_EMPTY ;
				}
				if (forceStsTest (pForce, FORCE_STS_MANUAL_ORDER)  &&  forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO))
				{
					// For AUTO mode when the FORCE_STS_MANUAL_ORDER is present,
					// the stop rule doesn't change the state of the forcing but stop it
					forceStsClear (pForce, FORCE_STS_MANUAL_ORDER) ;
					forceStop (pForce) ;
					tempStatus [ii] = DF_EMPTY ;
					dfStatus   [ii] = DF_EMPTY ;
				}
			}
		}
	}

	// 3) Evaluate start rule for all the forcing
	pForce = aaSunCfg.forceRules ;
	for (ii = 0 ; ii < FORCE_MAX ; ii++, pForce++)
	{
		if (forceRuleCheck (pForce, FORCE_RULE_START))
		{
			// The start rule of this forcing is true
			forceStsSet (pForce, FORCE_STS_START_RULE) ;

			// Override forcing only if it has lower number (higher priority)
			sts = DF_TYPE_FORCE | ii ;
			if (sts < tempStatus [pForce->channel])
			{
				tempStatus [pForce->channel] = sts ;
			}
		}
	}

	// Now we have the new status for all forcing channels

	// 4) Evaluate the diverting rules (diverting have lower priority than forcing)
	ii = DIV_SWITCH_NONE ;
	if ((tempStatus [0] & DF_TYPE_MASK) != DF_TYPE_FORCE)
	{
		// Not used for forcing then check for diverting
		tempStatus [0] = DF_EMPTY ;
		if (divRuleCheck (& aaSunCfg.diverterRule [0]))
		{
			tempStatus [0] = DF_TYPE_DIV ;
			ii = DIV_SWITCH_CHAN1 ;
		}
	}
	if ((tempStatus [1] & DF_TYPE_MASK) != DF_TYPE_FORCE)
	{
		tempStatus [1] = DF_EMPTY ;
		if (ii == DIV_SWITCH_NONE  &&  divRuleCheck (& aaSunCfg.diverterRule [1]))
		{
			tempStatus [1] = DF_TYPE_DIV + 1 ;
			ii = DIV_SWITCH_CHAN2 ;
		}
	}
	if (ii != diverterChannel)
	{
		// Diverting channels have changed
		diverterSwitch  = ii ;
		// Wait for the meterTask to change the diverting
		while (diverterSwitch != DIV_SWITCH_IDLE)
		{
			aaTaskDelay (1) ;
		}
	}

	// Now we have the new status for all forcing and diverting channels

	// 5) Stop channels preempted by a higher priority forcing
	for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
	{
		if ((tempStatus [ii] & dfStatus [ii] & DF_TYPE_MASK) != 0)
		{
			if (tempStatus [ii] < dfStatus [ii])
			{
				forceStop (& aaSunCfg.forceRules [dfStatus [ii] & DF_PRIO_MASK]) ;
			}
		}
	}

	// 6) Start new ready forcing channels
	for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
	{
		if ((tempStatus [ii] & DF_TYPE_MASK) == DF_TYPE_FORCE)
		{
			pForce = & aaSunCfg.forceRules [tempStatus [ii] & DF_PRIO_MASK] ;
			forceStart (pForce) ;	// Starts only not already running forcing

			// AUTO mode: evaluate next step using start/stop rules result
			if (forceFlagTest (pForce, FORCE_FLAG_MODE_AUTO) && forceStsTest (pForce, FORCE_STS_RUNNING))
			{
				forceAutoNext (pForce) ;
			}
		}
	}

	memcpy (dfStatus, tempStatus, sizeof (tempStatus)) ;
}

//--------------------------------------------------------------------------------
//	Initialize the forcing before starting

void	forceInit (void)
{
	forceRules_t	* pForce ;
	uint32_t		ii ;

	for (ii = 0 ; ii < FORCE_CHAN_MAX ; ii++)
	{
		dfStatus [ii] = DF_EMPTY ;
	}

	pForce = aaSunCfg.forceRules ;
	for (ii = 0 ; ii < FORCE_MAX ; ii++, pForce++)
	{
		pForce->autoState = AUTO_STATE_IDLE ;
		pForce->status   &= FORCE_STS_VALID ;
	}
}

//--------------------------------------------------------------------------------
/* Some tests

AUTO
cfr 2 out2 & auto  & Aon = 4 & aoff = 4 & i3 = 1 / i3 = 0

burst
cfr 2 out2 & std & 50%b & i3 = 1 / i3 = 0

STD
cfr 2 OUT2  &  ON  &  STD  &  I3 = 1 / I3 = 0

*/
