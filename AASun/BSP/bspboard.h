/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	bspboard.h	Defines GPIO to be configured by the BSP
				The definitions allows to use the BSP functions: bspOutput(), bspToggleOutput(), bspInput()

	When		Who	What
	19/03/20	ac	Creation
	13/05/20	ac	Add BSP_xxxn_ATTR

----------------------------------------------------------------------
*/

#if ! defined AABSPBOARD_H_
#define AABSPBOARD_H_
//--------------------------------------------------------------------------------
//	Fast access to some I/O pins
//	Don't change theses macros

#define	BSP_IO_GPIO(portNumber)				((GPIO_TypeDef *)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*(portNumber)))
#define	BSP_MAKE_IO(portNumber, pinNumber)	((portNumber) << 4 | (pinNumber))


//--------------------------------------------------------------------------------
//	Here the user defines the pins that should be configured by bspBoardInit() in bspboard.c

// For each I/O an optional attribute can be added. Example:
// #define	BSP_BUTTON0_ATTR			(AA_GPIO_PULL_UP)
// #define	BSP_OUTPUT0_ATTR			(AA_GPIO_OPEN_DRAIN | AA_GPIO_PULL_UP)

// Theses pins are for: AA_SUN board V1 + Expansion PCB
// LED
#define BSP_LED0_PORT_NUMBER		(1)	// 0 is port A
#define BSP_LED0_PIN_NUMBER			(12)
#define	BSP_LED0					(BSP_MAKE_IO(BSP_LED0_PORT_NUMBER, BSP_LED0_PIN_NUMBER))
#define	BSP_LED0_ON					(1)
#define	BSP_LED0_OFF				(0)
/*
// Blue LED
#define BSP_LED1_PORT_NUMBER		(1)
#define BSP_LED1_PIN_NUMBER			(15)
#define	BSP_LED1					(BSP_MAKE_IO(BSP_LED1_PORT_NUMBER, BSP_LED1_PIN_NUMBER))
#define	BSP_LED1_ON					(1)
#define	BSP_LED1_OFF				(0)
*/
/*
// User button
#define BSP_BUTTON0_PORT_NUMBER		(1)
#define BSP_BUTTON0_PIN_NUMBER		(13)
#define	BSP_BUTTON0					(BSP_MAKE_IO(BSP_BUTTON0_PORT_NUMBER, BSP_BUTTON0_PIN_NUMBER))
#define	BSP_BUTTON0_ATTR			AA_GPIO_PULL_UP
#define	BSP_BUTTON0_PRESSED			(0)
*/

// User defined pins
// Debug pin 0
#define	BSP_OUTPUT0_PORT_NUMBER		(1)	// B7 J14-7:E_SDA
#define	BSP_OUTPUT0_PIN_NUMBER		(7)
#define	BSP_OUTPUT0					(BSP_MAKE_IO(BSP_OUTPUT0_PORT_NUMBER, BSP_OUTPUT0_PIN_NUMBER))

// Debug pin 1
#define	BSP_OUTPUT1_PORT_NUMBER		(1)	// B6 J14-8:E_SCL
#define	BSP_OUTPUT1_PIN_NUMBER		(6)
#define	BSP_OUTPUT1					(BSP_MAKE_IO(BSP_OUTPUT1_PORT_NUMBER, BSP_OUTPUT1_PIN_NUMBER))

// Debug pin 2
#define	BSP_OUTPUT2_PORT_NUMBER		(0)	// A5 J14-14:CLK_1
#define	BSP_OUTPUT2_PIN_NUMBER		(5)
#define	BSP_OUTPUT2					(BSP_MAKE_IO(BSP_OUTPUT2_PORT_NUMBER, BSP_OUTPUT2_PIN_NUMBER))

//--------------------------------------------------------------------------------
#endif // AABSPBOARD_H_
