
#ifndef _EXCLUDED_BY_APPLE_H_
#define _EXCLUDED_BY_APPLE_H_

////////////////////////////////////////////////////////////////////
// From protosw.h
///////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1998, 1999 Apple Computer, Inc. All Rights Reserved */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)protosw.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/sys/sys/protosw.h,v 1.28.2.2 2001/07/03 11:02:01 ume Exp $
 */

#include <sys/appleapiopts.h>
#include <sys/cdefs.h>

#define	PR_SLOWHZ	2		/* 2 slow timeouts per second */
#define	PR_FASTHZ	5		/* 5 fast timeouts per second */

//#ifdef PRIVATE

/* Forward declare these structures referenced from prototypes below. */
struct mbuf;
struct proc;
struct sockaddr;
struct socket;
struct sockopt;
struct socket_filter;

/*#ifdef _KERNEL*/
/*
 * Protocol switch table.
 *
 * Each protocol has a handle initializing one of these structures,
 * which is used for protocol-protocol and system-protocol communication.
 *
 * A protocol is called through the pr_init entry before any other.
 * Thereafter it is called every 200ms through the pr_fasttimo entry and
 * every 500ms through the pr_slowtimo for timer based actions.
 * The system will call the pr_drain entry if it is low on space and
 * this should throw away any non-critical data.
 *
 * Protocols pass data between themselves as chains of mbufs using
 * the pr_input and pr_output hooks.  Pr_input passes data up (towards
 * the users) and pr_output passes it down (towards the interfaces); control
 * information passes up and down on pr_ctlinput and pr_ctloutput.
 * The protocol is responsible for the space occupied by any the
 * arguments to these entries and must dispose it.
 *
 * The userreq routine interfaces protocols to the system and is
 * described below.
 */
 
#include <sys/socketvar.h>
#include <sys/queue.h>
#ifdef KERNEL
#include <kern/locks.h>
#endif /* KERNEL */

#if __DARWIN_ALIGN_POWER
#pragma options align=power
#endif

struct protosw {
	short	pr_type;		/* socket type used for */
	struct	domain *pr_domain;	/* domain protocol a member of */
	short	pr_protocol;		/* protocol number */
	unsigned int pr_flags;		/* see below */
/* protocol-protocol hooks */
	void	(*pr_input)(struct mbuf *, int len);
					/* input to protocol (from below) */
	int	(*pr_output)(struct mbuf *m, struct socket *so);
					/* output to protocol (from above) */
	void	(*pr_ctlinput)(int, struct sockaddr *, void *);
					/* control input (from below) */
	int	(*pr_ctloutput)(struct socket *, struct sockopt *);
					/* control output (from above) */
/* user-protocol hook */
	void	*pr_ousrreq;
/* utility hooks */
	void	(*pr_init)(void);	/* initialization hook */
	void	(*pr_fasttimo)(void);
					/* fast timeout (200ms) */
	void	(*pr_slowtimo)(void);
					/* slow timeout (500ms) */
	void	(*pr_drain)(void);
					/* flush any excess space possible */
#if __APPLE__
	int	(*pr_sysctl)(int *, u_int, void *, size_t *, void *, size_t);
					/* sysctl for protocol */
#endif
	struct	pr_usrreqs *pr_usrreqs;	/* supersedes pr_usrreq() */
#if __APPLE__
	int	(*pr_lock) 	(struct socket *so, int locktype, int debug); /* lock function for protocol */
	int	(*pr_unlock) 	(struct socket *so, int locktype, int debug); /* unlock for protocol */
#ifdef _KERN_LOCKS_H_
	lck_mtx_t *	(*pr_getlock) 	(struct socket *so, int locktype);
#else
	void *	(*pr_getlock) 	(struct socket *so, int locktype);
#endif
#endif
#if __APPLE__
/* Implant hooks */
	TAILQ_HEAD(, socket_filter) pr_filter_head;
	struct protosw *pr_next;	/* Chain for domain */
	u_long	reserved[1];		/* Padding for future use */
#endif
};

#if __DARWIN_ALIGN_POWER
#pragma options align=reset
#endif

/*
 * Values for pr_flags.
 * PR_ADDR requires PR_ATOMIC;
 * PR_ADDR and PR_CONNREQUIRED are mutually exclusive.
 * PR_IMPLOPCL means that the protocol allows sendto without prior connect,
 *	and the protocol understands the MSG_EOF flag.  The first property is
 *	is only relevant if PR_CONNREQUIRED is set (otherwise sendto is allowed
 *	anyhow).
 */
#define	PR_ATOMIC			0x01		/* exchange atomic messages only */
#define	PR_ADDR			0x02		/* addresses given with messages */
#define	PR_CONNREQUIRED	0x04		/* connection required by protocol */
#define	PR_WANTRCVD		0x08		/* want PRU_RCVD calls */
#define	PR_RIGHTS			0x10		/* passes capabilities */
#define	PR_IMPLOPCL		0x20		/* implied open/close */
#define	PR_LASTHDR		0x40		/* enforce ipsec policy; last header */
#define	PR_PROTOLOCK		0x80		/* protocol takes care of it's own locking */
#define	PR_PCBLOCK		0x100	/* protocol supports per pcb finer grain locking */
#define	PR_DISPOSE		0x200	/* protocol requires late lists disposal */

/*
 * The arguments to usrreq are:
 *	(*protosw[].pr_usrreq)(up, req, m, nam, opt);
 * where up is a (struct socket *), req is one of these requests,
 * m is a optional mbuf chain containing a message,
 * nam is an optional mbuf chain containing an address,
 * and opt is a pointer to a socketopt structure or nil.
 * The protocol is responsible for disposal of the mbuf chain m,
 * the caller is responsible for any space held by nam and opt.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define	PRU_ATTACH		0	/* attach protocol to up */
#define	PRU_DETACH		1	/* detach protocol from up */
#define	PRU_BIND		2	/* bind socket to address */
#define	PRU_LISTEN		3	/* listen for connection */
#define	PRU_CONNECT		4	/* establish connection to peer */
#define	PRU_ACCEPT		5	/* accept connection from peer */
#define	PRU_DISCONNECT		6	/* disconnect from peer */
#define	PRU_SHUTDOWN		7	/* won't send any more data */
#define	PRU_RCVD		8	/* have taken data; more room now */
#define	PRU_SEND		9	/* send this data */
#define	PRU_ABORT		10	/* abort (fast DISCONNECT, DETATCH) */
#define	PRU_CONTROL		11	/* control operations on protocol */
#define	PRU_SENSE		12	/* return status into m */
#define	PRU_RCVOOB		13	/* retrieve out of band data */
#define	PRU_SENDOOB		14	/* send out of band data */
#define	PRU_SOCKADDR		15	/* fetch socket's address */
#define	PRU_PEERADDR		16	/* fetch peer's address */
#define	PRU_CONNECT2		17	/* connect two sockets */
/* begin for protocols internal use */
#define	PRU_FASTTIMO		18	/* 200ms timeout */
#define	PRU_SLOWTIMO		19	/* 500ms timeout */
#define	PRU_PROTORCV		20	/* receive from below */
#define	PRU_PROTOSEND		21	/* send to below */
/* end for protocol's internal use */
#define PRU_SEND_EOF		22	/* send and close */
#define PRU_NREQ		22

#ifdef PRUREQUESTS
char *prurequests[] = {
	"ATTACH",	"DETACH",	"BIND",		"LISTEN",
	"CONNECT",	"ACCEPT",	"DISCONNECT",	"SHUTDOWN",
	"RCVD",		"SEND",		"ABORT",	"CONTROL",
	"SENSE",	"RCVOOB",	"SENDOOB",	"SOCKADDR",
	"PEERADDR",	"CONNECT2",	"FASTTIMO",	"SLOWTIMO",
	"PROTORCV",	"PROTOSEND",
	"SEND_EOF",
};
#endif

#ifdef	KERNEL			/* users shouldn't see this decl */

struct ifnet;
struct stat;
struct ucred;
struct uio;

/*
 * If the ordering here looks odd, that's because it's alphabetical.
 * Having this structure separated out from the main protoswitch is allegedly
 * a big (12 cycles per call) lose on high-end CPUs.  We will eventually
 * migrate this stuff back into the main structure.
 */
struct pr_usrreqs {
	int	(*pru_abort)(struct socket *so);
	int	(*pru_accept)(struct socket *so, struct sockaddr **nam);
	int	(*pru_attach)(struct socket *so, int proto, struct proc *p);
	int	(*pru_bind)(struct socket *so, struct sockaddr *nam,
				 struct proc *p);
	int	(*pru_connect)(struct socket *so, struct sockaddr *nam,
				    struct proc *p);
	int	(*pru_connect2)(struct socket *so1, struct socket *so2);
	int	(*pru_control)(struct socket *so, u_long cmd, caddr_t data,
				    struct ifnet *ifp, struct proc *p);
	int	(*pru_detach)(struct socket *so);
	int	(*pru_disconnect)(struct socket *so);
	int	(*pru_listen)(struct socket *so, struct proc *p);
	int	(*pru_peeraddr)(struct socket *so, struct sockaddr **nam);
	int	(*pru_rcvd)(struct socket *so, int flags);
	int	(*pru_rcvoob)(struct socket *so, struct mbuf *m, int flags);
	int	(*pru_send)(struct socket *so, int flags, struct mbuf *m, 
				 struct sockaddr *addr, struct mbuf *control,
				 struct proc *p);
#define	PRUS_OOB	0x1
#define	PRUS_EOF	0x2
#define	PRUS_MORETOCOME	0x4
	int	(*pru_sense)(struct socket *so, struct stat *sb);
	int	(*pru_shutdown)(struct socket *so);
	int	(*pru_sockaddr)(struct socket *so, struct sockaddr **nam);
	 
	/*
	 * These three added later, so they are out of order.  They are used
	 * for shortcutting (fast path input/output) in some protocols.
	 * XXX - that's a lie, they are not implemented yet
	 * Rather than calling sosend() etc. directly, calls are made
	 * through these entry points.  For protocols which still use
	 * the generic code, these just point to those routines.
	 */
	int	(*pru_sosend)(struct socket *so, struct sockaddr *addr,
				   struct uio *uio, struct mbuf *top,
				   struct mbuf *control, int flags);
	int	(*pru_soreceive)(struct socket *so, 
				      struct sockaddr **paddr,
				      struct uio *uio, struct mbuf **mp0,
				      struct mbuf **controlp, int *flagsp);
	int	(*pru_sopoll)(struct socket *so, int events, 
				   struct ucred *cred, void *);
};

__BEGIN_DECLS

extern int	pru_abort_notsupp(struct socket *so);
extern int	pru_accept_notsupp(struct socket *so, struct sockaddr **nam);
extern int	pru_attach_notsupp(struct socket *so, int proto,
				   struct proc *p);
extern int	pru_bind_notsupp(struct socket *so, struct sockaddr *nam,
				 struct proc *p);
extern int	pru_connect_notsupp(struct socket *so, struct sockaddr *nam,
				    struct proc *p);
extern int	pru_connect2_notsupp(struct socket *so1, struct socket *so2);
extern int	pru_control_notsupp(struct socket *so, u_long cmd, caddr_t data,
				    struct ifnet *ifp, struct proc *p);
extern int	pru_detach_notsupp(struct socket *so);
extern int	pru_disconnect_notsupp(struct socket *so);
extern int	pru_listen_notsupp(struct socket *so, struct proc *p);
extern int	pru_peeraddr_notsupp(struct socket *so, 
				     struct sockaddr **nam);
extern int	pru_rcvd_notsupp(struct socket *so, int flags);
extern int	pru_rcvoob_notsupp(struct socket *so, struct mbuf *m,
				   int flags);
extern int	pru_send_notsupp(struct socket *so, int flags, struct mbuf *m, 
				 struct sockaddr *addr, struct mbuf *control,
				 struct proc *p);
extern int	pru_sense_null(struct socket *so, struct stat *sb);
extern int	pru_shutdown_notsupp(struct socket *so);
extern int	pru_sockaddr_notsupp(struct socket *so, 
				     struct sockaddr **nam);
extern int	pru_sosend_notsupp(struct socket *so, struct sockaddr *addr,
				   struct uio *uio, struct mbuf *top,
				   struct mbuf *control, int flags);
extern int	pru_soreceive_notsupp(struct socket *so, 
				      struct sockaddr **paddr,
				      struct uio *uio, struct mbuf **mp0,
				      struct mbuf **controlp, int *flagsp);
extern int	pru_sopoll_notsupp(struct socket *so, int events, 
				   struct ucred *cred, void *);

__END_DECLS

#endif /* KERNEL */

/*
 * The arguments to the ctlinput routine are
 *	(*protosw[].pr_ctlinput)(cmd, sa, arg);
 * where cmd is one of the commands below, sa is a pointer to a sockaddr,
 * and arg is a `void *' argument used within a protocol family.
 */
#define	PRC_IFDOWN		0	/* interface transition */
#define	PRC_ROUTEDEAD		1	/* select new route if possible ??? */
#define	PRC_IFUP		2 	/* interface has come back up */
#define	PRC_QUENCH2		3	/* DEC congestion bit says slow down */
#define	PRC_QUENCH		4	/* some one said to slow down */
#define	PRC_MSGSIZE		5	/* message size forced drop */
#define	PRC_HOSTDEAD		6	/* host appears to be down */
#define	PRC_HOSTUNREACH		7	/* deprecated (use PRC_UNREACH_HOST) */
#define	PRC_UNREACH_NET		8	/* no route to network */
#define	PRC_UNREACH_HOST	9	/* no route to host */
#define	PRC_UNREACH_PROTOCOL	10	/* dst says bad protocol */
#define	PRC_UNREACH_PORT	11	/* bad port # */
/* was	PRC_UNREACH_NEEDFRAG	12	   (use PRC_MSGSIZE) */
#define	PRC_UNREACH_SRCFAIL	13	/* source route failed */
#define	PRC_REDIRECT_NET	14	/* net routing redirect */
#define	PRC_REDIRECT_HOST	15	/* host routing redirect */
#define	PRC_REDIRECT_TOSNET	16	/* redirect for type of service & net */
#define	PRC_REDIRECT_TOSHOST	17	/* redirect for tos & host */
#define	PRC_TIMXCEED_INTRANS	18	/* packet lifetime expired in transit */
#define	PRC_TIMXCEED_REASS	19	/* lifetime expired on reass q */
#define	PRC_PARAMPROB		20	/* header incorrect */
#define	PRC_UNREACH_ADMIN_PROHIB	21	/* packet administrativly prohibited */

#define	PRC_NCMDS		22

#define	PRC_IS_REDIRECT(cmd)	\
	((cmd) >= PRC_REDIRECT_NET && (cmd) <= PRC_REDIRECT_TOSHOST)

#ifdef PRCREQUESTS
char	*prcrequests[] = {
	"IFDOWN", "ROUTEDEAD", "IFUP", "DEC-BIT-QUENCH2",
	"QUENCH", "MSGSIZE", "HOSTDEAD", "#7",
	"NET-UNREACH", "HOST-UNREACH", "PROTO-UNREACH", "PORT-UNREACH",
	"#12", "SRCFAIL-UNREACH", "NET-REDIRECT", "HOST-REDIRECT",
	"TOSNET-REDIRECT", "TOSHOST-REDIRECT", "TX-INTRANS", "TX-REASS",
	"PARAMPROB", "ADMIN-UNREACH"
};
#endif

