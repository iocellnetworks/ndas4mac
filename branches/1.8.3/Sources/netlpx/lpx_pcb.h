#ifndef _LPX_PCB_H_
#define _LPX_PCB_H_

/***************************************************************************
 *
 *  lpx_pch.h
 *
 *    API and data structure definitions for lpx_pcb.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)lpx_pcb.h
 *
 * $FreeBSD: src/sys/netlpx/lpx_pcb.h,v 1.18 2003/01/01 18:48:56 schweikh Exp $
 */

#include "lpx.h"

/*
 * LPX protocol interface control block.
 */
struct lpxpcb {
    struct  lpxpcb *lpxp_next;  /* doubly linked list */
    struct  lpxpcb *lpxp_prev;
    struct  lpxpcb *lpxp_head;
    struct  socket *lpxp_socket;    /* back pointer to socket */
    struct  lpx_addr lpxp_faddr;    /* destination address */
    struct  lpx_addr lpxp_laddr;    /* socket's address */
    caddr_t lpxp_pcb;       /* protocol specific stuff */
    struct  lpx_addr lpxp_lastdst;  /* validate cached route for dg socks*/
    u_long  lpxp_notify_param;  /* extra info passed via lpx_pcbnotify*/
    short   lpxp_flags;
    u_char  lpxp_dpt;       /* default packet type for lpx_USER_output */
    u_char  lpxp_rpt;       /* last received packet type by Lpx_USER_input() */

    u_char  lpxp_messageid;
		
	lck_mtx_t	*lpxp_mtx;   /* LPX pcb per-socket mutex */	
	lck_rw_t	*lpxp_list_rw;   /* PCB List lock. */	

	lck_grp_t *lpxp_mtx_grp;  
};

/* possible flags */

#define LPXP_IN_ABORT       0x1 /* calling abort through socket */
#define LPXP_RAWIN      0x2 /* show headers on input */
#define LPXP_RAWOUT     0x4 /* show header on output */
#define LPXP_ALL_PACKETS    0x8 /* Turn off higher proto processing */
#define LPXP_CHECKSUM       0x10    /* use checksum on this socket */
#define	LPXP_NEEDRELEASE	0x100

#define LPX_WILDCARD        1

#define lpxp_lport lpxp_laddr.x_port
#define lpxp_fport lpxp_faddr.x_port

#define sotolpxpcb(so)      ((struct lpxpcb *)((so)->so_pcb))

/*
 * Nominal space allocated to an LPX socket.
 */
#define LPXSNDQ     16384
#define LPXRCVQ     40960


#ifdef KERNEL

extern struct lpxpcb	lpx_datagram_pcb;		/* head of list */
extern struct lpxpcb	lpx_stream_pcb;

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

int Lpx_PCB_alloc( struct socket *so,
                   struct lpxpcb *head,
                   struct proc *td );
    
int Lpx_PCB_bind( register struct lpxpcb *lpxp,
                  struct sockaddr *nam,
                  struct proc *td);

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument slpx.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int Lpx_PCB_connect( struct lpxpcb *lpxp,
                     struct sockaddr *nam,
                     struct proc *td );

void Lpx_PCB_disconnect( struct lpxpcb *lpxp );

void Lpx_PCB_detach(struct lpxpcb *lpxp );

void Lpx_PCB_dispense(struct lpxpcb *lpxp );

struct lpxpcb * Lpx_PCB_lookup( struct lpx_addr *faddr,
								struct lpx_addr *laddr,
                                int wildp,
								struct lpxpcb	*head);

int Lpx_PCB_setsockaddr( register struct lpxpcb *lpxp,
                          struct sockaddr **nam );

int Lpx_PCB_setpeeraddr(register struct lpxpcb *lpxp,
                         struct sockaddr **nam );

#endif /* _KERNEL */

#endif /* !_LPX_PCB_H_ */
