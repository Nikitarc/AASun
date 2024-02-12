/*********************************************************************************
 * Access Dallas 1-Wire Devices
 * Author of the initial code: Peter Dannegger (danni(at)specs.de)
 * modified by Martin Thomas (mthomas(at)rhrk.uni-kl.de)
 * modified by Chi Zhang (zhangchi866(at)gmail.com)
 *********************************************************************************/

// https://github.com/dword1511/onewire-over-uart
// Thank you. This worked out of the box! (only cosmetic changes)

#include "aa.h"
#include "onewire.h"
#include "owuart.h"

//#define	OW_DEBUG
//#define	OW_DEBUG_BIT

#ifdef OW_DEBUG
#include <stdio.h>
#endif

#define OW_MATCH_ROM    0x55
#define OW_SKIP_ROM     0xcc
#define OW_SEARCH_ROM   0xf0

//--------------------------------------------------------------------------------

uint8_t ow_init(char *dev_path)
{
  if(ow_uart_init(dev_path) != UART_SUCCESS)
	  return OW_ERR_UART;
  else
	  return OW_ERR_SUCCESS;
}

//--------------------------------------------------------------------------------

void ow_finit(void)
{
	ow_uart_finit();
}

//--------------------------------------------------------------------------------

uint8_t ow_reset(void)
{
  uint8_t err;

  ow_uart_setb(OW_BAUD_LOW);
  /* pull DQ line low, then up. UART transmits LSB first. */
  err = ow_uart_putc(0xf0);
  ow_uart_setb(OW_BAUD_HIGH);

#ifdef OW_DEBUG
  aaPrintf("OW_DEBUG: Probe result: 0x%02x\n", err);
#endif

  if(err == 0x00)
	  return OW_ERR_SHORT;
  if(err == 0xf0)
	  return OW_ERR_PRESENCE;
  return OW_ERR_SUCCESS;
}

//--------------------------------------------------------------------------------
// Write or read 1 bit

uint8_t ow_bit(uint8_t b)
{
  uint8_t c;

  if (b)
	  c = ow_uart_putc(0xff); /* Write 1 */
  else
	  c = ow_uart_putc(0x00); /* Write 0 */

#ifdef OW_DEBUG_BIT
  aaPrintf("OW_DEBUG: ATOMIC OP: %d -> %d\n", b, c == 0xff);
#endif

  return (c == 0xff);
}

//--------------------------------------------------------------------------------
// Write 1 byte

uint8_t ow_byte_wr(uint8_t b)
{
  uint8_t i = 8, j;

#ifdef OW_DEBUG
  aaPrintf("OW_DEBUG: Write char:   0x%02x\n", b);
#endif

  do
  {
    j = ow_bit(b & 1);
    b >>= 1;
    if(j)
    	b |= 0x80;
  } while(-- i);

#ifdef OW_DEBUG
  aaPrintf("OW_DEBUG: Write result: 0x%02x\n", b);
#endif

  return b;
}

//--------------------------------------------------------------------------------

uint8_t ow_byte_rd(void)
{
  /* read by sending only "1"s, so bus gets released after the init low-pulse in every slot */
  return ow_byte_wr(0xff);
}

//--------------------------------------------------------------------------------

uint8_t ow_rom_search(uint8_t diff, uint8_t *id)
{
  uint8_t i, j, next_diff;
  uint8_t b;

  /* error, no device found <--- early exit! */
  if(ow_reset() != OW_ERR_SUCCESS)
	  return OW_ERR_PRESENCE;

  ow_byte_wr(OW_SEARCH_ROM);  /* ROM search command */
  next_diff = OW_LAST_DEVICE; /* unchanged on last device */

  i = OW_ROMCODE_SIZE * 8; /* 8 bytes */

  do
  {
    j = 8; /* 8 bits */
    do
    {
      b = ow_bit(1); /* read bit */
      if(ow_bit(1))
      {
        if(b)
        	return OW_ERR_DATA; /* read complement bit, 0b11: data error <--- early exit! */
      }
      else if(!b)
      {
    	  if(diff > i || ((*id & 1) && diff != i)) /* 0b00 = 2 devices */
    	  {
              b = 1;         /* now 1 */
    	      next_diff = i; /* next pass 0 */
    	  }
      }

      ow_bit(b); /* write bit */
      *id >>= 1;
      if(b)
    	  *id |= 0x80; /* store bit */

      i --;
    } while(-- j);
    id ++; /* next byte */
  } while(i);

  return next_diff; /* to continue search */
}

void ow_command(uint8_t command, uint8_t *id)
{
  uint8_t i;

  ow_reset();

  if(id)
  {
    ow_byte_wr(OW_MATCH_ROM); /* to a single device */
    i = OW_ROMCODE_SIZE;
    do
    {
      ow_byte_wr(*id);
      id++;
    } while(--i);
  }
  else
    ow_byte_wr(OW_SKIP_ROM); /* to all devices */

  ow_byte_wr(command);
}

//--------------------------------------------------------------------------------