/*
 * The arguments to ctloutput are:
 *	(*protosw[].pr_ctloutput)(req, so, level, optname, optval, p);
 * req is one of the actions listed below, so is a (struct socket *),
 * level is an indication of which protocol layer the option is intended.
 * optname is a protocol dependent socket option request,
 * optval is a pointer to a mbuf-chain pointer, for value-return results.
 * The protocol is responsible for disposal of the mbuf chain *optval
 * if supplied,
 * the caller is responsible for any space held by *optval, when returned.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define	PRCO_GETOPT	0
#define	PRCO_SETOPT	1

#define	PRCO_NCMDS	2

#ifdef PRCOREQUESTS
char	*prcorequests[] = {
	"GETOPT", "SETOPT",
};
#endif

#ifdef KERNEL

__BEGIN_DECLS

void	pfctlinput(int, struct sockaddr *);
void	pfctlinput2(int, struct sockaddr *, void *);
struct protosw *pffindproto(int family, int protocol, int type);
struct protosw *pffindproto_locked(int family, int protocol, int type);
struct protosw *pffindtype(int family, int type);

extern int net_add_proto(struct protosw *, struct domain *);
extern int net_del_proto(int, int, struct domain *);

__END_DECLS

/* Temp hack to link static domains together */

#define LINK_PROTOS(psw) \
static void link_ ## psw ## _protos() \
{ \
      int i; \
		 \
    for (i=0; i < ((sizeof(psw)/sizeof(psw[0])) - 1); i++) \
	     psw[i].pr_next = &psw[i + 1]; \
} 

#endif

//#endif /* PRIVATE */

////////////////////////////////////////////////////////////////////
// From if_var.h
///////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	From: @(#)if.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_var.h,v 1.18.2.7 2001/07/24 19:10:18 brooks Exp $
 */

//#ifndef	_NET_IF_VAR_H_
//#define	_NET_IF_VAR_H_

#include <sys/appleapiopts.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>		/* get TAILQ macros */
#ifdef KERNEL_PRIVATE
#include <kern/locks.h>
#endif /* KERNEL_PRIVATE */

#ifdef KERNEL
#include <net/kpi_interface.h>
#endif KERNEL

#ifdef __APPLE__
#define APPLE_IF_FAM_LOOPBACK  1
#define APPLE_IF_FAM_ETHERNET  2
#define APPLE_IF_FAM_SLIP      3
#define APPLE_IF_FAM_TUN       4
#define APPLE_IF_FAM_VLAN      5
#define APPLE_IF_FAM_PPP       6
#define APPLE_IF_FAM_PVC       7
#define APPLE_IF_FAM_DISC      8
#define APPLE_IF_FAM_MDECAP    9
#define APPLE_IF_FAM_GIF       10
#define APPLE_IF_FAM_FAITH     11
#define APPLE_IF_FAM_STF       12
#define APPLE_IF_FAM_FIREWIRE  13
#define APPLE_IF_FAM_BOND      14
#endif __APPLE__

/*
 * 72 was chosen below because it is the size of a TCP/IP
 * header (40) + the minimum mss (32).
 */
#define	IF_MINMTU	72
#define	IF_MAXMTU	65535

/*
 * Structures defining a network interface, providing a packet
 * transport mechanism (ala level 0 of the PUP protocols).
 *
 * Each interface accepts output datagrams of a specified maximum
 * length, and provides higher level routines with input datagrams
 * received from its medium.
 *
 * Output occurs when the routine if_output is called, with three parameters:
 *	(*ifp->if_output)(ifp, m, dst, rt)
 * Here m is the mbuf chain to be sent and dst is the destination address.
 * The output routine encapsulates the supplied datagram if necessary,
 * and then transmits it on its medium.
 *
 * On input, each interface unwraps the data received by it, and either
 * places it on the input queue of a internetwork datagram routine
 * and posts the associated software interrupt, or passes the datagram to a raw
 * packet input routine.
 *
 * Routines exist for locating interfaces by their addresses
 * or for locating a interface on a certain network, as well as more general
 * routing and gateway routines maintaining information used to locate
 * interfaces.  These routines live in the files if.c and route.c
 */

#define	IFNAMSIZ	16

/* This belongs up in socket.h or socketvar.h, depending on how far the
 *   event bubbles up.
 */
/*
struct net_event_data {
     unsigned long	if_family;
     unsigned long	if_unit;
     char		if_name[IFNAMSIZ];
};
*/
/*
 * Structure describing information about an interface
 * which may be of interest to management entities.
 */

//struct if_data {
	/* generic interface information */
//	unsigned char	ifi_type;	/* ethernet, tokenring, etc */
//#ifdef __APPLE__
//	unsigned char	ifi_typelen;	/* Length of frame type id */
//#endif
//	unsigned char	ifi_physical;	/* e.g., AUI, Thinnet, 10base-T, etc */
//	unsigned char	ifi_addrlen;	/* media address length */
//	unsigned char	ifi_hdrlen;	/* media header length */
//	unsigned char	ifi_recvquota;	/* polling quota for receive intrs */
//	unsigned char	ifi_xmitquota;	/* polling quota for xmit intrs */
  //  	unsigned char	ifi_unused1;	/* for future use */
//	unsigned long	ifi_mtu;	/* maximum transmission unit */
//	unsigned long	ifi_metric;	/* routing metric (external only) */
//	unsigned long	ifi_baudrate;	/* linespeed */
	/* volatile statistics */
//	unsigned long	ifi_ipackets;	/* packets received on interface */
//	unsigned long	ifi_ierrors;	/* input errors on interface */
//	unsigned long	ifi_opackets;	/* packets sent on interface */
//	unsigned long	ifi_oerrors;	/* output errors on interface */
//	unsigned long	ifi_collisions;	/* collisions on csma interfaces */
//	unsigned long	ifi_ibytes;	/* total number of octets received */
//	unsigned long	ifi_obytes;	/* total number of octets sent */
//	unsigned long	ifi_imcasts;	/* packets received via multicast */
//	unsigned long	ifi_omcasts;	/* packets sent via multicast */
//	unsigned long	ifi_iqdrops;	/* dropped on input, this interface */
//	unsigned long	ifi_noproto;	/* destined for unsupported protocol */
//	unsigned long	ifi_recvtiming;	/* usec spent receiving when timing */
//	unsigned long	ifi_xmittiming;	/* usec spent xmitting when timing */
//	struct	timeval ifi_lastchange;	/* time of last administrative change */
//	unsigned long	ifi_unused2;	/* used to be the default_proto */
//	unsigned long	ifi_hwassist;	/* HW offload capabilities */
//	unsigned long	ifi_reserved1;	/* for future use */
//	unsigned long	ifi_reserved2;	/* for future use */
//};

/*
 * Structure describing information about an interface
 * which may be of interest to management entities.
 */
//struct if_data64 {
	/* generic interface information */
//	u_char	ifi_type;		/* ethernet, tokenring, etc */
//#ifdef __APPLE__
//	u_char	ifi_typelen;		/* Length of frame type id */
//#endif
//	u_char		ifi_physical;		/* e.g., AUI, Thinnet, 10base-T, etc */
//	u_char		ifi_addrlen;		/* media address length */
//	u_char		ifi_hdrlen;		/* media header length */
//	u_char		ifi_recvquota;		/* polling quota for receive intrs */
//	u_char		ifi_xmitquota;		/* polling quota for xmit intrs */
//	u_char		ifi_unused1;		/* for future use */
//	u_long		ifi_mtu;		/* maximum transmission unit */
//	u_long		ifi_metric;		/* routing metric (external only) */
//	u_int64_t	ifi_baudrate;		/* linespeed */
	/* volatile statistics */
//	u_int64_t	ifi_ipackets;		/* packets received on interface */
//	u_int64_t	ifi_ierrors;		/* input errors on interface */
//	u_int64_t	ifi_opackets;		/* packets sent on interface */
//	u_int64_t	ifi_oerrors;		/* output errors on interface */
//	u_int64_t	ifi_collisions;		/* collisions on csma interfaces */
//	u_int64_t	ifi_ibytes;		/* total number of octets received */
//	u_int64_t	ifi_obytes;		/* total number of octets sent */
//	u_int64_t	ifi_imcasts;		/* packets received via multicast */
//	u_int64_t	ifi_omcasts;		/* packets sent via multicast */
//	u_int64_t	ifi_iqdrops;		/* dropped on input, this interface */
//	u_int64_t	ifi_noproto;		/* destined for unsupported protocol */
//	u_long		ifi_recvtiming;		/* usec spent receiving when timing */
//	u_long		ifi_xmittiming;		/* usec spent xmitting when timing */
//	struct	timeval ifi_lastchange;	/* time of last administrative change */
//};

//#ifdef PRIVATE
/*
 * Internal storage of if_data. This is bound to change. Various places in the
 * stack will translate this data structure in to the externally visible
 * if_data structure above.
 */
struct if_data_internal {
	/* generic interface information */
	u_char	ifi_type;		/* ethernet, tokenring, etc */
	u_char	ifi_typelen;	/* Length of frame type id */
	u_char	ifi_physical;	/* e.g., AUI, Thinnet, 10base-T, etc */
	u_char	ifi_addrlen;	/* media address length */
	u_char	ifi_hdrlen;		/* media header length */
	u_char	ifi_recvquota;	/* polling quota for receive intrs */
	u_char	ifi_xmitquota;	/* polling quota for xmit intrs */
    u_char	ifi_unused1;	/* for future use */
	u_long	ifi_mtu;		/* maximum transmission unit */
	u_long	ifi_metric;		/* routing metric (external only) */
	u_long	ifi_baudrate;	/* linespeed */
	/* volatile statistics */
	u_int64_t	ifi_ipackets;	/* packets received on interface */
	u_int64_t	ifi_ierrors;	/* input errors on interface */
	u_int64_t	ifi_opackets;	/* packets sent on interface */
	u_int64_t	ifi_oerrors;	/* output errors on interface */
	u_int64_t	ifi_collisions;	/* collisions on csma interfaces */
	u_int64_t	ifi_ibytes;		/* total number of octets received */
	u_int64_t	ifi_obytes;		/* total number of octets sent */
	u_int64_t	ifi_imcasts;	/* packets received via multicast */
	u_int64_t	ifi_omcasts;	/* packets sent via multicast */
	u_int64_t	ifi_iqdrops;	/* dropped on input, this interface */
	u_int64_t	ifi_noproto;	/* destined for unsupported protocol */
	u_long	ifi_recvtiming;		/* usec spent receiving when timing */
	u_long	ifi_xmittiming;		/* usec spent xmitting when timing */
#define IF_LASTCHANGEUPTIME	1	/* lastchange: 1-uptime 0-calendar time */
	struct	timeval ifi_lastchange;	/* time of last administrative change */
	u_long	ifi_hwassist;		/* HW offload capabilities */
};

#define	if_mtu		if_data.ifi_mtu
#define	if_type		if_data.ifi_type
#define if_typelen	if_data.ifi_typelen
#define if_physical	if_data.ifi_physical
#define	if_addrlen	if_data.ifi_addrlen
#define	if_hdrlen	if_data.ifi_hdrlen
#define	if_metric	if_data.ifi_metric
#define	if_baudrate	if_data.ifi_baudrate
#define	if_hwassist	if_data.ifi_hwassist
#define	if_ipackets	if_data.ifi_ipackets
#define	if_ierrors	if_data.ifi_ierrors
#define	if_opackets	if_data.ifi_opackets
#define	if_oerrors	if_data.ifi_oerrors
#define	if_collisions	if_data.ifi_collisions
#define	if_ibytes	if_data.ifi_ibytes
#define	if_obytes	if_data.ifi_obytes
#define	if_imcasts	if_data.ifi_imcasts
#define	if_omcasts	if_data.ifi_omcasts
#define	if_iqdrops	if_data.ifi_iqdrops
#define	if_noproto	if_data.ifi_noproto
#define	if_lastchange	if_data.ifi_lastchange
#define if_recvquota	if_data.ifi_recvquota
#define	if_xmitquota	if_data.ifi_xmitquota
#define if_iflags	if_data.ifi_iflags

struct	mbuf;
struct ifaddr;
TAILQ_HEAD(ifnethead, ifnet);	/* we use TAILQs so that the order of */
TAILQ_HEAD(ifaddrhead, ifaddr);	/* instantiation is preserved in the list */
TAILQ_HEAD(ifprefixhead, ifprefix);
LIST_HEAD(ifmultihead, ifmultiaddr);
struct tqdummy;
TAILQ_HEAD(tailq_head, tqdummy);

/*
 * Forward structure declarations for function prototypes [sic].
 */
struct	proc;
struct	rtentry;
struct	socket;
struct	ether_header;
struct  sockaddr_dl;
struct ifnet_filter;

TAILQ_HEAD(ifnet_filter_head, ifnet_filter);
TAILQ_HEAD(ddesc_head_name, dlil_demux_desc);

/* bottom 16 bits reserved for hardware checksum */
#define IF_HWASSIST_CSUM_IP		0x0001	/* will csum IP */
#define IF_HWASSIST_CSUM_TCP		0x0002	/* will csum TCP */
#define IF_HWASSIST_CSUM_UDP		0x0004	/* will csum UDP */
#define IF_HWASSIST_CSUM_IP_FRAGS	0x0008	/* will csum IP fragments */
#define IF_HWASSIST_CSUM_FRAGMENT	0x0010  /* will do IP fragmentation */
#define IF_HWASSIST_CSUM_TCP_SUM16	0x1000	/* simple TCP Sum16 computation */
#define IF_HWASSIST_CSUM_MASK		0xffff
#define IF_HWASSIST_CSUM_FLAGS(hwassist)	((hwassist) & IF_HWASSIST_CSUM_MASK)

/* VLAN support */
#define IF_HWASSIST_VLAN_TAGGING	0x10000	/* supports VLAN tagging */
#define IF_HWASSIST_VLAN_MTU		0x20000 /* supports VLAN MTU-sized packet (for software VLAN) */

#define IFNET_RW_LOCK 1

/*
 * Structure defining a queue for a network interface.
 */
struct	ifqueue {
	void *ifq_head;
	void *ifq_tail;
	int	ifq_len;
	int	ifq_maxlen;
	int	ifq_drops;
};

struct ddesc_head_str;
struct proto_hash_entry;
struct kev_msg;

/*
 * Structure defining a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */
struct ifnet {
	void	*if_softc;		/* pointer to driver state */
	const char	*if_name;		/* name, e.g. ``en'' or ``lo'' */
	TAILQ_ENTRY(ifnet) if_link; 	/* all struct ifnets are chained */
	struct	ifaddrhead if_addrhead;	/* linked list of addresses per if */
	u_long	if_refcnt;
#ifdef __KPI_INTERFACE__
	ifnet_check_multi	if_check_multi;
#else
	void*				if_check_multi;
#endif __KPI_INTERFACE__
	int	if_pcount;		/* number of promiscuous listeners */
	struct	bpf_if *if_bpf;		/* packet filter structure */
	u_short	if_index;		/* numeric abbreviation for this if  */
	short	if_unit;		/* sub-unit for lower level driver */
	short	if_timer;		/* time 'til if_watchdog called */
	short	if_flags;		/* up/down, broadcast, etc. */
	int	if_ipending;		/* interrupts pending */
	void	*if_linkmib;		/* link-type-specific MIB data */
	size_t	if_linkmiblen;		/* length of above data */
	struct	if_data_internal if_data;

/* New with DLIL */
#ifdef BSD_KERNEL_PRIVATE
	int	if_usecnt;
#else
	int	refcnt;
#endif
	int	offercnt;
#ifdef __KPI_INTERFACE__
	ifnet_output_func	if_output;
	ifnet_ioctl_func	if_ioctl;
	ifnet_set_bpf_tap	if_set_bpf_tap;
	ifnet_detached_func	if_free;
	ifnet_demux_func	if_demux;
	ifnet_event_func	if_event;
	ifnet_framer_func	if_framer;
	ifnet_family_t		if_family;		/* ulong assigned by Apple */
#else
	void*				if_output;
	void*				if_ioctl;
	void*				if_set_bpf_tap;
	void*				if_free;
	void*				if_demux;
	void*				if_event;
	void*				if_framer;
	u_long				if_family;		/* ulong assigned by Apple */
#endif

