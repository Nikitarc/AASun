/*
----------------------------------------------------------------------

	Energy monitor and diverter

	Alain Chebrou

	spid.c		PI controller for main period synchro of AASun

	When		Who	What
	12/08/22	ac	Creation
	18/10/22	ac	Adapted and configured for for AASun synchro

----------------------------------------------------------------------
*/
#if ! defined SPID_H_
#define SPID_H_
//--------------------------------------------------------------------------------

//	The PI controller uses a kind of i32.14 fixed point

#define	SPID_SCALE_BITS		14			// Scale factor: (1 << SPID_SCALE_FACTOR)
#define	SPID_SCALE_FACTOR	(1 << SPID_SCALE_BITS)

void	sPidInit			(int32_t outValue) ;
void	sPidInitIntegrator	(int32_t outValue) ;
void	sPidFactors			(int32_t pFact, int32_t iFact) ;
int32_t	sPid				(int32_t error) ;
void	sPidTraceEnable		(int32_t bEnable) ;

//--------------------------------------------------------------------------------
#endif	// SPID_H_
