/*
	Generic linker script for Cortex-M STM32G0xx
	This file is included by the specific MCU linker script
 
	Use multi-region initializations for code, data, bss and noinit

	11/06/21	ac		Add .RamFunc in .data section for HAL RAM code

*/
 
/*
	To set a function in RAM section:
	__attribute__ ((section(".text_RAM")))	void ramFunction (void) 
	or
	__attribute__ ((section(".RamFunc")))	void ramFunction (void)	// The section from the ST HAL 

	To set an initialized data in a CCMRAM or BKPRAM:
	__attribute__ ((section(".data_CCMRAM")))	int32_t	nbData = 3 ;
	__attribute__ ((section(".data_BKPRAM")))	int32_t	nbData = 3 ;
	
	To set a zeroized data in CCMRAM or BKPRAM:
	__attribute__ ((section(".bss_CCMRAM")))	int32_t	nbData ;
	__attribute__ ((section(".bss_BKPRAM")))	int32_t	nbData ;

	To set an uninitialized data in RAM, CCMRAM or BKPRAM:
	__attribute__ ((section(".noinit")))	int32_t	nbData ;
	__attribute__ ((section(".noinit_CCMRAM")))	int32_t	nbData ;
	__attribute__ ((section(".noinit_BKPRAM")))	int32_t	nbData ;

	There is no provision to put data or bss section in external RAM
*/

/*--------------------------------------------------------------------------
/* The main (privileged) stack definition, default heap definition */

/* Required amount of RAM for main stack */
Main_Stack_Size_ = 1024 ;

_estack_ = ORIGIN(RAM) + LENGTH(RAM);		/* End of main stack pointer just beyond the RAM */
_sstack_ = _estack_  - Main_Stack_Size_ ;	/* Start of main stack	*/

/* Default heap definitions.
 * The heap start immediately after the last statically allocated 
 * .bss/.noinit section, and extends up to the main stack start.
 */
Min_Heap_Size_ = 1024 ;					/* For RAM size check only */
PROVIDE ( _heap_begin_ = _end_noinit ) ;
PROVIDE ( _heap_end_   = _sstack_ ) ;

/* The entry point is informative since the Cortex-M reset vector points to it anyway */
ENTRY(start_)

/*--------------------------------------------------------------------------
 * Sections Definitions
 */

