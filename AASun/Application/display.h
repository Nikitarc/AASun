/*
----------------------------------------------------------------------

	Alain Chebrou

	display.h	Simplified library for 128x64 OLED display with SH1106 controller IC
				SPI interface only

	When		Who	What
	07/12/22	ac	Creation

----------------------------------------------------------------------
*/

#if ! defined DISPLAY_H_
#define DISPLAY_H_
//--------------------------------------------------------------------------------

// The display dimension
#define DISPLAY_WIDTH		128
#define DISPLAY_HEIGHT		64
#define DISPLAY_PAGE_COUNT	8		// Count of pixel in 1 page height
#define	DISPLAY_FLIP		0		// Invert top/bottom

#define	COLOR_WHITE			1
#define	COLOR_BLACK			0

#define	FONT_SMALL			8
#define	FONT_BIG			16

#ifdef __cplusplus
extern "C" {
#endif

// Display API
void 		displayReset		(void) ;
void 		displayInit			(void) ;
void		displayOn			(void) ;
void 		displayOff			(void) ;
void 		displayInvert		(unsigned mode) ;
void 		displaySetPos		(unsigned char x, unsigned char y) ;
void 		displaySetPixel		(unsigned char x, unsigned char y,unsigned char color) ;
void 		displayClear		(unsigned dat) ;
void 		displayUpdate		(void) ;
void		displaySetFont		(uint32_t fontType) ;	// FONT_SMALL or FONT_BIG
void		displayChar			(char cc) ;
void		displayString		(const char * pStr) ;
void		displayBitmap		(const uint8_t * pBitmap, uint32_t width, uint32_t height) ;

// Buttons API
void		buttonInit			(void) ;
void		buttonPoll			(void) ;
uint32_t	buttonGet			(void) ;
void		buttonAck			(uint32_t mask) ;

// Digital input/output API
									// To use for inputGet()
#define		IO_IN1				0	// 	3.3V input of J6
#define		IO_IN2				1	//  Input of J7 (routed to expansion)
#define		IO_PULSE1			2	// 	Input from pulse counter 1
#define		IO_PULSE2			3	// 	Input from pulse counter 2

									// To use for outputSet()
#define		IO_OUT3				2	// 	On board relay
#define		IO_OUT4				3	// 	3.3V/5V output, J3 on expansion

void		inOutInit			(void) ;
bool		inputGet			(uint32_t index, uint32_t * pValue) ;
void		outputSet			(uint32_t index, uint32_t value) ;

// Pulse count API
void		pulsePoll			(void) ;
uint32_t	pulseGet			(uint32_t ix) ;
void		pulseIncr			(uint32_t ix, uint32_t count) ;	// Debug only

// Page display API
#define		PAGE_UP				1
#define		PAGE_DOWN			0
void		pageSet				(uint32_t index) ;	// Display the page at index
uint32_t	pageGet				(void) ;			// Get current page index
void		pageNext			(uint32_t next) ;	// Next is PAGE_UP or PAGE_DOWN
void		pageUpdate 			(void) ;			// Redraw the current page

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------------------
#endif	// DISPLAY_H_

