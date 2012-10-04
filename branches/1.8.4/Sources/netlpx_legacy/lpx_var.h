/***************************************************************************
 *
 *  lpx_var.h
 *
 *    
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
 *  @(#)lpx_var.h
 *
 * $FreeBSD: src/sys/netlpx/lpx_var.h,v 1.17 2003/03/04 23:19:53 jlemon Exp $
 */

#ifndef _NETLPX_LPX_VAR_H_
#define _NETLPX_LPX_VAR_H_

/*
 * LPX Kernel Structures and Variables
 */
struct  lpxstat {
    u_long  lpxs_total;     /* total packets received */
    u_long  lpxs_badsum;        /* checksum bad */
    u_long  lpxs_tooshort;      /* packet too short */
    u_long  lpxs_toosmall;      /* not enough data */
    u_long  lpxs_forward;       /* packets forwarded */
    u_long  lpxs_cantforward;   /* packets rcvd for unreachable dest */
    u_long  lpxs_delivered;     /* datagrams delivered to upper level*/
    u_long  lpxs_localout;      /* total lpx packets generated here */
    u_long  lpxs_odropped;      /* lost packets due to nobufs, etc. */
    u_long  lpxs_noroute;       /* packets discarded due to no route */
    u_long  lpxs_mtutoosmall;   /* the interface mtu is too small */
};

#ifdef KERNEL

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_lpx);
SYSCTL_DECL(_net_lpx_lpx);
#endif

extern int lpxcksum;
extern long lpx_pexseq;
extern struct lpxstat lpxstat;
extern struct lpxpcb lpxrawpcb;
extern struct pr_usrreqs lpx_usrreqs;
extern struct pr_usrreqs rlpx_usrreqs;
extern struct sockaddr_lpx lpx_netmask;
extern struct sockaddr_lpx lpx_hostmask;

extern union lpx_net lpx_zeronet;
extern union lpx_host lpx_zerohost;
extern union lpx_net lpx_broadnet;
extern union lpx_host lpx_broadhost;

struct ifnet;
struct lpx_addr;
struct mbuf;
struct proc;
struct route;
struct sockaddr;
struct socket;
struct sockopt;

extern u_int32_t	min_mtu;

#endif /* _KERNEL */

#endif /* !_NETLPX_LPX_VAR_H_ */