	struct ifnet_filter_head if_flt_head;

/* End DLIL specific */

	u_long 	if_delayed_detach; /* need to perform delayed detach */
	void    *if_private;	/* private to interface */
	long	if_eflags;		/* autoaddr, autoaddr done, etc. */

	struct	ifmultihead if_multiaddrs; /* multicast addresses configured */
	int	if_amcount;		/* number of all-multicast requests */
/* procedure handles */
#ifdef __KPI_INTERFACE__
	union {
		int	(*original)(struct ifnet *ifp, u_long protocol_family,
			struct ddesc_head_str *demux_desc_head);
		ifnet_add_proto_func	kpi;
	} if_add_proto_u;
	ifnet_del_proto_func	if_del_proto;
#else __KPI_INTERFACE__
	void*	if_add_proto;
	void*	if_del_proto;
#endif __KPI_INTERFACE__
	struct proto_hash_entry	*if_proto_hash;
	void					*if_kpi_storage;
	
	void	*unused_was_init;
	void	*unused_was_resolvemulti;
	
	struct ifqueue	if_snd;
	u_long 	unused_2[1];
#ifdef __APPLE__
	u_long	family_cookie;
	struct	ifprefixhead if_prefixhead; /* list of prefixes per if */

#ifdef _KERN_LOCKS_H_
#if IFNET_RW_LOCK
	lck_rw_t *if_lock;		/* Lock to protect this interface */
#else
	lck_mtx_t *if_lock;		/* Lock to protect this interface */
#endif
#else
	void	*if_lock;
#endif

#else
	struct	ifprefixhead if_prefixhead; /* list of prefixes per if */
#endif /* __APPLE__ */
	struct {
		u_long	length;
		union {
			u_char	buffer[8];
			u_char	*ptr;
		} u;
	} if_broadcast;
};

#define if_add_proto	if_add_proto_u.original

#ifndef __APPLE__
/* for compatibility with other BSDs */
#define	if_addrlist	if_addrhead
#define	if_list		if_link
#endif !__APPLE__


//#endif /* PRIVATE */

//#ifdef KERNEL_PRIVATE
/*
 * Structure describing a `cloning' interface.
 */
struct if_clone {
	LIST_ENTRY(if_clone) ifc_list;	/* on list of cloners */
	const char *ifc_name;			/* name of device, e.g. `vlan' */
	size_t ifc_namelen;		/* length of name */
	int ifc_minifs;			/* minimum number of interfaces */
	int ifc_maxunit;		/* maximum unit number */
	unsigned char *ifc_units;	/* bitmap to handle units */
	int ifc_bmlen;			/* bitmap length */

	int	(*ifc_create)(struct if_clone *, int);
	void	(*ifc_destroy)(struct ifnet *);
};

#define IF_CLONE_INITIALIZER(name, create, destroy, minifs, maxunit)	\
    { { 0, 0 }, name, sizeof(name) - 1, minifs, maxunit, NULL, 0, create, destroy }

/*
 * Bit values in if_ipending
 */
#define	IFI_RECV	1	/* I want to receive */
#define	IFI_XMIT	2	/* I want to transmit */

/*
 * Output queues (ifp->if_snd) and slow device input queues (*ifp->if_slowq)
 * are queues of messages stored on ifqueue structures
 * (defined above).  Entries are added to and deleted from these structures
 * by these macros, which should be called with ipl raised to splimp().
 */
#define	IF_QFULL(ifq)		((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define	IF_DROP(ifq)		((ifq)->ifq_drops++)
#define	IF_ENQUEUE(ifq, m) { \
	(m)->m_nextpkt = 0; \
	if ((ifq)->ifq_tail == 0) \
		(ifq)->ifq_head = m; \
	else \
		((struct mbuf*)(ifq)->ifq_tail)->m_nextpkt = m; \
	(ifq)->ifq_tail = m; \
	(ifq)->ifq_len++; \
}
#define	IF_PREPEND(ifq, m) { \
	(m)->m_nextpkt = (ifq)->ifq_head; \
	if ((ifq)->ifq_tail == 0) \
		(ifq)->ifq_tail = (m); \
	(ifq)->ifq_head = (m); \
	(ifq)->ifq_len++; \
}
#define	IF_DEQUEUE(ifq, m) { \
	(m) = (ifq)->ifq_head; \
	if (m) { \
		if (((ifq)->ifq_head = (m)->m_nextpkt) == 0) \
			(ifq)->ifq_tail = 0; \
		(m)->m_nextpkt = 0; \
		(ifq)->ifq_len--; \
	} \
}

#define	IF_ENQ_DROP(ifq, m)	if_enq_drop(ifq, m)

#if defined(__GNUC__) && defined(MT_HEADER)
static __inline int
if_queue_drop(struct ifqueue *ifq, __unused struct mbuf *m)
{
	IF_DROP(ifq);
	return 0;
}
/*
static __inline int
if_enq_drop(struct ifqueue *ifq, struct mbuf *m)
{
	if (IF_QFULL(ifq) &&
	    !if_queue_drop(ifq, m))
		return 0;
	IF_ENQUEUE(ifq, m);
	return 1;
}
*/
#else

#ifdef MT_HEADER
int	if_enq_drop(struct ifqueue *, struct mbuf *);
#endif MT_HEADER

#endif defined(__GNUC__) && defined(MT_HEADER)

//#endif /* KERNEL_PRIVATE */


//#ifdef PRIVATE
/*
 * The ifaddr structure contains information about one address
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an address is set, and are linked
 * together so all addresses for an interface can be located.
 */
struct ifaddr {
	struct	sockaddr *ifa_addr;	/* address of interface */
	struct	sockaddr *ifa_dstaddr;	/* other end of p-to-p link */
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
	struct	sockaddr *ifa_netmask;	/* used to determine subnet */
	struct	ifnet *ifa_ifp;		/* back-pointer to interface */
	TAILQ_ENTRY(ifaddr) ifa_link;	/* queue macro glue */
	void	(*ifa_rtrequest)	/* check or clean routes (+ or -)'d */
		(int, struct rtentry *, struct sockaddr *);
	u_short	ifa_flags;		/* mostly rt_flags for cloning */
	int	ifa_refcnt;/* 32bit ref count, use ifaref, ifafree */
	int	ifa_metric;		/* cost of going out this interface */
#ifdef notdef
	struct	rtentry *ifa_rt;	/* XXXX for ROUTETOIF ????? */
#endif
	int (*ifa_claim_addr)		/* check if an addr goes to this if */
		(struct ifaddr *, const struct sockaddr *);
	u_long	ifa_debug;		/* debug flags */
};
#define	IFA_ROUTE	RTF_UP		/* route installed (0x1) */
#define	IFA_CLONING	RTF_CLONING	/* (0x100) */
#define IFA_ATTACHED 	0x1		/* ifa_debug: IFA is attached to an interface */

//#endif /* PRIVATE */

#ifdef KERNEL_PRIVATE
/*
 * The prefix structure contains information about one prefix
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an prefix or an address is set,
 * and are linked together so all prefixes for an interface can be located.
 */
struct ifprefix {
	struct	sockaddr *ifpr_prefix;	/* prefix of interface */
	struct	ifnet *ifpr_ifp;	/* back-pointer to interface */
	TAILQ_ENTRY(ifprefix) ifpr_list; /* queue macro glue */
	u_char	ifpr_plen;		/* prefix length in bits */
	u_char	ifpr_type;		/* protocol dependent prefix type */
};
#endif /* KERNEL_PRIVATE */

//#ifdef PRIVATE
typedef void (*ifma_protospec_free_func)(void* ifma_protospec);

/*
 * Multicast address structure.  This is analogous to the ifaddr
 * structure except that it keeps track of multicast addresses.
 * Also, the reference count here is a count of requests for this
 * address, not a count of pointers to this structure.
 */
struct ifmultiaddr {
	LIST_ENTRY(ifmultiaddr) ifma_link; /* queue macro glue */
	struct	sockaddr *ifma_addr; 	/* address this membership is for */
	struct ifmultiaddr *ifma_ll;	/* link-layer translation, if any */
	struct	ifnet *ifma_ifp;		/* back-pointer to interface */
	u_int	ifma_usecount;			/* use count, protected by ifp's lock */
	void	*ifma_protospec;		/* protocol-specific state, if any */
	int32_t	ifma_refcount;			/* reference count, atomically protected */
	ifma_protospec_free_func ifma_free;	/* function called to free ifma_protospec */
};
//#endif /* PRIVATE */

#ifdef KERNEL_PRIVATE
#define IFAREF(ifa) ifaref(ifa)
#define IFAFREE(ifa) ifafree(ifa)

/*
 * To preserve kmem compatibility, we define
 * ifnet_head to ifnet. This should be temp.
 */
#define ifnet_head ifnet
extern	struct ifnethead ifnet_head;
extern struct	ifnet	**ifindex2ifnet;
extern	int ifqmaxlen;
extern	struct ifnet loif[];
extern	int if_index;
extern	struct ifaddr **ifnet_addrs;
extern struct ifnet *lo_ifp;

int	if_addmulti(struct ifnet *, const struct sockaddr *, struct ifmultiaddr **);
int	if_allmulti(struct ifnet *, int);
void	if_attach(struct ifnet *);
int	if_delmultiaddr(struct ifmultiaddr *ifma, int locked);
int	if_delmulti(struct ifnet *, const struct sockaddr *);
void	if_down(struct ifnet *);
void	if_route(struct ifnet *, int flag, int fam);
void	if_unroute(struct ifnet *, int flag, int fam);
void	if_up(struct ifnet *);
void	if_updown(struct ifnet *ifp, int up);
/*void	ifinit(void));*/ /* declared in systm.h for main( */
int	ifioctl(struct socket *, u_long, caddr_t, struct proc *);
int	ifioctllocked(struct socket *, u_long, caddr_t, struct proc *);
struct	ifnet *ifunit(const char *);
struct  ifnet *if_withname(struct sockaddr *);

void	if_clone_attach(struct if_clone *);
void	if_clone_detach(struct if_clone *);

void	ifnet_lock_assert(struct ifnet *ifp, int what);
void	ifnet_lock_shared(struct ifnet *ifp);
void	ifnet_lock_exclusive(struct ifnet *ifp);
void	ifnet_lock_done(struct ifnet *ifp);

void	ifnet_head_lock_shared(void);
void	ifnet_head_lock_exclusive(void);
void	ifnet_head_done(void);

void	if_attach_ifa(struct ifnet * ifp, struct ifaddr *ifa);
void	if_detach_ifa(struct ifnet * ifp, struct ifaddr *ifa);

void	ifma_reference(struct ifmultiaddr *ifma);
void	ifma_release(struct ifmultiaddr *ifma);

struct	ifaddr *ifa_ifwithaddr(const struct sockaddr *);
struct	ifaddr *ifa_ifwithdstaddr(const struct sockaddr *);
struct	ifaddr *ifa_ifwithnet(const struct sockaddr *);
struct	ifaddr *ifa_ifwithroute(int, const struct sockaddr *, const struct sockaddr *);
struct	ifaddr *ifaof_ifpforaddr(const struct sockaddr *, struct ifnet *);
void	ifafree(struct ifaddr *);
void	ifaref(struct ifaddr *);

struct	ifmultiaddr *ifmaof_ifpforaddr(const struct sockaddr *, struct ifnet *);

#ifdef BSD_KERNEL_PRIVATE
void	if_data_internal_to_if_data(const struct if_data_internal *if_data_int,
							   struct if_data *if_data);
void	if_data_internal_to_if_data64(const struct if_data_internal *if_data_int,
							   struct if_data64 *if_data64);
#endif /* BSD_KERNEL_PRIVATE */
#endif /* KERNEL_PRIVATE */
//#endif /* !_NET_IF_VAR_H_ */

////////////////////////////////////////////////////////////////////
// From radix.h
///////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
															   * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)radix.h	8.2 (Berkeley) 10/31/94
 * $FreeBSD: src/sys/net/radix.h,v 1.16.2.1 2000/05/03 19:17:11 wollman Exp $
 */

//#ifndef _RADIX_H_
//#define	_RADIX_H_
#include <sys/appleapiopts.h>

//#ifdef PRIVATE

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_RTABLE);
#endif

/*
 * Radix search tree node layout.
 */

struct radix_node {
	struct	radix_mask *rn_mklist;	/* list of masks contained in subtree */
	struct	radix_node *rn_parent;	/* parent */
	short	rn_bit;			/* bit offset; -1-index(netmask) */
	char	rn_bmask;		/* node: mask for bit test*/
	u_char	rn_flags;		/* enumerated next */
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {			/* leaf only data: */
			caddr_t	rn_Key;		/* object of search */
			caddr_t	rn_Mask;	/* netmask, if present */
			struct	radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			/* node only data: */
			int	rn_Off;		/* where to start compare */
			struct	radix_node *rn_L;/* progeny */
				struct	radix_node *rn_R;/* progeny */
		} rn_node;
	}		rn_u;
#ifdef RN_DEBUG
int rn_info;
struct radix_node *rn_twin;
struct radix_node *rn_ybro;
#endif
};

#define	rn_dupedkey	rn_u.rn_leaf.rn_Dupedkey
#define	rn_key		rn_u.rn_leaf.rn_Key
#define	rn_mask		rn_u.rn_leaf.rn_Mask
#define	rn_offset	rn_u.rn_node.rn_Off
#define	rn_left		rn_u.rn_node.rn_L
#define	rn_right	rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

struct radix_mask {
	short	rm_bit;			/* bit offset; -1-index(netmask) */
	char	rm_unused;		/* cf. rn_bmask */
	u_char	rm_flags;		/* cf. rn_flags */
	struct	radix_mask *rm_mklist;	/* more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}	rm_rmu;
	int	rm_refs;		/* # of references to this struct */
};

#define	rm_mask rm_rmu.rmu_mask
#define	rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */


#define MKGet(m) {\
	if (rn_mkfreelist) {\
		m = rn_mkfreelist; \
			rn_mkfreelist = (m)->rm_mklist; \
	} else \
		R_Malloc(m, struct radix_mask *, sizeof (*(m))); }\

#define MKFree(m) { (m)->rm_mklist = rn_mkfreelist; rn_mkfreelist = (m);}

typedef int walktree_f_t(struct radix_node *, void *);

