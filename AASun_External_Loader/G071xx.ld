
/*--------------------------------------------------------------------------
/*
 * Memory Spaces Definitions.
 *
 * Update for a specific board. 
 *   FLASH.ORIGIN : starting address of flash
 *   FLASH.LENGTH : length of flash
 *   RAM.ORIGIN   : starting address of main RAM
 *   RAM.LENGTH   : length of main RAM
 *
 * The values below can be addressed in linker scripts
 * using functions like 'ORIGIN(RAM)' or 'LENGTH(RAM)'.
 */

/*	Memory definitions for
	144 KB	G0B0xx / G0C0xx / G0B1xx / G0C1xx
	 36 KB	G070xx /          G071xx / G081xx
	 18 KB	                  G051xx / G061xx
	  8 KB	G030xx / G050xx
 
	Adjust your amount of FLASH

	Can execute code from SRAM
*/

MEMORY
{
	FLASH         (rx) : ORIGIN = 0x08000000, LENGTH = 128K
	FLASHB2       (rx) : ORIGIN = 0x00000000, LENGTH = 0
	RAM          (xrw) : ORIGIN = 0x20000000, LENGTH = 36K
	CCMRAM        (rw) : ORIGIN = 0x10000000, LENGTH = 0K
	BKPRAM        (rw) : ORIGIN = 0x40024000, LENGTH = 0K

	EXTMEMB0      (rx) : ORIGIN = 0x00000000, LENGTH = 0
	EXTMEMB1      (rx) : ORIGIN = 0x00000000, LENGTH = 0
	EXTMEMB2      (rx) : ORIGIN = 0x00000000, LENGTH = 0
	EXTMEMB3      (rx) : ORIGIN = 0x00000000, LENGTH = 0
}

INCLUDE ../sections.ld