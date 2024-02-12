/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	bspboard.c	I/O specific routines for NUCLEO-144 boards
	When		Who	What
	10/06/17	ac	Creation
	13/05/20	ac	Add BSP_xxcx_ATTR

----------------------------------------------------------------------
*/
/*
----------------------------------------------------------------------
	Copyright (c) 2013-2023 Alain Chebrou - AdAstra-Soft . com
	All rights reserved.

	This file is part of the AdAstra Real Time Kernel distribution.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.

	- Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.

	- Neither the name of the copyright holder nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----------------------------------------------------------------------
*/

#include	"aa.h"
#include	"gpiobasic.h"
#include	"bsp.h"

//--------------------------------------------------------------------------------
//	Configure LEDs, button and test pin I/O
//	This allows a sufficient number of resources in most cases. You are free to add any.

// Theses declarations uses defines from bspboard.h
STATIC	const gpioPinDesc_t	bspBoardPinDesc [] =
{
	// LEDS
#if (defined BSP_LED0)
	{ 'A'+BSP_LED0_PORT_NUMBER,		BSP_LED0_PIN_NUMBER,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW
	#if (defined BSP_LED0_ATTR)
			| BSP_LED0_ATTR
	#endif
	},
#endif
#if (defined BSP_LED1)
	{ 'A'+BSP_LED1_PORT_NUMBER,		BSP_LED1_PIN_NUMBER,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW
	#if (defined BSP_LED1_ATTR)
			| BSP_LED1_ATTR
	#endif
	},
#endif
#if (defined BSP_LED2)
	{ 'A'+BSP_LED2_PORT_NUMBER,		BSP_LED2_PIN_NUMBER,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW
	#if (defined BSP_LED2_ATTR)
			| BSP_LED2_ATTR
	#endif
	},
#endif
#if (defined BSP_LED3)
	{ 'A'+BSP_LED3_PORT_NUMBER,		BSP_LED3_PIN_NUMBER,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW
	#if (defined BSP_LED3_ATTR)
			| BSP_LED3_ATTR
	#endif
	},
#endif
#if (defined BSP_LED4)
	{ 'A'+BSP_LED4_PORT_NUMBER,		BSP_LED4_PIN_NUMBER,	0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_LOW
	#if (defined BSP_LED4_ATTR)
			| BSP_LED4_ATTR
	#endif
	},
#endif
#if (defined BSP_LED5)
	#error "Too many BSP_LEDx"
#endif

	// Buttons
#if (defined BSP_BUTTON0)
	{ 'A'+BSP_BUTTON0_PORT_NUMBER, BSP_BUTTON0_PIN_NUMBER, 0, AA_GPIO_MODE_INPUT  | AA_GPIO_SPEED_LOW
	#if (defined BSP_BUTTON0_ATTR)
			| BSP_BUTTON0_ATTR
	#endif
	},
#endif
#if (defined BSP_BUTTON1)
	{ 'A'+BSP_BUTTON1_PORT_NUMBER, BSP_BUTTON1_PIN_NUMBER, 0, AA_GPIO_MODE_INPUT  | AA_GPIO_SPEED_LOW
	#if (defined BSP_BUTTON1_ATTR)
			| BSP_BUTTON1_ATTR
	#endif
	},
#endif
#if (defined BSP_BUTTON2)
	{ 'A'+BSP_BUTTON2_PORT_NUMBER, BSP_BUTTON2_PIN_NUMBER, 0, AA_GPIO_MODE_INPUT  | AA_GPIO_SPEED_LOW
	#if (defined BSP_BUTTON2_ATTR)
			| BSP_BUTTON2_ATTR
	#endif
	},
#endif
#if (defined BSP_BUTTON3)
	{ 'A'+BSP_BUTTON3_PORT_NUMBER, BSP_BUTTON3_PIN_NUMBER, 0, AA_GPIO_MODE_INPUT  | AA_GPIO_SPEED_LOW
	#if (defined BSP_BUTTON3_ATTR)
			| BSP_BUTTON3_ATTR
	#endif
	},
#endif
#if (defined BSP_BUTTON4)
	{ 'A'+BSP_BUTTON4_PORT_NUMBER, BSP_BUTTON4_PIN_NUMBER, 0, AA_GPIO_MODE_INPUT  | AA_GPIO_SPEED_LOW
	#if (defined BSP_BUTTON4_ATTR)
			| BSP_BUTTON4_ATTR
	#endif
	},
#endif
#if (defined BSP_BUTTON5)
	#error "Too many BSP_BUTTONx"
#endif

	// Outputs
#if (defined BSP_OUTPUT0)
	{ 'A'+BSP_OUTPUT0_PORT_NUMBER, BSP_OUTPUT0_PIN_NUMBER, 0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_HIGH
	#if (defined BSP_OUTPUT0_ATTR)
			| BSP_OUTPUT0_ATTR
	#endif
	},
#endif
#if (defined BSP_OUTPUT1)
	{ 'A'+BSP_OUTPUT1_PORT_NUMBER, BSP_OUTPUT1_PIN_NUMBER, 0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_HIGH
	#if (defined BSP_OUTPUT1_ATTR)
			| BSP_OUTPUT1_ATTR
	#endif
	},
#endif
#if (defined BSP_OUTPUT2)
	{ 'A'+BSP_OUTPUT2_PORT_NUMBER, BSP_OUTPUT2_PIN_NUMBER, 0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_HIGH
	#if (defined BSP_OUTPUT2_ATTR)
			| BSP_OUTPUT2_ATTR
	#endif
	},
#endif
#if (defined BSP_OUTPUT3)
	{ 'A'+BSP_OUTPUT3_PORT_NUMBER, BSP_OUTPUT3_PIN_NUMBER, 0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_HIGH
	#if (defined BSP_OUTPUT3_ATTR)
			| BSP_OUTPUT3_ATTR
	#endif
	},
#endif
#if (defined BSP_OUTPUT4)
	{ 'A'+BSP_OUTPUT4_PORT_NUMBER, BSP_OUTPUT4_PIN_NUMBER, 0, AA_GPIO_MODE_OUTPUT_PP | AA_GPIO_SPEED_HIGH
	#if (defined BSP_OUTPUT4_ATTR)
			| BSP_OUTPUT4_ATTR
	#endif
	},
#endif
#if (defined BSP_OUTPUT5)
	#error "Too many BSP_OUTPUTx"
#endif
} ;

//--------------------------------------------------------------------------------

void	bspBoardInit_ (void)
{
	uint32_t	ii ;

	for (ii = 0u ; ii < sizeof (bspBoardPinDesc) / sizeof (gpioPinDesc_t) ; ii++)
	{
		gpioConfigurePin (& bspBoardPinDesc [ii]) ;
	}
}

//--------------------------------------------------------------------------------
