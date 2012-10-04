#ifndef _LPX_CONTROL_H_
#define _LPX_CONTROL_H_

/***************************************************************************
 *
 *  lpx_control.h
 *
 *    API definitions for lpx_control.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

int Lpx_CTL_control( struct socket *so,
                     u_long cmd,
                     caddr_t data,
                     register struct ifnet *ifp,
                     struct proc *td);

struct lpx_ifaddr * Lpx_CTL_iaonnetof( register struct lpx_addr *dst );

void Lpx_CTL_printhost( register struct lpx_addr *addr );

#endif	/* !_LPX_CONTROL_H_ */
