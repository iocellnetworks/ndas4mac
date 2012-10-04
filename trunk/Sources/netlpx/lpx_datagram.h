#ifndef _LPX_DATAGRAM_H_
#define _LPX_DATAGRAM_H_

#include "lpx_pcb.h"

/***************************************************************************
*
*  Helper function prototypes
*
**************************************************************************/

void lpx_datagram_init();

static int lpx_datagram_user_abort( struct socket *so );

static int lpx_datagram_user_attach( struct socket *so, int proto, struct proc *td );

static int lpx_datagram_user_bind( struct socket *so,
                          struct sockaddr *nam,
                          struct proc *td );

static int lpx_datagram_user_connect( struct socket *so,
                             struct sockaddr *nam,
                             struct proc *td );

static int lpx_datagram_user_detach( struct socket *so );

static int lpx_datagram_user_disconnect( struct socket *so );

static int lpx_datagram_user_send( struct socket *so,
                          int flags,
                          struct mbuf *m,
                          struct sockaddr *nam,
                          struct mbuf *control,
                          struct proc *td );

static int lpx_datagram_user_shutdown( struct socket *so );

static int lpx_datagram_user_output( struct lpxpcb *lpxp, struct mbuf *m0 );

/***************************************************************************
*
*  API prototypes
*+
**************************************************************************/

/*
 *  This may also be called for raw listeners.
 */
void lpx_datagram_free();

void lpx_datagram_input( struct mbuf *m,
                     register struct lpxpcb *lpxp,
                     struct ether_header *frame_header);

int lpx_datagram_user_ctloutput( struct socket *so, struct sockopt *sopt );

extern lck_attr_t	*datagram_mtx_attr;      // mutex attributes
extern lck_grp_t    *datagram_mtx_grp;       // mutex group definition 

int lpx_datagram_lock(struct socket *so, int refcount, int lr);
int lpx_datagram_unlock(struct socket *so, int refcount, int lr);
lck_mtx_t *lpx_datagram_getlock(struct socket *so, int locktype);

#endif // _LPX_DATAGRAM_H_