struct radix_node_head {
	struct	radix_node *rnh_treetop;
	int	rnh_addrsize;		/* permit, but not require fixed keys */
	int	rnh_pktsize;		/* permit, but not require fixed keys */
	struct	radix_node *(*rnh_addaddr)	/* add based on sockaddr */
		(void *v, void *mask,
		 struct radix_node_head *head, struct radix_node nodes[]);
	struct	radix_node *(*rnh_addpkt)	/* add based on packet hdr */
		(void *v, void *mask,
		 struct radix_node_head *head, struct radix_node nodes[]);
	struct	radix_node *(*rnh_deladdr)	/* remove based on sockaddr */
		(void *v, void *mask, struct radix_node_head *head);
	struct	radix_node *(*rnh_delpkt)	/* remove based on packet hdr */
		(void *v, void *mask, struct radix_node_head *head);
	struct	radix_node *(*rnh_matchaddr)	/* locate based on sockaddr */
		(void *v, struct radix_node_head *head);
	struct	radix_node *(*rnh_lookup)	/* locate based on sockaddr */
		(void *v, void *mask, struct radix_node_head *head);
	struct	radix_node *(*rnh_matchpkt)	/* locate based on packet hdr */
		(void *v, struct radix_node_head *head);
	int	(*rnh_walktree)			/* traverse tree */
		(struct radix_node_head *head, walktree_f_t *f, void *w);
	int	(*rnh_walktree_from)		/* traverse tree below a */
		(struct radix_node_head *head, void *a, void *m,
		 walktree_f_t *f, void *w);
	void	(*rnh_close)	/* do something when the last ref drops */
		(struct radix_node *rn, struct radix_node_head *head);
	struct	radix_node rnh_nodes[3];	/* empty tree for common case */
};

#ifndef KERNEL
#define Bcmp(a, b, n) bcmp(((char *)(a)), ((char *)(b)), (n))
#define Bcopy(a, b, n) bcopy(((char *)(a)), ((char *)(b)), (unsigned)(n))
#define Bzero(p, n) bzero((char *)(p), (int)(n));
#define R_Malloc(p, t, n) (p = (t) malloc((unsigned int)(n)))
#define R_Free(p) free((char *)p);
#else
#define Bcmp(a, b, n) bcmp(((caddr_t)(a)), ((caddr_t)(b)), (unsigned)(n))
#define Bcopy(a, b, n) bcopy(((caddr_t)(a)), ((caddr_t)(b)), (unsigned)(n))
#define Bzero(p, n) bzero((caddr_t)(p), (unsigned)(n));
#define R_Malloc(p, t, n) (p = (t) _MALLOC((unsigned long)(n), M_RTABLE, M_WAITOK))
#define R_Free(p) FREE((caddr_t)p, M_RTABLE);
#endif /*KERNEL*/

void	 rn_init(void);
int	 rn_inithead(void **, int);
int	 rn_refines(void *, void *);
struct radix_node
*rn_addmask(void *, int, int),
*rn_addroute(void *, void *, struct radix_node_head *,
			 struct radix_node [2]),
*rn_delete(void *, void *, struct radix_node_head *),
*rn_lookup(void *v_arg, void *m_arg, struct radix_node_head *head),
*rn_match(void *, struct radix_node_head *);

//#endif /* PRIVATE */
//#endif /* _RADIX_H_ */

////////////////////////////////////////////////////////////////////
// From route.h
///////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)route.h	8.3 (Berkeley) 4/19/94
 * $FreeBSD: src/sys/net/route.h,v 1.36.2.1 2000/08/16 06:14:23 jayanth Exp $
 */

//#ifndef _NET_ROUTE_H_
//#define _NET_ROUTE_H_
#include <sys/appleapiopts.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
 * Kernel resident routing tables.
 *
 * The routing tables are initialized when interface addresses
 * are set by making entries for all directly connected interfaces.
 */

/*
 * A route consists of a destination address and a reference
 * to a routing entry.  These are often held by protocols
 * in their control blocks, e.g. inpcb.
 */
//#ifdef PRIVATE
struct  rtentry;
struct route {
	struct	rtentry *ro_rt;
	struct	sockaddr ro_dst;
	u_long	reserved[2];	/* for future use if needed */
};
//#else
struct route;
//#endif /* PRIVATE */

/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
//struct rt_metrics {
//	u_long	rmx_locks;	/* Kernel must leave these values alone */
//	u_long	rmx_mtu;	/* MTU for this path */
//	u_long	rmx_hopcount;	/* max hops expected */
//	int32_t	rmx_expire;	/* lifetime for route, e.g. redirect */
//	u_long	rmx_recvpipe;	/* inbound delay-bandwidth product */
//	u_long	rmx_sendpipe;	/* outbound delay-bandwidth product */
//	u_long	rmx_ssthresh;	/* outbound gateway buffer limit */
//	u_long	rmx_rtt;	/* estimated round trip time */
//	u_long	rmx_rttvar;	/* estimated rtt variance */
//	u_long	rmx_pksent;	/* packets sent using this route */
//	u_long	rmx_filler[4];	/* will be used for T/TCP later */
//};

/*
 * rmx_rtt and rmx_rttvar are stored as microseconds;
 * RTTTOPRHZ(rtt) converts to a value suitable for use
 * by a protocol slowtimo counter.
 */
#define	RTM_RTTUNIT	1000000	/* units for rtt, rttvar, as units per sec */
#define	RTTTOPRHZ(r)	((r) / (RTM_RTTUNIT / PR_SLOWHZ))

/*
 * XXX kernel function pointer `rt_output' is visible to applications.
 */

/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
//#ifdef PRIVATE
#ifndef RNF_NORMAL
//#include <net/radix.h>
struct radix_node;
#endif
struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
#define	rt_key(r)	((struct sockaddr *)((r)->rt_nodes->rn_key))
#define	rt_mask(r)	((struct sockaddr *)((r)->rt_nodes->rn_mask))
	struct	sockaddr *rt_gateway;	/* value */
	int32_t	rt_refcnt;		/* # held references */
	u_long	rt_flags;		/* up/down?, host/net */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
        u_long  rt_dlt;			/* DLIL dl_tag */
	struct	ifaddr *rt_ifa;		/* the answer: interface to use */
	struct	sockaddr *rt_genmask;	/* for generation of cloned routes */
	caddr_t	rt_llinfo;		/* pointer to link level info cache */
	struct	rt_metrics rt_rmx;	/* metrics used by rx'ing protocols */
	struct	rtentry *rt_gwroute;	/* implied entry for gatewayed routes */
	int	(*rt_output)(struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *);
					/* output routine for this (rt,if) */
	struct	rtentry *rt_parent; 	/* cloning parent of this route */
	u_long	generation_id;		/* route generation id */
};
//#endif /* PRIVATE */

#ifdef __APPLE_API_OBSOLETE
/*
 * Following structure necessary for 4.3 compatibility;
 * We should eventually move it to a compat file.
 */
//struct ortentry {
//	u_long	rt_hash;		/* to speed lookups */
//	struct	sockaddr rt_dst;	/* key */
//	struct	sockaddr rt_gateway;	/* value */
//	short	rt_flags;		/* up/down?, host/net */
//	short	rt_refcnt;		/* # held references */
//	u_long	rt_use;			/* raw # packets forwarded */
//	struct	ifnet *rt_ifp;		/* the answer: interface to use */
//};
#endif /* __APPLE_API_OBSOLETE */

//#ifdef PRIVATE
#define rt_use rt_rmx.rmx_pksent
//#endif /* PRIVATE */

#define	RTF_UP		0x1		/* route usable */
#define	RTF_GATEWAY	0x2		/* destination is a gateway */
#define	RTF_HOST	0x4		/* host entry (net otherwise) */
#define	RTF_REJECT	0x8		/* host or net unreachable */
#define	RTF_DYNAMIC	0x10		/* created dynamically (by redirect) */
#define	RTF_MODIFIED	0x20		/* modified dynamically (by redirect) */
#define RTF_DONE	0x40		/* message confirmed */
#define RTF_DELCLONE	0x80		/* delete cloned route */
#define RTF_CLONING	0x100		/* generate new routes on use */
#define RTF_XRESOLVE	0x200		/* external daemon resolves name */
#define RTF_LLINFO	0x400		/* generated by link layer (e.g. ARP) */
#define RTF_STATIC	0x800		/* manually added */
#define RTF_BLACKHOLE	0x1000		/* just discard pkts (during updates) */
#define RTF_PROTO2	0x4000		/* protocol specific routing flag */
#define RTF_PROTO1	0x8000		/* protocol specific routing flag */

#define RTF_PRCLONING	0x10000		/* protocol requires cloning */
#define RTF_WASCLONED	0x20000		/* route generated through cloning */
#define RTF_PROTO3	0x40000		/* protocol specific routing flag */
					/* 0x80000 unused */
#define RTF_PINNED	0x100000	/* future use */
#define	RTF_LOCAL	0x200000 	/* route represents a local address */
#define	RTF_BROADCAST	0x400000	/* route represents a bcast address */
#define	RTF_MULTICAST	0x800000	/* route represents a mcast address */
					/* 0x1000000 and up unassigned */

/*
 * Routing statistics.
 */
//struct	rtstat {
//	short	rts_badredirect;	/* bogus redirect calls */
//	short	rts_dynamic;		/* routes created by redirects */
//	short	rts_newgateway;		/* routes modified by redirects */
//	short	rts_unreach;		/* lookups which failed */
//	short	rts_wildcard;		/* lookups satisfied by a wildcard */
//};

/*
 * Structures for routing messages.
 */
//struct rt_msghdr {
//	u_short	rtm_msglen;	/* to skip over non-understood messages */
//	u_char	rtm_version;	/* future binary compatibility */
//	u_char	rtm_type;	/* message type */
//	u_short	rtm_index;	/* index for associated ifp */
//	int	rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
//	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
//	pid_t   rtm_pid;	/* identify sender */
//	int     rtm_seq;	/* for sender to identify action */
//	int     rtm_errno;	/* why failed */
//	int	rtm_use;	/* from rtentry */
//	u_long	rtm_inits;	/* which metrics we are initializing */
//	struct	rt_metrics rtm_rmx; /* metrics themselves */
//};

//struct rt_msghdr2 {
 //       u_short rtm_msglen;     /* to skip over non-understood messages */
//        u_char  rtm_version;    /* future binary compatibility */
//        u_char  rtm_type;       /* message type */
  //      u_short rtm_index;      /* index for associated ifp */
//        int     rtm_flags;      /* flags, incl. kern & message, e.g. DONE */
//        int     rtm_addrs;      /* bitmask identifying sockaddrs in msg */
//	int32_t rtm_refcnt;         /* reference count */
//	int     rtm_parentflags;      /* flags of the parent route */
  //      int     rtm_reserved;      /* reserved field set to 0 */
    //    int     rtm_use;        /* from rtentry */
//        u_long  rtm_inits;      /* which metrics we are initializing */
  //      struct  rt_metrics rtm_rmx; /* metrics themselves */
//};


#define RTM_VERSION	5	/* Up the ante and ignore older versions */

/*
 * Message types.
 */
#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#define RTM_LOSING	0x5	/* Kernel Suspects Partitioning */
#define RTM_REDIRECT	0x6	/* Told to use different route */
#define RTM_MISS	0x7	/* Lookup failed on this address */
#define RTM_LOCK	0x8	/* fix specified metrics */
#define RTM_OLDADD	0x9	/* caused by SIOCADDRT */
#define RTM_OLDDEL	0xa	/* caused by SIOCDELRT */
#define RTM_RESOLVE	0xb	/* req to resolve dst to LL addr */
#define RTM_NEWADDR	0xc	/* address being added to iface */
#define RTM_DELADDR	0xd	/* address being removed from iface */
#define RTM_IFINFO	0xe	/* iface going up/down etc. */
#define	RTM_NEWMADDR	0xf	/* mcast group membership being added to if */
#define	RTM_DELMADDR	0x10	/* mcast group membership being deleted */
#ifdef PRIVATE
#define RTM_GET_SILENT	0x11
#endif PRIVATE
#define RTM_IFINFO2	0x12	/* */
#define RTM_NEWMADDR2	0x13	/* */
#define RTM_GET2	0x14	/* */

/*
 * Bitmask values for rtm_inits and rmx_locks.
 */
#define RTV_MTU		0x1	/* init or lock _mtu */
#define RTV_HOPCOUNT	0x2	/* init or lock _hopcount */
#define RTV_EXPIRE	0x4	/* init or lock _expire */
#define RTV_RPIPE	0x8	/* init or lock _recvpipe */
#define RTV_SPIPE	0x10	/* init or lock _sendpipe */
#define RTV_SSTHRESH	0x20	/* init or lock _ssthresh */
#define RTV_RTT		0x40	/* init or lock _rtt */
#define RTV_RTTVAR	0x80	/* init or lock _rttvar */

/*
 * Bitmask values for rtm_addrs.
 */
#define RTA_DST		0x1	/* destination sockaddr present */
#define RTA_GATEWAY	0x2	/* gateway sockaddr present */
#define RTA_NETMASK	0x4	/* netmask sockaddr present */
#define RTA_GENMASK	0x8	/* cloning mask sockaddr present */
#define RTA_IFP		0x10	/* interface name sockaddr present */
#define RTA_IFA		0x20	/* interface addr sockaddr present */
#define RTA_AUTHOR	0x40	/* sockaddr for author of redirect */
#define RTA_BRD		0x80	/* for NEWADDR, broadcast or p-p dest addr */

/*
 * Index offsets for sockaddr array for alternate internal encoding.
 */
#define RTAX_DST	0	/* destination sockaddr present */
#define RTAX_GATEWAY	1	/* gateway sockaddr present */
#define RTAX_NETMASK	2	/* netmask sockaddr present */
#define RTAX_GENMASK	3	/* cloning mask sockaddr present */
#define RTAX_IFP	4	/* interface name sockaddr present */
#define RTAX_IFA	5	/* interface addr sockaddr present */
#define RTAX_AUTHOR	6	/* sockaddr for author of redirect */
#define RTAX_BRD	7	/* for NEWADDR, broadcast or p-p dest addr */
#define RTAX_MAX	8	/* size of array to allocate */

//struct rt_addrinfo {
//	int	rti_addrs;
//	struct	sockaddr *rti_info[RTAX_MAX];
//};

//struct route_cb {
//	int	ip_count;
//	int	ip6_count;
//	int	ipx_count;
//	int	ns_count;
//	int	iso_count;
//	int	any_count;
//};

#ifdef KERNEL_PRIVATE
#define RTFREE(rt)	rtfree(rt)
extern struct route_cb route_cb;
extern struct radix_node_head *rt_tables[AF_MAX+1];

struct ifmultiaddr;
struct proc;

void	 route_init(void);
void	 rt_ifmsg(struct ifnet *);
void	 rt_missmsg(int, struct rt_addrinfo *, int, int);
void	 rt_newaddrmsg(int, struct ifaddr *, int, struct rtentry *);
void	 rt_newmaddrmsg(int, struct ifmultiaddr *);
int	 rt_setgate(struct rtentry *, struct sockaddr *, struct sockaddr *);
void	 rtalloc(struct route *);
void	 rtalloc_ign(struct route *, u_long);
struct rtentry *
	 rtalloc1(struct sockaddr *, int, u_long);
struct rtentry *
	 rtalloc1_locked(const struct sockaddr *, int, u_long);
void	rtfree(struct rtentry *);
void	rtfree_locked(struct rtentry *);
void	rtref(struct rtentry *);
/*
 * rtunref will decrement the refcount, rtfree will decrement and free if
 * the refcount has reached zero and the route is not up.
 * Unless you have good reason to do otherwise, use rtfree.
 */
void	rtunref(struct rtentry *);
void	rtsetifa(struct rtentry *, struct ifaddr *);
int	 rtinit(struct ifaddr *, int, int);
int	 rtinit_locked(struct ifaddr *, int, int);
int	 rtioctl(int, caddr_t, struct proc *);
void	 rtredirect(struct sockaddr *, struct sockaddr *,
	    struct sockaddr *, int, struct sockaddr *, struct rtentry **);
