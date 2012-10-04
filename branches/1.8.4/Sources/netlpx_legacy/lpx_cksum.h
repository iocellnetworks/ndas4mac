#ifndef _LPX_CKSUM_H_
#define _LPX_CKSUM_H_

/***************************************************************************
 *
 *  lpx_cksum.h
 *
 *    API definitions for lpx_cksum.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

u_short Lpx_cksum(struct mbuf *m, int len);

#endif	/* !_LPX_CKSUM_H_ */
