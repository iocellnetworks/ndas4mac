#ifndef _LPX_CONTROL_INT_H_
#define _LPX_CONTROL_INT_H_

/***************************************************************************
 *
 *  lpx_control_int.h
 *
 *    Internal definitions for lpx_control.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  Helper function prototypes
 *
 **************************************************************************/

/*
 * Delete any previous route for an old address.
 */
static void lpx_CTL_ifscrub( register struct ifnet *ifp,
                             register struct lpx_ifaddr *ia );

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int lpx_CTL_ifinit( register struct ifnet *ifp,
                           register struct lpx_ifaddr *ia,
                           register struct sockaddr_lpx *slpx,
                           int scrub );

#endif	/* !_LPX_CONTROL_INT_H_ */
