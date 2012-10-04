#ifndef _SMP_USRREQ_INT_H_
#define _SMP_USRREQL_INT_H_

/***************************************************************************
 *
 *  smp_usrreq_int.h
 *
 *    Internal definitions for smp_usrreq.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  Internal function prototypes
 *
 **************************************************************************/

static int smp_usr_attach(struct socket *so, int proto, struct proc *p);

static int smp_usr_detach(struct socket *so);

static int smp_usr_bind( struct socket *so,
                         struct sockaddr *nam,
                         struct proc *p );

static int smp_usr_listen(struct socket *so, struct proc *p);

static int smp_usr_connect( struct socket *so,
                            struct sockaddr *nam,
                            struct proc *td );

static int smp_usr_disconnect(struct socket *so);

static int smp_usr_accept(struct socket *so, struct sockaddr **nam);

static int smp_usr_shutdown(struct socket *so);

static int smp_usr_rcvd(struct socket *so, int flags);

static int smp_usr_send( struct socket *so,
                         int flags,
                         struct mbuf *m,
                         struct sockaddr *addr,
                         struct mbuf *controlp,
                         struct proc *td);

static int smp_usr_abort(struct socket *so);

static int smp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags);

static int smp_attach( struct socket *so, struct proc *td );

static struct smppcb *smp_disconnect( register struct smppcb *cb );

static struct smppcb *smp_usrclosed(register struct smppcb *cb);

static void smp_template(register struct smppcb *cb);

static int smp_output(register struct smppcb *cb, struct mbuf *m0);

static void smp_setpersist(register struct smppcb *cb);

/*
 * SMP timer processing.
 */
static struct smppcb *smp_timers(register struct smppcb *cb, int timer);

static int smp_reass(register struct smppcb *cb, register struct lpxhdr *lpxh);

static struct smppcb *smp_close(struct smppcb *cb);

struct smppcb *smp_drop(struct smppcb *cb, int errno);

#endif	/* !_SMP_USRREQ_INT_H_ */
