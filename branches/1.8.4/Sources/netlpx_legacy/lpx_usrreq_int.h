#ifndef _LPX_USRREQ_INT_H_
#define _LPX_USRREQL_INT_H_

/***************************************************************************
 *
 *  lpx_usrreq_int.h
 *
 *    Internal definitions for lpx_usrreq.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  Helper function prototypes
 *
 **************************************************************************/

static int lpx_USER_abort( struct socket *so );

static int lpx_USER_attach( struct socket *so, int proto, struct proc *td );

static int lpx_USER_bind( struct socket *so,
                          struct sockaddr *nam,
                          struct proc *td );

static int lpx_USER_connect( struct socket *so,
                             struct sockaddr *nam,
                             struct proc *td );

static int lpx_USER_detach( struct socket *so );

static int lpx_USER_disconnect( struct socket *so );

static int lpx_USER_send( struct socket *so,
                          int flags,
                          struct mbuf *m,
                          struct sockaddr *nam,
                          struct mbuf *control,
                          struct proc *td );

static int lpx_USER_shutdown( struct socket *so );

static int lpx_USER_r_attach( struct socket *so, int proto, struct proc *td );

static int lpx_USER_output( struct lpxpcb *lpxp, struct mbuf *m0 );

#endif	/* !_LPX_USRREQ_INT_H_ */