int	 rtrequest(int, struct sockaddr *,
	    struct sockaddr *, struct sockaddr *, int, struct rtentry **);
int	 rtrequest_locked(int, struct sockaddr *,
	    struct sockaddr *, struct sockaddr *, int, struct rtentry **);
#endif KERNEL_PRIVATE

//#endif

////////////////////////////////////////////////////////////////////
// From domain.h
///////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
															   * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1998, 1999 Apple Computer, Inc. All Rights Reserved */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)domain.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/sys/sys/domain.h,v 1.14 1999/12/29 04:24:40 peter Exp $
 */

//#ifndef _SYS_DOMAIN_H_
//#define _SYS_DOMAIN_H_

//#ifdef PRIVATE

#include <sys/appleapiopts.h>
#ifdef KERNEL
#include <kern/locks.h>
#endif /* KERNEL */
/*
 * Structure per communications domain.
 */

#include <sys/cdefs.h>

/*
 * Forward structure declarations for function prototypes [sic].
 */
struct	mbuf;
#define DOM_REENTRANT	0x01

#if __DARWIN_ALIGN_POWER
#pragma options align=power
#endif

struct	domain {
	int	dom_family;		/* AF_xxx */
	char	*dom_name;
	void	(*dom_init)(void);	/* initialize domain data structures */
	int	(*dom_externalize)(struct mbuf *);
	/* externalize access rights */
	void	(*dom_dispose)(struct mbuf *);
	/* dispose of internalized rights */
	struct	protosw *dom_protosw;	/* Chain of protosw's for AF */
	struct	domain *dom_next;
	int	(*dom_rtattach)(void **, int);
	/* initialize routing table */
	int	dom_rtoffset;		/* an arg to rtattach, in bits */
	int	dom_maxrtkey;		/* for routing layer */
	int	dom_protohdrlen;	/* Let the protocol tell us */
	int	dom_refs;		/* # socreates outstanding */
#ifdef _KERN_LOCKS_H_
	lck_mtx_t 	*dom_mtx;		/* domain global mutex */
#else
	void 	*dom_mtx;		/* domain global mutex */
#endif
	u_long		dom_flags;
	u_long		reserved[2];
};

#if __DARWIN_ALIGN_POWER
#pragma options align=reset
#endif

#ifdef KERNEL
extern struct	domain *domains;
extern struct	domain localdomain;

__BEGIN_DECLS
extern void	net_add_domain(struct domain *dp);
extern int	net_del_domain(struct domain *);
__END_DECLS

#define DOMAIN_SET(domain_set) 

#endif /* KERNEL */
//#endif /* PRIVATE */
//#endif	/* _SYS_DOMAIN_H_ */

/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
															   * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)select.h	8.2 (Berkeley) 1/4/94
 */

//#ifndef _SYS_SELECT_H_
//#define	_SYS_SELECT_H_

#include <sys/appleapiopts.h>
#include <sys/cdefs.h>
#include <sys/_types.h>

/*
 * The time_t and suseconds_t types shall be defined as described in
 * <sys/types.h>
 * The sigset_t type shall be defined as described in <signal.h>
 * The timespec structure shall be defined as described in <time.h>
 */
#ifndef	_TIME_T
#define	_TIME_T
typedef	__darwin_time_t		time_t;
#endif

#ifndef _SUSECONDS_T
#define _SUSECONDS_T
typedef __darwin_suseconds_t	suseconds_t;
#endif

#ifndef _SIGSET_T
#define _SIGSET_T
typedef __darwin_sigset_t	sigset_t;
#endif

#ifndef _TIMESPEC
#define _TIMESPEC
struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
};
#endif

/*
 * [XSI] The <sys/select.h> header shall define the fd_set type as a structure.
 * [XSI] FD_CLR, FD_ISSET, FD_SET, FD_ZERO may be declared as a function, or
 *	 defined as a macro, or both
 * [XSI] FD_SETSIZE shall be defined as a macro
 *
 * Note:	We use _FD_SET to protect all select related
 *		types and macros
 */
#ifndef _FD_SET
#define	_FD_SET

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).  The
 * extra protection here is to permit application redefinition above
 * the default size.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024
#endif

#define	__DARWIN_NBBY	8				/* bits in a byte */
#define __DARWIN_NFDBITS	(sizeof(__int32_t) * __DARWIN_NBBY) /* bits per mask */
#define	__DARWIN_howmany(x, y) (((x) + ((y) - 1)) / (y))	/* # y's == x bits? */

typedef	struct fd_set {
	__int32_t	fds_bits[__DARWIN_howmany(FD_SETSIZE, __DARWIN_NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/__DARWIN_NFDBITS] |= (1<<((n) % __DARWIN_NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/__DARWIN_NFDBITS] &= ~(1<<((n) % __DARWIN_NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/__DARWIN_NFDBITS] & (1<<((n) % __DARWIN_NFDBITS)))
#if __GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >= 3
/*
 * Use the built-in bzero function instead of the library version so that
 * we do not pollute the namespace or introduce prototype warnings.
 */
#define	FD_ZERO(p)	__builtin_bzero(p, sizeof(*(p)))
#else
#define	FD_ZERO(p)	bzero(p, sizeof(*(p)))
#endif
#ifndef _POSIX_C_SOURCE
#define	FD_COPY(f, t)	bcopy(f, t, sizeof(*(f)))
#endif	/* !_POSIX_C_SOURCE */

#endif	/* !_FD_SET */

#ifdef KERNEL
#ifdef KERNEL_PRIVATE

//#include <kern/wait_queue.h>

struct hslock {
	int		lock_data;
};
typedef struct hslock hw_lock_data_t, *hw_lock_t;

/*
 *	A generic doubly-linked list (queue).
 */

struct queue_entry {
	struct queue_entry	*next;		/* next element */
	struct queue_entry	*prev;		/* previous element */
};

typedef struct queue_entry	*queue_t;
typedef	struct queue_entry	queue_head_t;
typedef	struct queue_entry	queue_chain_t;
typedef	struct queue_entry	*queue_entry_t;

/*
 *	wait_queue_t
 *	This is the definition of the common event wait queue
 *	that the scheduler APIs understand.  It is used
 *	internally by the gerneralized event waiting mechanism
 *	(assert_wait), and also for items that maintain their
 *	own wait queues (such as ports and semaphores).
 *
 *	It is not published to other kernel components.  They
 *	can create wait queues by calling wait_queue_alloc.
 *
 *	NOTE:  Hardware locks are used to protect event wait
 *	queues since interrupt code is free to post events to
 *	them.
 */
typedef struct wait_queue {
    unsigned int                    /* flags */
    /* boolean_t */	wq_type:16,		/* only public field */
					wq_fifo:1,		/* fifo wakeup policy? */		
					wq_isprepost:1,	/* is waitq preposted? set only */
					:0;				/* force to long boundary */
    hw_lock_data_t	wq_interlock;	/* interlock */
    queue_head_t	wq_queue;		/* queue of elements */
} WaitQueue;

struct klist;
SLIST_HEAD(klist, knote);

#endif
#include <sys/kernel_types.h>

#include <sys/event.h>

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
#ifdef KERNEL_PRIVATE
struct selinfo {
	struct  wait_queue si_wait_queue;	/* wait_queue for wait/wakeup */
	struct klist si_note;		/* JMM - temporary separation */
	u_int	si_flags;		/* see below */
};

#define	SI_COLL		0x0001		/* collision occurred */
#define	SI_RECORDED	0x0004		/* select has been recorded */ 
#define	SI_INITED	0x0008		/* selinfo has been inited */ 
#define	SI_CLEAR	0x0010		/* selinfo has been cleared */ 

#else
struct selinfo;
#endif

__BEGIN_DECLS

void	selrecord(proc_t selector, struct selinfo *, void *);
void	selwakeup(struct selinfo *);
void	selthreadclear(struct selinfo *);

__END_DECLS

#endif /* KERNEL */


#ifndef KERNEL
#ifndef _POSIX_C_SOURCE
#include <sys/types.h>
#ifndef  __MWERKS__
#include <signal.h>
#endif /* __MWERKS__ */
#include <sys/time.h>
#endif	/* !_POSIX_C_SOURCE */

__BEGIN_DECLS
#ifndef  __MWERKS__
int	 pselect(int, fd_set * __restrict, fd_set * __restrict,
			 fd_set * __restrict, const struct timespec * __restrict,
			 const sigset_t * __restrict);
#endif /* __MWERKS__ */
int	 select(int, fd_set * __restrict, fd_set * __restrict,
			fd_set * __restrict, struct timeval * __restrict);
__END_DECLS
#endif /* ! KERNEL */

//#endif /* !_SYS_SELECT_H_ */

/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1998, 1999 Apple Computer, Inc. All Rights Reserved */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)socketvar.h	8.3 (Berkeley) 2/19/95
 * $FreeBSD: src/sys/sys/socketvar.h,v 1.46.2.6 2001/08/31 13:45:49 jlemon Exp $
 */

//#ifndef _SYS_SOCKETVAR_H_
//#define _SYS_SOCKETVAR_H_

#include <sys/appleapiopts.h>
#include <sys/queue.h>			/* for TAILQ macros */
#include <sys/select.h>			/* for struct selinfo */
#include <net/kext_net.h>
#include <sys/ev.h>
#include <sys/cdefs.h>

/*
 * Hacks to get around compiler complaints
 */
struct mbuf;
struct socket_filter_entry;
struct protosw;
struct sockif;
struct sockutil;

#ifdef KERNEL_PRIVATE
/* strings for sleep message: */
extern	char netio[], netcon[], netcls[];
#define SOCKET_CACHE_ON	
#define SO_CACHE_FLUSH_INTERVAL 1	/* Seconds */
#define SO_CACHE_TIME_LIMIT	(120/SO_CACHE_FLUSH_INTERVAL) /* Seconds */
#define SO_CACHE_MAX_FREE_BATCH	50
#define MAX_CACHED_SOCKETS	60000
#define TEMPDEBUG		0

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
#endif /* KERNEL_PRIVATE */

//typedef	u_quad_t so_gen_t;

#ifdef KERNEL_PRIVATE
#ifndef __APPLE__
/* We don't support BSD style socket filters */
struct accept_filter;
#endif

struct socket {
	int		so_zone;			/* zone we were allocated from */
	short	so_type;			/* generic type, see socket.h */
	short	so_options;		/* from socket call, see socket.h */
	short	so_linger;		/* time to linger while closing */
	short	so_state;			/* internal state flags SS_*, below */
	caddr_t	so_pcb;			/* protocol control block */
	struct	protosw *so_proto;	/* protocol handle */
/*
 * Variables for connection queuing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_incomp queues partially completed connections,
 * while so_comp is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_incomp or so_comp.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
	struct	socket *so_head;	/* back pointer to accept socket */
	TAILQ_HEAD(, socket) so_incomp;	/* queue of partial unaccepted connections */
	TAILQ_HEAD(, socket) so_comp;	/* queue of complete unaccepted connections */
	TAILQ_ENTRY(socket) so_list;	/* list of unaccepted connections */
	short	so_qlen;		/* number of unaccepted connections */
	short	so_incqlen;		/* number of unaccepted incomplete
					   connections */
	short	so_qlimit;		/* max number queued connections */
	short	so_timeo;		/* connection timeout */
	u_short	so_error;		/* error affecting connection */
	pid_t	so_pgid;		/* pgid for signals */
	u_long	so_oobmark;		/* chars to oob mark */
#ifndef __APPLE__
	/* We don't support AIO ops */
	TAILQ_HEAD(, aiocblist) so_aiojobq; /* AIO ops waiting on socket */
#endif
/*
 * Variables for socket buffering.
 */
	struct	sockbuf {
		u_long	sb_cc;		/* actual chars in buffer */
		u_long	sb_hiwat;	/* max actual char count */
		u_long	sb_mbcnt;	/* chars of mbufs used */
		u_long	sb_mbmax;	/* max chars of mbufs to use */
		long	sb_lowat;	/* low water mark */
		struct	mbuf *sb_mb;	/* the mbuf chain */
#if __APPLE__
        struct  socket *sb_so;  /* socket back ptr for kexts */
#endif
		struct	selinfo sb_sel;	/* process selecting read/write */
		short	sb_flags;	/* flags, see below */
		struct timeval	sb_timeo;	/* timeout for read/write */
		void    *reserved1;     /* for future use if needed */
		void    *reserved2;
	} so_rcv, so_snd;
		
#define	SB_MAX		(256*1024)	/* default for max chars in sockbuf */
#define	SB_LOCK		0x01		/* lock on data queue */
#define	SB_WANT		0x02		/* someone is waiting to lock */
#define	SB_WAIT		0x04		/* someone is waiting for data/space */
#define	SB_SEL		0x08		/* someone is selecting */
#define	SB_ASYNC		0x10		/* ASYNC I/O, need signals */
#define	SB_UPCALL		0x20		/* someone wants an upcall */
#define	SB_NOINTR		0x40		/* operations not interruptible */
#define SB_KNOTE		0x100	/* kernel note attached */
#ifndef __APPLE__
#define SB_AIO		0x80		/* AIO operations queued */
#else
#define	SB_NOTIFY	(SB_WAIT|SB_SEL|SB_ASYNC)
#define SB_RECV		0x8000		/* this is rcv sb */

	caddr_t	so_tpcb;		/* Wisc. protocol control block - XXX unused? */
#endif

	void	(*so_upcall)(struct socket *so, caddr_t arg, int waitf);
	caddr_t	so_upcallarg;		/* Arg for above */
	uid_t	so_uid;			/* who opened the socket */
	/* NB: generation count must not be first; easiest to make it last. */
	so_gen_t so_gencnt;		/* generation count */
#ifndef __APPLE__
	void	*so_emuldata;		/* private data for emulators */
	struct	so_accf { 
		struct	accept_filter *so_accept_filter;
		void	*so_accept_filter_arg;	/* saved filter args */
		char	*so_accept_filter_str;	/* saved user args */
	} *so_accf;
#else
	TAILQ_HEAD(,eventqelt) so_evlist;
	int	cached_in_sock_layer;	/* Is socket bundled with pcb/pcb.inp_ppcb? */
	struct	socket	*cache_next;
	struct	socket	*cache_prev;
	u_long		cache_timestamp;
	caddr_t		so_saved_pcb;	/* Saved pcb when cacheing */
	struct	mbuf *so_temp;		/* Holding area for outbound frags */
	/* Plug-in support - make the socket interface overridable */
	struct	mbuf *so_tail;
	struct socket_filter_entry *so_filt;		/* NKE hook */
	u_long	so_flags;		/* Flags */
#define SOF_NOSIGPIPE		0x00000001
#define SOF_NOADDRAVAIL		0x00000002	/* returns EADDRNOTAVAIL if src address is gone */
#define SOF_PCBCLEARING		0x00000004	/* pru_disconnect done, no need to call pru_detach */
	int	so_usecount;		/* refcounting of socket use */;
	int	so_retaincnt;
	u_int32_t	so_filteruse; /* usecount for the socket filters */
	void	*reserved3;		/* Temporarily in use/debug: last socket lock LR */
	void	*reserved4;		/* Temporarily in use/debug: last socket unlock LR */

#endif
};
#endif /* KERNEL_PRIVATE */

/*
 * Socket state bits.
 */
#define	SS_NOFDREF		0x001	/* no file table ref any more */
#define	SS_ISCONNECTED		0x002	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x004	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x008	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x010	/* can't send more data to peer */
#define	SS_CANTRCVMORE		0x020	/* can't receive more data from peer */
#define	SS_RCVATMARK		0x040	/* at mark on input */

#define	SS_PRIV			0x080	/* privileged for broadcast, raw... */
#define	SS_NBIO			0x100	/* non-blocking ops */
#define	SS_ASYNC		0x200	/* async i/o notify */
#define	SS_ISCONFIRMING		0x400	/* deciding to accept connection req */
#define	SS_INCOMP		0x800	/* Unaccepted, incomplete connection */
#define	SS_COMP			0x1000	/* unaccepted, complete connection */
#define	SS_ISDISCONNECTED	0x2000	/* socket disconnected from peer */
#define	SS_DRAINING		0x4000	/* close waiting for blocked system calls to drain */

/*
 * Externalized form of struct socket used by the sysctl(3) interface.
 */
#if 0
struct	xsocket {
	size_t	xso_len;	/* length of this structure */
	struct	socket *xso_so;	/* makes a convenient handle sometimes */
	short	so_type;
	short	so_options;
	short	so_linger;
	short	so_state;
	caddr_t	so_pcb;		/* another convenient handle */
	int	xso_protocol;
	int	xso_family;
	short	so_qlen;
	short	so_incqlen;
	short	so_qlimit;
	short	so_timeo;
	u_short	so_error;
	pid_t	so_pgid;
	u_long	so_oobmark;
	struct	xsockbuf {
		u_long	sb_cc;
		u_long	sb_hiwat;
		u_long	sb_mbcnt;
		u_long	sb_mbmax;
		long	sb_lowat;
		short	sb_flags;
		short	sb_timeo;
	} so_rcv, so_snd;
	uid_t	so_uid;		/* XXX */
};
#endif

#ifdef KERNEL_PRIVATE
/*
 * Macros for sockets and socket buffering.
 */

#define sbtoso(sb) (sb->sb_so)

/*
 * Functions for sockets and socket buffering.
 * These are macros on FreeBSD. On Darwin the
 * implementation is in bsd/kern/uipc_socket2.c
 */

__BEGIN_DECLS
int		sb_notify(struct sockbuf *sb);
long		sbspace(struct sockbuf *sb);
int		sosendallatonce(struct socket *so);
int		soreadable(struct socket *so);
int		sowriteable(struct socket *so);
void		sballoc(struct sockbuf *sb, struct mbuf *m);
void		sbfree(struct sockbuf *sb, struct mbuf *m);
int		sblock(struct sockbuf *sb, int wf);
void		sbunlock(struct sockbuf *sb, int locked);
void		sorwakeup(struct socket * so);
void		sowwakeup(struct socket * so);
__END_DECLS

/*
 * Socket extension mechanism: control block hooks:
 * This is the "head" of any control block for an extenstion
 * Note: we separate intercept function dispatch vectors from
 *  the NFDescriptor to permit selective replacement during
 *  operation, e.g., to disable some functions.
 */
struct kextcb
{	struct kextcb *e_next;		/* Next kext control block */
	void *e_fcb;			/* Real filter control block */
	struct NFDescriptor *e_nfd;	/* NKE Descriptor */
	/* Plug-in support - intercept functions */
	struct sockif *e_soif;		/* Socket functions */
	struct sockutil *e_sout;	/* Sockbuf utility functions */
};
#define EXT_NULL	0x0		/* STATE: Not in use */
#define sotokextcb(so) (so ? so->so_ext : 0)

#ifdef KERNEL

#define SO_FILT_HINT_LOCKED 0x1

/*
 * Argument structure for sosetopt et seq.  This is in the KERNEL
 * section because it will never be visible to user code.
 */
enum sopt_dir { SOPT_GET, SOPT_SET };
struct sockopt {
	enum	sopt_dir sopt_dir; /* is this a get or a set? */
	int	sopt_level;	/* second arg of [gs]etsockopt */
	int	sopt_name;	/* third arg of [gs]etsockopt */
	user_addr_t sopt_val;	/* fourth arg of [gs]etsockopt */
	size_t	sopt_valsize;	/* (almost) fifth arg of [gs]etsockopt */
	struct	proc *sopt_p;	/* calling process or null if kernel */
};

#if SENDFILE
struct sf_buf {
	SLIST_ENTRY(sf_buf) free_list;	/* list of free buffer slots */
	int		refcnt;		/* reference count */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
};
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PCB);
MALLOC_DECLARE(M_SONAME);
#endif

extern int	maxsockets;
extern u_long	sb_max;
extern int socket_zone;
extern so_gen_t so_gencnt;

struct file;
struct filedesc;
struct mbuf;
struct sockaddr;
struct stat;
struct ucred;
struct uio;
struct knote;

/*
 * From uipc_socket and friends
 */
__BEGIN_DECLS
struct	sockaddr *dup_sockaddr(struct sockaddr *sa, int canwait);
int	getsock(struct filedesc *fdp, int fd, struct file **fpp);
int	sockargs(struct mbuf **mp, user_addr_t data, int buflen, int type);
int	getsockaddr(struct sockaddr **namp, user_addr_t uaddr, size_t len);
int	sbappend(struct sockbuf *sb, struct mbuf *m);
int	sbappendaddr(struct sockbuf *sb, struct sockaddr *asa,
	    struct mbuf *m0, struct mbuf *control, int *error_out);
int	sbappendcontrol(struct sockbuf *sb, struct mbuf *m0,
	    struct mbuf *control, int *error_out);
int	sbappendrecord(struct sockbuf *sb, struct mbuf *m0);
void	sbcheck(struct sockbuf *sb);
int	sbcompress(struct sockbuf *sb, struct mbuf *m, struct mbuf *n);
struct mbuf *
	sbcreatecontrol(caddr_t p, int size, int type, int level);
void	sbdrop(struct sockbuf *sb, int len);
void	sbdroprecord(struct sockbuf *sb);
void	sbflush(struct sockbuf *sb);
int		sbinsertoob(struct sockbuf *sb, struct mbuf *m0);
void	sbrelease(struct sockbuf *sb);
int	sbreserve(struct sockbuf *sb, u_long cc);
void	sbtoxsockbuf(struct sockbuf *sb, struct xsockbuf *xsb);
int	sbwait(struct sockbuf *sb);
int	sb_lock(struct sockbuf *sb);
int	soabort(struct socket *so);
int	soaccept(struct socket *so, struct sockaddr **nam);
int	soacceptlock (struct socket *so, struct sockaddr **nam, int dolock);
struct	socket *soalloc(int waitok, int dom, int type);
int	sobind(struct socket *so, struct sockaddr *nam);
void	socantrcvmore(struct socket *so);
void	socantsendmore(struct socket *so);
int	soclose(struct socket *so);
int	soconnect(struct socket *so, struct sockaddr *nam);
int	soconnectlock (struct socket *so, struct sockaddr *nam, int dolock);
int	soconnect2(struct socket *so1, struct socket *so2);
int	socreate(int dom, struct socket **aso, int type, int proto);
void	sodealloc(struct socket *so);
int	sodisconnect(struct socket *so);
void	sofree(struct socket *so);
int	sogetopt(struct socket *so, struct sockopt *sopt);
void	sohasoutofband(struct socket *so);
void	soisconnected(struct socket *so);
void	soisconnecting(struct socket *so);
void	soisdisconnected(struct socket *so);
void	soisdisconnecting(struct socket *so);
int	solisten(struct socket *so, int backlog);
struct socket *
	sodropablereq(struct socket *head);
struct socket *
	sonewconn(struct socket *head, int connstatus, const struct sockaddr* from);
int	sooptcopyin(struct sockopt *sopt, void *data, size_t len, size_t minlen);
int	sooptcopyout(struct sockopt *sopt, void *data, size_t len);
int 	socket_lock(struct socket *so, int refcount);
int 	socket_unlock(struct socket *so, int refcount);

/*
 * XXX; prepare mbuf for (__FreeBSD__ < 3) routines.
 * Used primarily in IPSec and IPv6 code.
 */
int	soopt_getm(struct sockopt *sopt, struct mbuf **mp);
int	soopt_mcopyin(struct sockopt *sopt, struct mbuf *m);
int	soopt_mcopyout(struct sockopt *sopt, struct mbuf *m);

int	sopoll(struct socket *so, int events, struct ucred *cred, void *wql);
int	soreceive(struct socket *so, struct sockaddr **paddr,
		       struct uio *uio, struct mbuf **mp0,
		       struct mbuf **controlp, int *flagsp);
int	soreserve(struct socket *so, u_long sndcc, u_long rcvcc);
void	sorflush(struct socket *so);
int	sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
		    struct mbuf *top, struct mbuf *control, int flags);

