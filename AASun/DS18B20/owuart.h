#ifndef __UART_H__
#define __UART_H__

#define UART_SUCCESS 0

/* Settings */
/* NOTE: baud rate lower than 9600 / 15200 might not work */
#ifndef OW_BAUD_LOW
  #define OW_BAUD_LOW  9600
#endif
#ifndef OW_BAUD_HIGH
  #define OW_BAUD_HIGH 115200
#endif

/* UART implementation for different platforms */
int           ow_uart_init		(char * dev_path) ;
void          ow_uart_finit		(void) ;
void          ow_uart_setb		(uint32_t baud) ;
unsigned char ow_uart_putc		(unsigned char c) ;

extern	uint32_t	owuartTimeout ;

#endif /* __UART_H__ */
