#ifndef _SMP_USRREQ_H_
#define _SMP_USRREQ_H_

/***************************************************************************
 *
 *  smp_usrreq.h
 *
 *    API definitions for smp_usrreq.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

#ifdef KERNEL

void Smp_init();

int Smp_ctloutput( struct socket *so, struct sockopt *sopt );

void Smp_ctlinput( int cmd,
                   struct sockaddr *arg_as_sa, /* XXX should be swapped with dummy */
                   void *dummy );

void Smp_fasttimo();

void Smp_slowtimo();

#if 0
void Smp_input( register struct mbuf *m, register struct lpxpcb *lpxp );
#else /* if 0  */
void Smp_input( register struct mbuf *m, 
                register struct lpxpcb *lpxp,
                void *frame_header);

#endif /* if 0 else */

#endif /* _KERNEL */

#endif	/* !_SMP_USRREQ_H_ */