int	sosetopt(struct socket *so, struct sockopt *sopt);
int	soshutdown(struct socket *so, int how);
void	sotoxsocket(struct socket *so, struct xsocket *xso);
void	sowakeup(struct socket *so, struct sockbuf *sb);
int	soioctl(struct socket *so, u_long cmd, caddr_t data, struct proc *p);

#ifndef __APPLE__
/* accept filter functions */
int	accept_filt_add(struct accept_filter *filt);
int	accept_filt_del(char *name);
struct accept_filter *	accept_filt_get(char *name);
#ifdef ACCEPT_FILTER_MOD
int accept_filt_generic_mod_event(module_t mod, int event, void *data);
SYSCTL_DECL(_net_inet_accf);
#endif /* ACCEPT_FILTER_MOD */
#endif /* !defined(__APPLE__) */

__END_DECLS

#endif /* KERNEL */
#endif /* KERNEL_PRIVATE */

//#endif /* !_SYS_SOCKETVAR_H_ */

/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1998, 1999 Apple Computer, Inc. All Rights Reserved */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/* 
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * Copyright (c) 1994 NeXT Computer, Inc. All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	@(#)mbuf.h	8.3 (Berkeley) 1/21/94
 **********************************************************************
 * HISTORY
 * 20-May-95  Mac Gillon (mgillon) at NeXT
 *	New version based on 4.4
 *	Purged old history
 */

//#ifndef	_SYS_MBUF_H_
//#define	_SYS_MBUF_H_

#include <sys/cdefs.h>
#include <sys/appleapiopts.h>

#ifdef KERNEL_PRIVATE

#include <sys/lock.h>
#include <sys/queue.h>

/*
 * Mbufs are of a single size, MSIZE (machine/param.h), which
 * includes overhead.  An mbuf may add a single "mbuf cluster" of size
 * MCLBYTES (also in machine/param.h), which has no additional overhead
 * and is used instead of the internal data area; this is done when
 * at least MINCLSIZE of data must be stored.
 */

#define	MLEN		(MSIZE - sizeof(struct m_hdr))	/* normal data len */
#define	MHLEN		(MLEN - sizeof(struct pkthdr))	/* data len w/pkthdr */

#define	MINCLSIZE	(MHLEN + MLEN)	/* smallest amount to put in cluster */
#define	M_MAXCOMPRESS	(MHLEN / 2)	/* max amount to copy for compression */

#define NMBPCL		(sizeof(union mcluster) / sizeof(struct mbuf))

/*
 * Macros for type conversion
 * mtod(m,t) -	convert mbuf pointer to data pointer of correct type
 * dtom(x) -	convert data pointer within mbuf to mbuf pointer (XXX)
 * mtocl(x) -	convert pointer within cluster to cluster index #
 * cltom(x) -	convert cluster # to ptr to beginning of cluster
 */
#define mtod(m,t)       ((t)m_mtod(m))
#define dtom(x)         m_dtom(x)
#define mtocl(x)        m_mtocl(x)
#define cltom(x)        m_cltom(x)

#define MCLREF(p)       m_mclref(p)
#define MCLUNREF(p)     m_mclunref(p)

/* header at beginning of each mbuf: */
struct m_hdr {
	struct	mbuf *mh_next;		/* next buffer in chain */
	struct	mbuf *mh_nextpkt;	/* next chain in queue/record */
	long	mh_len;			/* amount of data in this mbuf */
	caddr_t	mh_data;		/* location of data */
	short	mh_type;		/* type of data in this mbuf */
	short	mh_flags;		/* flags; see below */
};

/*
 * Packet tag structure (see below for details).
 */
struct m_tag {
	SLIST_ENTRY(m_tag)	m_tag_link;	/* List of packet tags */
	u_int16_t			m_tag_type;	/* Module specific type */
	u_int16_t			m_tag_len;	/* Length of data */
	u_int32_t			m_tag_id;	/* Module ID */
};

/* record/packet header in first mbuf of chain; valid if M_PKTHDR set */
struct	pkthdr {
	int	len;			/* total packet length */
	struct	ifnet *rcvif;		/* rcv interface */

	/* variables for ip and tcp reassembly */
	void	*header;		/* pointer to packet header */
        /* variables for hardware checksum */
#ifdef KERNEL_PRIVATE
    	/* Note: csum_flags is used for hardware checksum and VLAN */
#endif KERNEL_PRIVATE
        int     csum_flags;             /* flags regarding checksum */       
        int     csum_data;              /* data field used by csum routines */
	struct mbuf *aux;		/* extra data buffer; ipsec/others */
#ifdef KERNEL_PRIVATE
	u_short	vlan_tag;		/* VLAN tag, host byte order */
	u_short socket_id;		/* socket id */
#else KERNEL_PRIVATE
	u_int	reserved1;		/* for future use */
#endif KERNEL_PRIVATE
        SLIST_HEAD(packet_tags, m_tag) tags; /* list of packet tags */
};


/* description of external storage mapped into mbuf, valid if M_EXT set */
struct m_ext {
	caddr_t	ext_buf;		/* start of buffer */
	void	(*ext_free)(caddr_t , u_int, caddr_t);	/* free routine if not the usual */
	u_int	ext_size;		/* size of buffer, for ext_free */
	caddr_t	ext_arg;		/* additional ext_free argument */
	struct	ext_refsq {		/* references held */
		struct ext_refsq *forward, *backward;
	} ext_refs;
};

