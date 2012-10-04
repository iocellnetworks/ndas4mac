#ifndef _SMP_USRREQ_H_
#define _SMP_USRREQ_H_

/***************************************************************************
 *
 *  lpx_stream_usrreq.h
 *
 *    API definitions for lpx_stream_usrreq.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

/* Following was struct lpx_stream_stat lpx_stream_stat; */
#ifndef lpx_stream_stat 
#define lpx_stream_stat lpx_stream_istat.newstats
#endif  

extern struct   lpx_stream_istat lpx_stream_istat;
extern u_short  lpx_stream_iss;

#ifdef KERNEL

void lpx_stream_init();
void lpx_stream_free();

int Smp_ctloutput( struct socket *so, struct sockopt *sopt );

void Smp_ctlinput( int cmd,
                   struct sockaddr *arg_as_sa, /* XXX should be swapped with dummy */
                   void *dummy );

void lpx_stream_input( register struct mbuf *m, 
                register struct lpxpcb *lpxp,
                void *frame_header);


static int lpx_stream_user_attach(struct socket *so, int proto, struct proc *p);

static int lpx_stream_user_detach(struct socket *so);

static int lpx_stream_user_bind( struct socket *so,
                         struct sockaddr *nam,
                         struct proc *p );

static int lpx_stream_user_listen(struct socket *so, struct proc *p);

static int lpx_stream_user_connect( struct socket *so,
                            struct sockaddr *nam,
                            struct proc *td );

static int lpx_stream_user_disconnect(struct socket *so);

static int lpx_stream_user_accept(struct socket *so, struct sockaddr **nam);

static int lpx_stream_user_shutdown(struct socket *so);

static int lpx_stream_user_rcvd(struct socket *so, int flags);

static int lpx_stream_user_send( struct socket *so,
                         int flags,
                         struct mbuf *m,
                         struct sockaddr *addr,
                         struct mbuf *controlp,
                         struct proc *td);

static int lpx_stream_user_abort(struct socket *so);

static int lpx_stream_attach( struct socket *so, struct proc *td );

static struct stream_pcb *lpx_stream_disconnect( register struct stream_pcb *cb );

static struct stream_pcb *lpx_stream_usrclosed(register struct stream_pcb *cb);

static void lpx_stream_template(register struct stream_pcb *cb);

int lpx_stream_output(register struct stream_pcb *cb, struct mbuf *m0);

//static void lpx_stream_setpersist(register struct stream_pcb *cb);

/*
 * SMP timer processing.
 */
struct stream_pcb *lpx_stream_timers(register struct stream_pcb *cb, int timer);

static int lpx_stream_reass(register struct stream_pcb *cb, register struct lpxhdr *lpxh);

static struct stream_pcb *lpx_stream_close(struct stream_pcb *cb);

struct stream_pcb *lpx_stream_drop(struct stream_pcb *cb, int errno);

extern lck_attr_t			*stream_mtx_attr;      // mutex attributes
extern lck_grp_t           *stream_mtx_grp;       // mutex group definition 

int lpx_stream_lock(struct socket *so, int refcount, int lr);
int lpx_stream_unlock(struct socket *so, int refcount, int lr);
lck_mtx_t *lpx_stream_getlock(struct socket *so, int locktype);

#endif /* _KERNEL */

#endif	/* !_SMP_USRREQ_H_ */