SECTIONS
{
    /*
     * The main stack address, IRQ vectors, the default IRQ handlers and the startup code
     * are stored in the .isr_vectors section, which goes to beginning of FLASH. 
     */
    .isr_vectors : ALIGN(4)
    {
        FILL(0xFF)
        
        KEEP(*(.isr_stackaddress))			/* Interrupt stack address */
        KEEP(*(.isr_vectors))				/* Interrupt vectors */
        *(.isr_handlers .isr_handlers.*)	/* Interrupt handlers */
        *(.startup .startup.*)				/* Startup code*/
		*(.after_vectors*)

    } >FLASH

    .inits : ALIGN (4)
    {
        /* 
         * Memory regions initialization tables.
         *
         * There are two kinds of arrays for each RAM region: one for data and one for bss.
         * The startup code uses these tables to perform the initialization.
         * 
         * The data table includes:
         * - from (LOADADDR()) (in FLASH)
         * - region_begin (ADDR())
         * - region_end (ADDR()+SIZEOF())
         *
         * The bss table includes:
         * - region_begin (ADDR())
         * - region_end (ADDR()+SIZEOF())
         *
         * WARNING: It is mandatory that the regions are word aligned, 
         * since the initialization code works only on words.
         */
         
         /* The startup code uses only the variables defined in this section */
         
        __data_regions_array_start = . ;
        
        /* Code in RAM */
        LONG(LOADADDR(.text_RAM)) ;
        LONG(ADDR(.text_RAM)) ;
        LONG(ADDR(.text_RAM)+SIZEOF(.text_RAM)) ;
        
        /* Initialized data in RAM */
        LONG(LOADADDR(.data)) ;
        LONG(ADDR(.data)) ;
        LONG(ADDR(.data)+SIZEOF(.data)) ;
        
        /* Initialized data in CCM RAM */
        LONG(LOADADDR(.data_CCMRAM)) ;
        LONG(ADDR(.data_CCMRAM)) ;
        LONG(ADDR(.data_CCMRAM)+SIZEOF(.data_CCMRAM)) ;
        
 		/* Backup RAM initialized data */
        LONG(LOADADDR(.data_BKPRAM)) ;
        LONG(ADDR(.data_BKPRAM)) ;
        LONG(ADDR(.data_BKPRAM)+SIZEOF(.data_BKPRAM)) ;

        __data_regions_array_end = . ;
        
        __bss_regions_array_start = . ;
        
        LONG(ADDR(.bss)) ;
        LONG(ADDR(.bss)+SIZEOF(.bss)) ;
        
        LONG(ADDR(.bss_CCMRAM)) ;
        LONG(ADDR(.bss_CCMRAM)+SIZEOF(.bss_CCMRAM)) ;
        
        LONG(ADDR(.bss_BKPRAM)) ;
        LONG(ADDR(.bss_BKPRAM)+SIZEOF(.bss_BKPRAM)) ;
        
        __bss_regions_array_end = . ;

        /* End of memory regions initialization arrays. */
    
		/*--------------------------------------------------------------------------
         * The C++ preinit code, i.e. an array of pointers to initialization 
         * functions to be performed before constructors.
         */
		. = ALIGN (4) ;
		PROVIDE_HIDDEN (__preinit_array_start = .) ;
        
        /* The application inits. If you need to enforce some order in 
         * execution, create new sections before this.
         */
		KEEP (*(.preinit_array .preinit_array.*))
		PROVIDE_HIDDEN (__preinit_array_end = .) ;

		/*--------------------------------------------------------------------------
         * The C++ init code, i.e. an array of pointers to static constructors.
         */
		. = ALIGN (4) ;
		PROVIDE_HIDDEN (__init_array_start = .) ;
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .) ;

		/*--------------------------------------------------------------------------
         * The C++ fini code, i.e. an array of pointers to static destructors.
         * .fini_array ommited because when would we run destructors?
         */
		. = ALIGN (4) ;
		PROVIDE_HIDDEN (__fini_array_start = .) ;
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .) ;

    } >FLASH

    /*--------------------------------------------------------------------------
     * The program code is stored in the .text section, which goes to FLASH.
     */
	.text : ALIGN(4)
	{
		*(.text .text.*)			/* All the code */
 
		/* Read-only data (constants) */
		*(.rodata .rodata.* .constdata .constdata.*)

		*(vtable)					/* C++ virtual tables */

		KEEP(*(.eh_frame*))

	} >FLASH

	/* ARM magic sections */
	.ARM.extab : ALIGN(4)
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASH

	. = ALIGN(4);
	.ARM.exidx : ALIGN(4)
	{
		__exidx_start = .;   	
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
		__exidx_end = .;
	} > FLASH
   	
	. = ALIGN(4);
	_etext = .;
    
	/*--------------------------------------------------------------------------	
	 * The code section in RAM
	 */
	.text_RAM : ALIGN(4)
	{
		*(.text_RAM .text_RAM.*)
		*(.RamFunc)        /* .RamFunc sections */
		*(.RamFunc*)       /* .RamFunc* sections */
		. = ALIGN(4) ;
	} > RAM AT>FLASH

	/* --------------------------------------------------------------------------
     * The initialized data sections.
     *
     * The program executes knowing that the data is in the RAM
     * but the compiler puts the initial values in the FLASH (inidata).
     * It is one task of the startup to copy the initial values from 
     * FLASH to RAM.
     */
	.data : ALIGN(4)
	{
		FILL(0xFF)

		*(.data)
		*(.data.*)

		. = ALIGN(4);
	} >RAM AT>FLASH

	.data_CCMRAM : ALIGN(4)
	{
		*(.data_CCMRAM .data_CCMRAM.*)
		. = ALIGN(4) ;
	} > CCMRAM AT>FLASH

	.data_BKPRAM : ALIGN(4)
	{
		*(.data_BKPRAM .data_BKPRAM.*)
		. = ALIGN(4) ;
	} >BKPRAM AT>FLASH

    /*--------------------------------------------------------------------------
     * The zeroized data sections. NOLOAD is used to avoid
     * the section type to be changed to PROGBITS
     */
     
	.bss_CCMRAM (NOLOAD) : ALIGN(4)
	{
		*(.bss_CCMRAM .bss_CCMRAM.*)
		. = ALIGN(4) ;
	} > CCMRAM

	.bss_BKPRAM (NOLOAD) : ALIGN(4)
	{
		*(.bss_BKPRAM .bss_BKPRAM.*)
		. = ALIGN(4) ;
	} > BKPRAM AT>FLASH

	/* The primary uninitialised data section. */
	.bss (NOLOAD) : ALIGN(4)
	{
		*(.bss)
		*(.bss.*)
		*(COMMON)

		. = ALIGN(4);
	} >RAM

    /*--------------------------------------------------------------------------
     * The uninitialized data sections. NOLOAD is used to avoid
     * the section type to be changed to PROGBITS
     */
    .noinit_CCMRAM (NOLOAD) : ALIGN(4)
    {
        *(.noinit_CCMRAM .noinit_CCMRAM.*)         
    } > CCMRAM
    
    .noinit_BKPRAM (NOLOAD) : ALIGN(4)
    {
        *(.noinit_BKPRAM .noinit_BKPRAM.*)         
         . = ALIGN(4) ;
    } > BKPRAM

	/* This section MUST be the last one in RAM */
	.noinit (NOLOAD) : ALIGN(8)
	{
		_noinit = .;
		*(.noinit .noinit.*) 

		. = ALIGN(4) ;
		_end_noinit = . ;	/* Mandatory to be word aligned, _sbrk assumes this */
	} > RAM

	PROVIDE ( end  = _end_noinit );		/* For _sbrk, if used */
	PROVIDE ( _end = _end_noinit );
    
    /*--------------------------------------------------------------------------
     * Used for validation only, do not allocate anything in RAM from here
     *
     * This is to check that there is enough RAM left for the Main stack.
     * It generate an error if it's full.
     */
	._check_heap_stack : ALIGN(4)
	{
		. = . + Main_Stack_Size_ + Min_Heap_Size_ ;
	} >RAM

	/*--------------------------------------------------------------------------
	/*--------------------------------------------------------------------------
     * The FLASH Bank2
     * The C or assembly source must explicitly place the code 
     * or data there using the "section" attribute.
	 */
	.text_b2 : ALIGN(4)
	{
		*(.text_b2 .test_b2.*)			/* The code */
		*(.rodata_b2*)					/* Read-only data (constants) */
		*(.rodata_b2.*)
	} >FLASHB2
    
    /*--------------------------------------------------------------------------
    /*--------------------------------------------------------------------------
     * The EXTMEM.
     * The C or assembly source must explicitly place the code or data there
     * using the "section" attribute.
     */

    /* EXTMEM Bank0 */
    .eb0text : ALIGN(4)
    {
        *(.eb0text   .eb0text.*)     /* remaining code */
        *(.eb0rodata .eb0rodata.*)   /* read-only data (constants) */
    } >EXTMEMB0
    
    /* EXTMEM Bank1 */
    .eb1text : ALIGN(4)
    {
        *(.eb1text   .eb1text.*)     /* remaining code */
        *(.eb1rodata .eb1rodata.*)   /* read-only data (constants) */
    } >EXTMEMB1
    
    /* EXTMEM Bank2 */
    .eb2text : ALIGN(4)
    {
        *(.eb2text   .eb2text.*)     /* remaining code */
        *(.eb2rodata .eb2rodata.*)   /* read-only data (constants) */
    } >EXTMEMB2
    
    /* EXTMEM Bank3 */
    .eb3text : ALIGN(4)
    {
        *(.eb3text   .eb3text.*)     /* remaining code */
        *(.eb3rodata .eb3rodata.*)   /* read-only data (constants) */
    } >EXTMEMB3

	/*--------------------------------------------------------------------------*/
	/*--------------------------------------------------------------------------*/

	/* Remove the debugging information from the standard libraries */    
	DISCARD :
	{
		libc.a ( * )
		libm.a ( * )
		libgcc.a ( * )
	}
}
