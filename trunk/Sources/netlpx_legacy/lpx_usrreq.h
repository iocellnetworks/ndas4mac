#ifndef _LPX_USRREQ_H_
#define _LPX_USRREQ_H_

/***************************************************************************
 *
 *  lpx_usrreq.h
 *
 *    API definitions for lpx_usrreq.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

/*
 *  This may also be called for raw listeners.
 */
#if 0
void Lpx_USER_input( struct mbuf *m, register struct lpxpcb *lpxp );
#else /* if 0 */
void Lpx_USER_input( struct mbuf *m,
                     register struct lpxpcb *lpxp,
                     struct ether_header *frame_header);
#endif /* if 0 else */

int Lpx_USER_ctloutput( struct socket *so, struct sockopt *sopt );

int Lpx_USER_peeraddr( struct socket *so, struct sockaddr **nam );

int Lpx_USER_sockaddr( struct socket *so, struct sockaddr **nam );

#endif	/* !_LPX_USRREQ_H_ */