struct mbuf {
	struct	m_hdr m_hdr;
	union {
		struct {
			struct	pkthdr MH_pkthdr;	/* M_PKTHDR set */
			union {
				struct	m_ext MH_ext;	/* M_EXT set */
				char	MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char	M_databuf[MLEN];		/* !M_PKTHDR, !M_EXT */
	} M_dat;
};

#define	m_next		m_hdr.mh_next
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

/* mbuf flags */
#define	M_EXT		0x0001	/* has associated external storage */
#define	M_PKTHDR	0x0002	/* start of record */
#define	M_EOR		0x0004	/* end of record */
#define	M_PROTO1	0x0008	/* protocol-specific */
#define	M_PROTO2	0x0010	/* protocol-specific */
#define	M_PROTO3	0x0020	/* protocol-specific */
#define	M_PROTO4	0x0040	/* protocol-specific */
#define	M_PROTO5	0x0080	/* protocol-specific */

/* mbuf pkthdr flags, also in m_flags */
#define	M_BCAST		0x0100	/* send/received as link-level broadcast */
#define	M_MCAST		0x0200	/* send/received as link-level multicast */
#define	M_FRAG		0x0400	/* packet is a fragment of a larger packet */
#define	M_FIRSTFRAG	0x0800	/* packet is first fragment */
#define	M_LASTFRAG	0x1000	/* packet is last fragment */
#define	M_PROMISC	0x2000	/* packet is promiscuous (shouldn't go to stack) */

/* flags copied when copying m_pkthdr */
#define M_COPYFLAGS     (M_PKTHDR|M_EOR|M_PROTO1|M_PROTO2|M_PROTO3 | \
                            M_PROTO4|M_PROTO5|M_BCAST|M_MCAST|M_FRAG | \
                            M_FIRSTFRAG|M_LASTFRAG|M_PROMISC)

/* flags indicating hw checksum support and sw checksum requirements [freebsd4.1]*/
#define CSUM_IP                 0x0001          /* will csum IP */
#define CSUM_TCP                0x0002          /* will csum TCP */
#define CSUM_UDP                0x0004          /* will csum UDP */
#define CSUM_IP_FRAGS           0x0008          /* will csum IP fragments */
#define CSUM_FRAGMENT           0x0010          /* will do IP fragmentation */
        
#define CSUM_IP_CHECKED         0x0100          /* did csum IP */
#define CSUM_IP_VALID           0x0200          /*   ... the csum is valid */
#define CSUM_DATA_VALID         0x0400          /* csum_data field is valid */
#define CSUM_PSEUDO_HDR         0x0800          /* csum_data has pseudo hdr */
#define CSUM_TCP_SUM16          0x1000          /* simple TCP Sum16 computation */
 
#define CSUM_DELAY_DATA         (CSUM_TCP | CSUM_UDP)
#define CSUM_DELAY_IP           (CSUM_IP)       /* XXX add ipv6 here too? */
/*
 * Note: see also IF_HWASSIST_CSUM defined in <net/if_var.h>
 */
/* bottom 16 bits reserved for hardware checksum */
#define CSUM_CHECKSUM_MASK	0xffff

/* VLAN tag present */
#define CSUM_VLAN_TAG_VALID	0x10000		/* vlan_tag field is valid */
#endif KERNEL_PRIVATE


/* mbuf types */
#define	MT_FREE		0	/* should be on free list */
#define	MT_DATA		1	/* dynamic (data) allocation */
#define	MT_HEADER	2	/* packet header */
#define	MT_SOCKET	3	/* socket structure */
#define	MT_PCB		4	/* protocol control block */
#define	MT_RTABLE	5	/* routing tables */
#define	MT_HTABLE	6	/* IMP host tables */
#define	MT_ATABLE	7	/* address resolution tables */
#define	MT_SONAME	8	/* socket name */
#define	MT_SOOPTS	10	/* socket options */
#define	MT_FTABLE	11	/* fragment reassembly header */
#define	MT_RIGHTS	12	/* access rights */
#define	MT_IFADDR	13	/* interface address */
#define MT_CONTROL	14	/* extra-data protocol message */
#define MT_OOBDATA	15	/* expedited data  */
#define MT_TAG          16      /* volatile metadata associated to pkts */
#define MT_MAX		32	/* enough? */

#ifdef KERNEL_PRIVATE

/* flags to m_get/MGET */
/* Need to include malloc.h to get right options for malloc  */
#include	<sys/malloc.h>

#define	M_DONTWAIT	M_NOWAIT
#define	M_WAIT		M_WAITOK

/*
 * mbuf utility macros:
 *
 *	MBUFLOCK(code)
 * prevents a section of code from from being interrupted by network
 * drivers.
 */

#ifdef _KERN_LOCKS_H_
extern lck_mtx_t * mbuf_mlock;
#else
extern void * mbuf_mlock;
#endif

#define MBUF_LOCK()	lck_mtx_lock(mbuf_mlock);
#define MBUF_UNLOCK()	lck_mtx_unlock(mbuf_mlock);

/*
 * mbuf allocation/deallocation macros:
 *
 *	MGET(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain internal data.
 *
 *	MGETHDR(struct mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain a packet header
 * and internal data.
 */

#if 1
#define MCHECK(m) m_mcheck(m)
#else
#define MCHECK(m)
#endif

extern struct mbuf *mfree;				/* mbuf free list */

#define	MGET(m, how, type) ((m) = m_get((how), (type)))

#define	MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))

/*
 * Mbuf cluster macros.
 * MCLALLOC(caddr_t p, int how) allocates an mbuf cluster.
 * MCLGET adds such clusters to a normal mbuf;
 * the flag M_EXT is set upon success.
 * MCLFREE releases a reference to a cluster allocated by MCLALLOC,
 * freeing the cluster if the reference count has reached 0.
 *
 * Normal mbuf clusters are normally treated as character arrays
 * after allocation, but use the first word of the buffer as a free list
 * pointer while on the free list.
 */
union mcluster {
	union	mcluster *mcl_next;
	char	mcl_buf[MCLBYTES];
};

#define	MCLALLOC(p, how)	((p) = m_mclalloc(how))

#define	MCLFREE(p)	m_mclfree(p)

#define	MCLGET(m, how) 	((m) = m_mclget(m, how))

/*
 * Mbuf big cluster
 */

union mbigcluster {
	union mbigcluster	*mbc_next;
	char 			mbc_buf[NBPG];
};


#define MCLHASREFERENCE(m) m_mclhasreference(m)

/*
 * MFREE(struct mbuf *m, struct mbuf *n)
 * Free a single mbuf and associated external storage.
 * Place the successor, if any, in n.
 */

#define	MFREE(m, n) ((n) = m_free(m))

/*
 * Copy mbuf pkthdr from from to to.
 * from must have M_PKTHDR set, and to must be empty.
 * aux pointer will be moved to `to'.
 */
#define	M_COPY_PKTHDR(to, from)		m_copy_pkthdr(to, from)

/*
 * Set the m_data pointer of a newly-allocated mbuf (m_get/MGET) to place
 * an object of the specified size at the end of the mbuf, longword aligned.
 */
#define	M_ALIGN(m, len)				\
	{ (m)->m_data += (MLEN - (len)) &~ (sizeof(long) - 1); }
/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_COPY_PKTHDR.
 */
#define	MH_ALIGN(m, len) \
	{ (m)->m_data += (MHLEN - (len)) &~ (sizeof(long) - 1); }

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 * Subroutine - data not available if certain references.
 */
#define	M_LEADINGSPACE(m)	m_leadingspace(m)

/*
 * Compute the amount of space available
 * after the end of data in an mbuf.
 * Subroutine - data not available if certain references.
 */
#define	M_TRAILINGSPACE(m)	m_trailingspace(m)

/*
 * Arrange to prepend space of size plen to mbuf m.
 * If a new mbuf must be allocated, how specifies whether to wait.
 * If how is M_DONTWAIT and allocation fails, the original mbuf chain
 * is freed and m is set to NULL.
 */
#define	M_PREPEND(m, plen, how) 	((m) = m_prepend_2((m), (plen), (how)))

/* change mbuf to new type */
#define MCHTYPE(m, t) 		m_mchtype(m, t)

/* length to m_copy to copy all */
#define	M_COPYALL	1000000000

/* compatiblity with 4.3 */
#define  m_copy(m, o, l)	m_copym((m), (o), (l), M_DONTWAIT)

#endif /* KERNEL_PRIVATE */

/*
 * Mbuf statistics.
 */
/* LP64todo - not 64-bit safe */
#if 0
struct mbstat {
	u_long  m_mbufs;        /* mbufs obtained from page pool */
	u_long  m_clusters;     /* clusters obtained from page pool */
	u_long  m_spare;        /* spare field */
	u_long  m_clfree;       /* free clusters */
	u_long  m_drops;        /* times failed to find space */
	u_long  m_wait;         /* times waited for space */
	u_long  m_drain;        /* times drained protocols for space */
	u_short m_mtypes[256];  /* type specific mbuf allocations */
	u_long  m_mcfail;       /* times m_copym failed */
	u_long  m_mpfail;       /* times m_pullup failed */
	u_long  m_msize;        /* length of an mbuf */
	u_long  m_mclbytes;     /* length of an mbuf cluster */
	u_long  m_minclsize;    /* min length of data to allocate a cluster */
	u_long  m_mlen;         /* length of data in an mbuf */
	u_long  m_mhlen;        /* length of data in a header mbuf */
	u_long  m_bigclusters;     /* clusters obtained from page pool */
	u_long  m_bigclfree;       /* free clusters */
	u_long  m_bigmclbytes;     /* length of an mbuf cluster */
};

/* Compatibillity with 10.3 */
struct ombstat {
	u_long	m_mbufs;	/* mbufs obtained from page pool */
	u_long	m_clusters;	/* clusters obtained from page pool */
	u_long	m_spare;	/* spare field */
	u_long	m_clfree;	/* free clusters */
	u_long	m_drops;	/* times failed to find space */
	u_long	m_wait;		/* times waited for space */
	u_long	m_drain;	/* times drained protocols for space */
	u_short	m_mtypes[256];	/* type specific mbuf allocations */
	u_long	m_mcfail;	/* times m_copym failed */
	u_long	m_mpfail;	/* times m_pullup failed */
	u_long	m_msize;	/* length of an mbuf */
	u_long	m_mclbytes;	/* length of an mbuf cluster */
	u_long	m_minclsize;	/* min length of data to allocate a cluster */
	u_long	m_mlen;		/* length of data in an mbuf */
	u_long	m_mhlen;	/* length of data in a header mbuf */
};
#endif
#ifdef KERNEL_PRIVATE

/*
 * pkthdr.aux type tags.
 */
struct mauxtag {
	int af;
	int type;
};

#ifdef	KERNEL
extern union 	mcluster *mbutl;	/* virtual address of mclusters */
extern union 	mcluster *embutl;	/* ending virtual address of mclusters */
extern short 	*mclrefcnt;		/* cluster reference counts */
extern int 	*mcl_paddr;		/* physical addresses of clusters */
extern struct 	mbstat mbstat;		/* statistics */
extern int 	nmbclusters;		/* number of mapped clusters */
extern union 	mcluster *mclfree;	/* free mapped cluster list */
extern int	max_linkhdr;		/* largest link-level header */
extern int	max_protohdr;		/* largest protocol header */
extern int	max_hdr;		/* largest link+protocol header */
extern int	max_datalen;		/* MHLEN - max_hdr */

__BEGIN_DECLS
struct	mbuf *m_copym(struct mbuf *, int, int, int);
struct	mbuf *m_split(struct mbuf *, int, int);
struct	mbuf *m_free(struct mbuf *);
struct	mbuf *m_get(int, int);
struct	mbuf *m_getpacket(void);
struct	mbuf *m_getclr(int, int);
struct	mbuf *m_gethdr(int, int);
struct	mbuf *m_prepend(struct mbuf *, int, int);
struct  mbuf *m_prepend_2(struct mbuf *, int, int);
struct	mbuf *m_pullup(struct mbuf *, int);
struct	mbuf *m_retry(int, int);
struct	mbuf *m_retryhdr(int, int);
void m_adj(struct mbuf *, int);
void m_freem(struct mbuf *);
int m_freem_list(struct mbuf *);
struct	mbuf *m_devget(char *, int, int, struct ifnet *, void (*)(const void *, void *, size_t));
char   *mcl_to_paddr(char *);
struct mbuf *m_pulldown(struct mbuf*, int, int, int*);
struct mbuf *m_aux_add(struct mbuf *, int, int);
struct mbuf *m_aux_find(struct mbuf *, int, int);
void m_aux_delete(struct mbuf *, struct mbuf *);

struct mbuf *m_mclget(struct mbuf *, int);
caddr_t m_mclalloc(int);
void m_mclfree(caddr_t p);
int m_mclhasreference(struct mbuf *);
void m_copy_pkthdr(struct mbuf *, struct mbuf*);

int m_mclref(struct mbuf *);
int m_mclunref(struct mbuf *);

void *          m_mtod(struct mbuf *);
struct mbuf *   m_dtom(void *);
int             m_mtocl(void *);
union mcluster *m_cltom(int );

int m_trailingspace(struct mbuf *);
int m_leadingspace(struct mbuf *);

void m_mchtype(struct mbuf *m, int t);
void m_mcheck(struct mbuf*);

void m_copyback(struct mbuf *, int , int , caddr_t);
void m_copydata(struct mbuf *, int , int , caddr_t);
struct mbuf* m_dup(struct mbuf *m, int how);
void m_cat(struct mbuf *, struct mbuf *);
struct  mbuf *m_copym_with_hdrs(struct mbuf*, int, int, int, struct mbuf**, int*);
struct mbuf *m_getpackets(int, int, int);
struct mbuf * m_getpackethdrs(int , int );
struct mbuf* m_getpacket_how(int );
struct mbuf * m_getpackets_internal(unsigned int *, int , int , int , size_t);
struct mbuf * m_allocpacket_internal(unsigned int * , size_t , unsigned int *, int , int , size_t );

__END_DECLS

/*
 Packets may have annotations attached by affixing a list of "packet
 tags" to the pkthdr structure.  Packet tags are dynamically allocated
 semi-opaque data structures that have a fixed header (struct m_tag)
 that specifies the size of the memory block and an <id,type> pair that
 identifies it. The id identifies the module and the type identifies the
 type of data for that module. The id of zero is reserved for the kernel.
 
 Note that the packet tag returned by m_tag_allocate has the default
 memory alignment implemented by malloc.  To reference private data one
 can use a construct like:
 
      struct m_tag *mtag = m_tag_allocate(...);
      struct foo *p = (struct foo *)(mtag+1);
 
 if the alignment of struct m_tag is sufficient for referencing members
 of struct foo.  Otherwise it is necessary to embed struct m_tag within
 the private data structure to insure proper alignment; e.g.
 
      struct foo {
              struct m_tag    tag;
              ...
      };
      struct foo *p = (struct foo *) m_tag_allocate(...);
      struct m_tag *mtag = &p->tag;
 */

#define KERNEL_MODULE_TAG_ID	0

enum {
	KERNEL_TAG_TYPE_NONE			= 0,
	KERNEL_TAG_TYPE_DUMMYNET		= 1,
	KERNEL_TAG_TYPE_DIVERT			= 2,
	KERNEL_TAG_TYPE_IPFORWARD		= 3,
	KERNEL_TAG_TYPE_IPFILT			= 4
};

/*
 * As a temporary and low impact solution to replace the even uglier
 * approach used so far in some parts of the network stack (which relies
 * on global variables), packet tag-like annotations are stored in MT_TAG
 * mbufs (or lookalikes) prepended to the actual mbuf chain.
 *
 *      m_type  = MT_TAG
 *      m_flags = m_tag_id
 *      m_next  = next buffer in chain.
 *
 * BE VERY CAREFUL not to pass these blocks to the mbuf handling routines.
 */
#define _m_tag_id       m_hdr.mh_flags

__BEGIN_DECLS

/* Packet tag routines */
struct  m_tag   *m_tag_alloc(u_int32_t id, u_int16_t type, int len, int wait);
void             m_tag_free(struct m_tag *);
void             m_tag_prepend(struct mbuf *, struct m_tag *);
void             m_tag_unlink(struct mbuf *, struct m_tag *);
void             m_tag_delete(struct mbuf *, struct m_tag *);
void             m_tag_delete_chain(struct mbuf *, struct m_tag *);
struct  m_tag   *m_tag_locate(struct mbuf *,u_int32_t id, u_int16_t type,
							  struct m_tag *);
struct  m_tag   *m_tag_copy(struct m_tag *, int wait);
int              m_tag_copy_chain(struct mbuf *to, struct mbuf *from, int wait);
void             m_tag_init(struct mbuf *);
struct  m_tag   *m_tag_first(struct mbuf *);
struct  m_tag   *m_tag_next(struct mbuf *, struct m_tag *);

__END_DECLS

#endif /* KERNEL */

#endif /* KERNEL_PRIVATE */
#ifdef KERNEL
#include <sys/kpi_mbuf.h>
#endif
//#endif	/* !_SYS_MBUF_H_ */

/*
 * The general lock structure.  Provides for multiple shared locks,
 * upgrading from shared to exclusive, and sleeping until the lock
 * can be gained. The simple locks are defined in <machine/param.h>.
 */
struct lock__bsd__ {
	void * lk_interlock[10];		/* lock on remaining fields */
	u_int	lk_flags;		/* see below */
	int	lk_sharecount;		/* # of accepted shared locks */
	int	lk_waitcount;		/* # of processes sleeping for lock */
	short	lk_exclusivecount;	/* # of recursive exclusive locks */
	short	lk_prio;		/* priority at which to sleep */
	const char	*lk_wmesg;	/* resource sleeping (for tsleep) */
	int	lk_timo;		/* maximum sleep time (for tsleep) */
	pid_t	lk_lockholder;		/* pid of exclusive lock holder */
	void	*lk_lockthread;		/* thread which acquired excl lock */
};

/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995, 1997 Apple Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)proc.h	8.15 (Berkeley) 5/19/95
 */

