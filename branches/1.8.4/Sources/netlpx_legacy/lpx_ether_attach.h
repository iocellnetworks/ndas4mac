#ifndef _LPX_ETHER_ATTACH_H_
#define _LPX_ETHER_ATTACH_H_

/***************************************************************************
 *
 *  lpx_ether_attach.h
 *
 *    API definitions for lpx_ether_attach.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

u_long Lpx_EA_ether_attach(struct ifnet *ifp);
u_long Lpx_EA_ether_detach(struct ifnet *ifp);

#endif	/* !_LPX_ETHER_ATTACH_H_ */
