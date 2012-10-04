#ifndef _LPX_OUTPUTFL_H_
#define _LPX_OUTPUTFL_H_

/***************************************************************************
 *
 *  lpx_outputfl.h
 *
 *    API definitions for lpx_outputfl.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

int Lpx_OUT_outputfl( struct mbuf *m0,
                      struct route *ro,
                      int flags,
                      struct lpxpcb *lpxp);

#endif	/* !_LPX_OUTPUTFL_H_ */
