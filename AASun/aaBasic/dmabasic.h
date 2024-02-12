/*
----------------------------------------------------------------------

	AdAstra - Real Time Kernel

	Alain Chebrou

	dmabasic.c	DMA management for L4+ and G0 (doesn't have data cache)

	When		Who	What
	23/04/18	ac	Creation
	27/02/20	ac	Port to STM32H7xx
	13/04/21	ac	Port to STM32G0xx
	20/04/21	ac	Add dmaSetCallbackRaw()
					Unify G0 / L4+
	29/07/21	ac	Modify dmaDesc_t: add muxInput and config
					Add dmaGetFlagsRaw(), dmaClearFlagsRaw()
	11/10/21	ac	Add dmaStopStream() and dmaStopStreamRaw()
					Change dmaResetStream() to dmaStreamResetRaw()
					Change dmaStreamAddress() to dmaStreamAddressRaw()
					Add dmaSetMode()

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

#if ! defined	DMABASIC_H_
#define DMABASIC_H_
//----------------------------------------------------------------------

#if (defined STM32G0)
	#include	"stm32g0xx_ll_dma.h"
#elif (defined STM32L4)
	#include	"stm32l4xx_ll_dma.h"
#elif (defined STM32G4)
	#include	"stm32g4xx_ll_dma.h"
#else
	#error "MCU not supported"
#endif

//----------------------------------------------------------------------
// No cache for L4 and G0, declaration for H7 compatibility only

// BEWARE: If DMA is used for I/O, the library must manage the data cache.
// So align buffers address on multiple of cache line size (32 bytes)
// Buffers size should be a multiple of cache line size (32 bytes). Mandatory for RX buffers.

// Macro to get variable aligned on 32-bytes,needed for cache maintenance purpose

# if ! defined ALIGN_32BYTES
	#if defined   (__GNUC__)		// GNU Compiler
		#define ALIGN_32BYTES(buf)  buf __attribute__ ((aligned (32)))
	#elif defined (__ICCARM__)		// IAR Compiler
		#define ALIGN_32BYTES(buf) _Pragma("data_alignment=32") buf
	#elif defined   (__CC_ARM)		// ARM Compiler
		#define ALIGN_32BYTES(buf) __align(32) buf
	#endif
#endif

// Macro to round the size of an array to a upper multiple of 32
#define	SIZE_32BYTES(count,type)	(((sizeof (type) * (count)) + 31u) & ~0x0000001Fu)

//----------------------------------------------------------------------

// Get pointer to DMA device from DMA number
#if (defined DMA2)
	#define	DMA_GETDMA(dmaNum)		((dma_t *)(((dmaNum) == 1u) ? DMA1 : DMA2))
#else
	#define	DMA_GETDMA(dmaNum)		((dma_t *) DMA1)
#endif
#define	DMA_GETSTREAM(pDesc)		((pDesc)->streamNum)

#define	DMA_NONE	0
#define	DMA_DMA1	1
#define	DMA_DMA2	2

typedef struct dmaDesc_s
{
	uint8_t		dmaNum ;		//	DMA number 1 or 2, DMA_NONE => unused descriptor
	uint8_t		streamNum ;		//	Stream number 0..n depending on MCU
	uint8_t		irqPrio ;		//	Interrupt priority
	uint8_t		muxInput ;		//	DMAMUX input
	uint16_t	config ;		//	Direction e.g. LL_DMA_DIRECTION_PERIPH_TO_MEMORY, mode, priority...

} dmaDesc_t ;

// Event of DMA IRQ callback function
#define	DMA_EV_COMPLETE		1u		// Transfer complete
#define	DMA_EV_HCOMPLETE	2u		// Transfer half complete
#define	DMA_EV_ERROR		3u		// Transfer error
#define	DMA_EV_GLOBAL		4u		// Global interrupt

// Callback parameter source is 'dmaNum << 16 | streamNum' :
//		High word : DMA number (1 or 2)
//		Low word  : stream number (0..n)

// Prototype of DMA IRQ callback function
typedef void (* dmaCallback_t) (uint32_t event, uint32_t source, uintptr_t arg) ;

//----------------------------------------------------------------------
//	Some definitions of DMA structures easier to use than those of LL

// STM32G0: DMAMUX1 have up to 12 channels, 4 request generator
// STM32G4: DMAMUX1 have up to 16 channels, 4 request generator
// STM32L4: DMAMUX1 have 14 channels, DMAMUX2 have 8 channels, 4 request generator
// STM32H7: DMAMUX1 have 16 channels, DMAMUX2 have 8 channels, 8 request generator

typedef struct dmaMux_s
{
	__IO uint32_t	CCR  [16] ;		// 0x000 Request line multiplexer channel x configuration register
	__IO uint32_t	res1 [16] ;		//       Reserved
	__IO uint32_t	CSR ;			// 0x080 Request line multiplexer interrupt channel status register
	__IO uint32_t	CFR ;			// 0x084 Request line multiplexer interrupt clear flag register
	__IO uint32_t	res2 [30] ;		//       Reserved
	__IO uint32_t	RGCR [8] ;		// 0x100 Request generator channel x configuration register
	__IO uint32_t	res3 [8] ;		//       Reserved
	__IO uint32_t	RGSR ;			// 0x140 DMA Request Generator Status Register
	__IO uint32_t	RGCFR ;			// 0x144 Request generator interrupt status register

} dmaMux_t ;

typedef struct dmaStream_s
{
	__IO uint32_t	CCR ;         	// DMA channel x configuration register
	__IO uint32_t	CNDTR ;       	// DMA channel x number of data register
	__IO uint32_t	CPAR ;        	// DMA channel x peripheral address register
	__IO uint32_t	CMAR ;        	// DMA channel x memory address register
	__IO uint32_t	reserved ;		// Not defined by ST files...

} dmaStream_t ;

// This is the structure pointed to by DMA1 or DMA2
typedef struct dma_s
{
	__IO uint32_t	ISR ;			// DMA interrupt status register
	__IO uint32_t	IFCR ;			// DMA interrupt flag clear register
	dmaStream_t		stream [8] ;

} dma_t ;

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

void		dmaEnableClock		(dma_t * pDma) ;
void		dmaSetIrq			(const dmaDesc_t * pDmaDesc) ;
void		dmaDisableIrq		(const dmaDesc_t * pDmaDesc) ;
void		dmaSetMode 			(const dmaDesc_t * pDmaDesc, uint32_t mode) ;
void		dmaSetCallback		(const dmaDesc_t * pDmaDesc, dmaCallback_t fn, uintptr_t arg) ;
void		dmaSetCallbackRaw	(dma_t * pDma, uint32_t streamNum, dmaCallback_t fn, uintptr_t arg) ;

// Flags to use with dmaGetFlags(), dmaClearFlags(), dmaGetFlagsRaw(), dmaClearFlagsRaw()
#define DMA_FLAG_GIF		((uint32_t) 0x00000001u)	// Global interrupt
#define DMA_FLAG_TCIF		((uint32_t) 0x00000002u)	// Transfer complete
#define DMA_FLAG_HTIF		((uint32_t) 0x00000004u)	// Transfer half complete
#define DMA_FLAG_TEIF		((uint32_t) 0x00000008u)	// Transfer error
#define DMA_FLAG_ALLIF		((uint32_t) 0x0000000Fu)

uint32_t	dmaGetFlags		(const dmaDesc_t * pDmaDesc) ;
void		dmaClearFlags	(const dmaDesc_t * pDmaDesc, uint32_t flags) ;

//	Allows to get or clear DMA_ISR flags for streamNum. Bits are right aligned
//	Use a combination of: DMA_FLAG_TEIF, DMA_FLAG_HTIF, DMA_FLAG_TCIF

__STATIC_INLINE uint32_t	dmaGetFlagsRaw		(dma_t * pDma, uint32_t streamNum)
{
	uint32_t	isr = pDma->ISR ;
	return ((isr >> (streamNum << 2)) & 0x0Fu) ;	// Each stream have 4 flags
}

__STATIC_INLINE void		dmaClearFlagsRaw	(dma_t * pDma, uint32_t streamNum, uint32_t flags)
{
	pDma->IFCR = (flags & 0x0Fu) << (streamNum << 2u) ;
}

// Get a pointer to a stream registers
__STATIC_INLINE dmaStream_t * dmaStreamAddressRaw (dma_t * pDma, uint32_t streamNum)
{
	return & pDma->stream [streamNum] ;
}

__STATIC_INLINE dmaStream_t * dmaStreamAddress (const dmaDesc_t * pDmaDesc)
{
	return dmaStreamAddressRaw (DMA_GETDMA (pDmaDesc->dmaNum), DMA_GETSTREAM (pDmaDesc)) ;
}

// Set stream to reset state
__STATIC_INLINE void	dmaStreamResetRaw	(dma_t * pDma, uint32_t streamNum)
{
	dmaStream_t * pStream = dmaStreamAddressRaw (pDma, streamNum) ;

	pStream->CCR	= 0u ;
	pStream->CNDTR	= 0u ;
	pStream->CPAR	= 0u ;
	pStream->CMAR	= 0u ;
	dmaClearFlagsRaw (pDma, streamNum, DMA_FLAG_ALLIF) ;
}

__STATIC_INLINE void	dmaStreamReset (const dmaDesc_t * pDmaDesc)
{
	dmaStreamResetRaw (DMA_GETDMA (pDmaDesc->dmaNum), DMA_GETSTREAM (pDmaDesc)) ;
}

// Stop a stream and reset flags
__STATIC_INLINE void	dmaStreamStopRaw	(dma_t *  pDma, uint32_t streamNum)
{
	dmaStream_t * pStream = dmaStreamAddressRaw (pDma, streamNum) ;

	pStream->CCR	&= ~DMA_CCR_EN ;
	dmaClearFlagsRaw (pDma, streamNum, DMA_FLAG_ALLIF) ;
}

__STATIC_INLINE void	dmaStreamStop	(const dmaDesc_t * pDmaDesc)
{
	dmaStreamStopRaw (DMA_GETDMA (pDmaDesc->dmaNum), DMA_GETSTREAM (pDmaDesc)) ;
}

#ifdef __cplusplus
}
#endif

//----------------------------------------------------------------------
#endif	// DMABASIC_H_