//#ifndef _SYS_PROC_H_
//#define	_SYS_PROC_H_

#include <sys/appleapiopts.h>
#include <sys/cdefs.h>
#include <sys/select.h>			/* For struct selinfo. */
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/event.h>

#ifdef __APPLE_API_PRIVATE

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	proc *s_leader;		/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	pid_t	s_sid;		/* Session ID */
	char	s_login[MAXLOGNAME];	/* Setlogin() name. */
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * Description of a process.
 *
 * This structure contains the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressible except for those marked "(PROC ONLY)" below,
 * which might be addressible only on a processor on which the process
 * is running.
 */
struct	proc {
	LIST_ENTRY(proc) p_list;	/* List of all processes. */

	/* substructures: */
	struct	pcred *p_cred;		/* Process owner's identity. */
	struct	filedesc *p_fd;		/* Ptr to open files structure. */
	struct	pstats *p_stats;	/* Accounting/statistics (PROC ONLY). */
	struct	plimit *p_limit;	/* Process limits. */
	struct	sigacts *p_sigacts;	/* Signal actions, state (PROC ONLY). */

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int	p_flag;			/* P_* flags. */
	char	p_stat;			/* S* process status. */
        char	p_shutdownstate;
	char	p_pad1[2];

	pid_t	p_pid;			/* Process identifier. */
	LIST_ENTRY(proc) p_pglist;	/* List of processes in pgrp. */
	struct	proc *p_pptr;	 	/* Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid

	pid_t	p_oppid;	 /* Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* Sideways return value from fdopen. XXX */

	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* %cpu for this process during p_swtime */
	void	*p_wchan;	 /* Sleep address. */
	char	*p_wmesg;	 /* Reason for sleep. */
	u_int	p_swtime;	 /* DEPRECATED (Time swapped in or out.) */
#define	p_argslen p_swtime	 /* Length of process arguments. */
	u_int	p_slptime;	 /* Time since last blocked. */

	struct	itimerval p_realtimer;	/* Alarm timer. */
	struct	timeval p_rtime;	/* Real time. */
	u_quad_t p_uticks;		/* Statclock hits in user mode. */
	u_quad_t p_sticks;		/* Statclock hits in system mode. */
	u_quad_t p_iticks;		/* Statclock hits processing intr. */

	int	p_traceflag;		/* Kernel trace points. */
	struct	vnode *p_tracep;	/* Trace to vnode. */

	sigset_t p_siglist;		/* DEPRECATED. */

	struct	vnode *p_textvp;	/* Vnode of executable. */

/* End area that is zeroed on creation. */
#define	p_endzero	p_hash.le_next

	/*
	 * Not copied, not zero'ed.
	 * Belongs after p_pid, but here to avoid shifting proc elements.
	 */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */
	TAILQ_HEAD( ,eventqelt) p_evlist;

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask

	sigset_t p_sigmask;		/* DEPRECATED */
	sigset_t p_sigignore;	/* Signals being ignored. */
	sigset_t p_sigcatch;	/* Signals being caught by user. */

	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and p_nice. */
	char	p_nice;		/* Process "nice" value. */
	char	p_comm[MAXCOMLEN+1];

	struct 	pgrp *p_pgrp;	/* Pointer to process group. */

/* End area that is copied on creation. */
#define	p_endcopy	p_xstat
	
	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
	u_short	p_acflag;	/* Accounting flags. */
	struct	rusage *p_ru;	/* Exit information. XXX */

	int	p_debugger;	/* 1: can exec set-bit programs if suser */

	void	*task;			/* corresponding task */
	void	*sigwait_thread;	/* 'thread' holding sigwait */
	struct lock__bsd__ signal_lock;	/* multilple thread prot for signals*/
	boolean_t	 sigwait;	/* indication to suspend */
	void	*exit_thread;		/* Which thread is exiting? */
	caddr_t	user_stack;		/* where user stack was allocated */
	void * exitarg;			/* exit arg for proc terminate */
	void * vm_shm;			/* for sysV shared memory */
	int  p_argc;			/* saved argc for sysctl_procargs() */
	int		p_vforkcnt;		/* number of outstanding vforks */
    void *  p_vforkact;     /* activation running this vfork proc */
	TAILQ_HEAD( , uthread) p_uthlist; /* List of uthreads  */
	/* Following fields are info from SIGCHLD */
	pid_t	si_pid;
	u_short si_status;
	u_short	si_code;
	uid_t	si_uid;
	TAILQ_HEAD( , aio_workq_entry ) aio_activeq; /* active async IO requests */
	int		aio_active_count;	/* entries on aio_activeq */
	TAILQ_HEAD( , aio_workq_entry ) aio_doneq;	 /* completed async IO requests */
	int		aio_done_count;		/* entries on aio_doneq */

	struct klist p_klist;  /* knote list */
	struct  auditinfo		*p_au;	/* User auditing data */
#if DIAGNOSTIC
#if SIGNAL_DEBUG
	unsigned int lockpc[8];
	unsigned int unlockpc[8];
#endif /* SIGNAL_DEBUG */
#endif /* DIAGNOSTIC */
};

#else /* !__APPLE_API_PRIVATE */
struct session;
struct pgrp;
struct proc;
#endif /* !__APPLE_API_PRIVATE */

#ifdef __APPLE_API_UNSTABLE
/* Exported fields for kern sysctls */
struct extern_proc {
	union {
		struct {
			struct	proc *__p_forw;	/* Doubly-linked run/sleep queue. */
			struct	proc *__p_back;
		} p_st1;
		struct timeval __p_starttime; 	/* process start time */
	} p_un;
#define p_forw p_un.p_st1.__p_forw
#define p_back p_un.p_st1.__p_back
#define p_starttime p_un.__p_starttime
	struct	vmspace *p_vmspace;	/* Address space. */
	struct	sigacts *p_sigacts;	/* Signal actions, state (PROC ONLY). */
	int	p_flag;			/* P_* flags. */
	char	p_stat;			/* S* process status. */
	pid_t	p_pid;			/* Process identifier. */
	pid_t	p_oppid;	 /* Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* Sideways return value from fdopen. XXX */
	/* Mach related  */
	caddr_t user_stack;	/* where user stack was allocated */
	void	*exit_thread;	/* XXX Which thread is exiting? */
	int		p_debugger;		/* allow to debug */
	boolean_t	sigwait;	/* indication to suspend */
	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* %cpu for this process during p_swtime */
	void	*p_wchan;	 /* Sleep address. */
	char	*p_wmesg;	 /* Reason for sleep. */
	u_int	p_swtime;	 /* Time swapped in or out. */
	u_int	p_slptime;	 /* Time since last blocked. */
	struct	itimerval p_realtimer;	/* Alarm timer. */
	struct	timeval p_rtime;	/* Real time. */
	u_quad_t p_uticks;		/* Statclock hits in user mode. */
	u_quad_t p_sticks;		/* Statclock hits in system mode. */
	u_quad_t p_iticks;		/* Statclock hits processing intr. */
	int	p_traceflag;		/* Kernel trace points. */
	struct	vnode *p_tracep;	/* Trace to vnode. */
	int	p_siglist;		/* DEPRECATED */
	struct	vnode *p_textvp;	/* Vnode of executable. */
	int	p_holdcnt;		/* If non-zero, don't swap. */
	sigset_t p_sigmask;	/* DEPRECATED. */
	sigset_t p_sigignore;	/* Signals being ignored. */
	sigset_t p_sigcatch;	/* Signals being caught by user. */
	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and p_nice. */
	char	p_nice;		/* Process "nice" value. */
	char	p_comm[MAXCOMLEN+1];
	struct 	pgrp *p_pgrp;	/* Pointer to process group. */
	struct	user *p_addr;	/* Kernel virtual addr of u-area (PROC ONLY). */
	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
	u_short	p_acflag;	/* Accounting flags. */
	struct	rusage *p_ru;	/* Exit information. XXX */
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/* Status values. */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */

/* These flags are kept in p_flags. */
#define	P_ADVLOCK	0x00001	/* Process may hold a POSIX advisory lock. */
#define	P_CONTROLT	0x00002	/* Has a controlling terminal. */
#define	P_INMEM		0x00004	/* Loaded into memory. */
#define	P_NOCLDSTOP	0x00008	/* No SIGCHLD when children stop. */
#define	P_PPWAIT	0x00010	/* Parent is waiting for child to exec/exit. */
#define	P_PROFIL	0x00020	/* Has started profiling. */
#define	P_SELECT	0x00040	/* Selecting; wakeup/waiting danger. */
#define	P_SINTR		0x00080	/* Sleep is interruptible. */
#define	P_SUGID		0x00100	/* Had set id privileges since last exec. */
#define	P_SYSTEM	0x00200	/* System proc: no sigs, stats or swapping. */
#define	P_TIMEOUT	0x00400	/* Timing out during sleep. */
#define	P_TRACED	0x00800	/* Debugged process being traced. */
#define	P_WAITED	0x01000	/* Debugging process has waited for child. */
#define	P_WEXIT		0x02000	/* Working on exiting. */
#define	P_EXEC		0x04000	/* Process called exec. */

/* Should be moved to machine-dependent areas. */
#define	P_OWEUPC	0x08000	/* Owe process an addupc() call at next ast. */

#define	P_AFFINITY	0x0010000	/* xxx */
#define	P_CLASSIC	0x0020000	/* xxx */
/*
#define	P_FSTRACE	  0x10000	/ * tracing via file system (elsewhere?) * /
#define	P_SSTEP		  0x20000	/ * process needs single-step fixup ??? * /
*/

#define	P_WAITING	0x0040000	/* process has a wait() in progress */
#define	P_KDEBUG	0x0080000	/* kdebug tracing is on for this process */
#define	P_TTYSLEEP	0x0100000	/* blocked due to SIGTTOU or SIGTTIN */
#define	P_REBOOT	0x0200000	/* Process called reboot() */
#define	P_TBE		0x0400000	/* Process is TBE */
#define	P_SIGEXC	0x0800000	/* signal exceptions */
#define	P_BTRACE	0x1000000	/* process is being branch traced */
#define	P_VFORK		0x2000000	/* process has vfork children */
#define	P_NOATTACH	0x4000000
#define	P_INVFORK	0x8000000	/* proc in vfork */
#define	P_NOSHLIB	0x10000000	/* no shared libs are in use for proc */
					/* flag set on exec */
#define	P_FORCEQUOTA	0x20000000	/* Force quota for root */
#define	P_NOCLDWAIT	0x40000000	/* No zombies when chil procs exit */
#define	P_NOREMOTEHANG	0x80000000	/* Don't hang on remote FS ops */

#define	P_NOSWAP	0		/* Obsolete: retained so that nothing breaks */
#define	P_PHYSIO	0		/* Obsolete: retained so that nothing breaks */
#define	P_FSTRACE	0		/* Obsolete: retained so that nothing breaks */
#define	P_SSTEP		0		/* Obsolete: retained so that nothing breaks */

/*
 * Shareable process credentials (always resident).  This includes a reference
 * to the current user credentials as well as real and saved ids that may be
 * used to change ids.
 */
#if 0
struct	pcred {
	struct	lock__bsd__ pc_lock;
	struct	ucred *pc_ucred;	/* Current credentials. */
	uid_t	p_ruid;			/* Real user id. */
	uid_t	p_svuid;		/* Saved effective user id. */
	gid_t	p_rgid;			/* Real group id. */
	gid_t	p_svgid;		/* Saved effective group id. */
	int	p_refcnt;		/* Number of references. */
};
#endif 

#define pcred_readlock(p)	lockmgr(&(p)->p_cred->pc_lock,		\
						LK_SHARED, 0, (p))
#define pcred_writelock(p)	lockmgr(&(p)->p_cred->pc_lock,		\
						LK_EXCLUSIVE, 0, (p))
#define pcred_unlock(p)		lockmgr(&(p)->p_cred->pc_lock,		\
						LK_RELEASE, 0, (p))
#endif /* __APPLE_API_UNSTABLE */

#ifdef KERNEL

__BEGIN_DECLS
#ifdef __APPLE_API_PRIVATE
/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 */
extern int nprocs, maxproc;		/* Current and max number of procs. */
__private_extern__ int hard_maxproc;	/* hard limit */

#define	PID_MAX		30000
#define	NO_PID		30001

#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s)	sessrele(s)

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

LIST_HEAD(proclist, proc);
extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */
extern struct proc *initproc, *kernproc;
extern void	pgdelete __P((struct pgrp *pgrp));
extern void	sessrele __P((struct session *sess));
extern void	procinit __P((void));
__private_extern__ char *proc_core_name(const char *name, uid_t uid, pid_t pid);
extern int proc_is_classic(struct proc *p);
struct proc *current_proc_EXTERNAL(void);
#endif /* __APPLE_API_PRIVATE */

#ifdef __APPLE_API_UNSTABLE

extern int isinferior(struct proc *, struct proc *);
extern struct	proc *pfind __P((pid_t));	/* Find process by id. */
__private_extern__ struct proc *pzfind(pid_t);	/* Find zombie by id. */
extern struct	pgrp *pgfind __P((pid_t));	/* Find process group by id. */

extern int	chgproccnt __P((uid_t uid, int diff));
extern int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
extern void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
extern int	inferior __P((struct proc *p));
extern int	leavepgrp __P((struct proc *p));
#ifdef __APPLE_API_OBSOLETE
extern void	mi_switch __P((void));
#endif /* __APPLE_API_OBSOLETE */
extern void	resetpriority __P((struct proc *));
extern void	setrunnable __P((struct proc *));
extern void	setrunqueue __P((struct proc *));
extern int	sleep __P((void *chan, int pri));
extern int	tsleep __P((void *chan, int pri, char *wmesg, int timo));
extern int	tsleep0 __P((void *chan, int pri, char *wmesg, int timo, int (*continuation)(int)));
extern int	tsleep1 __P((void *chan, int pri, char *wmesg, u_int64_t abstime, int (*continuation)(int)));
extern void	unsleep __P((struct proc *));
extern void	wakeup __P((void *chan));
#endif /* __APPLE_API_UNSTABLE */

__END_DECLS

#ifdef __APPLE_API_OBSOLETE
/* FreeBSD source compatibility macro */ 
#define PRISON_CHECK(p1, p2)	(1)
#endif /* __APPLE_API_OBSOLETE */

#endif	/* KERNEL */

//#endif	/* !_SYS_PROC_H_ */

#endif	// _EXCLUDED_BY_APPLE_H_
