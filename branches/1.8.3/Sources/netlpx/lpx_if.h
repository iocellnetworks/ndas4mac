/***************************************************************************
 *
 *  lpx_if.h
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
 *  @(#)lpx_if.h
 *
 * $FreeBSD: src/sys/netlpx/lpx_if.h,v 1.13 2003/03/04 23:19:53 jlemon Exp $
 */

#ifndef _NETLPX_LPX_IF_H_
#define _NETLPX_LPX_IF_H_

#include <net/if_var.h>

#include "lpx.h"

/*
 * Interface address.  One of these structures
 * is allocated for each interface with an internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct lpx_ifaddr {
	struct ifaddr		ia_ifa;				/* protocol-independent info */
#define ia_ifp          ia_ifa.ifa_ifp
#define ia_flags        ia_ifa.ifa_flags
    struct  lpx_ifaddr *ia_next;    /* next in list of lpx addresses */
    struct  sockaddr_lpx ia_addr;   /* reserve space for my address */
    struct  sockaddr_lpx ia_dstaddr;    /* space for my broadcast address */
#define ia_broadaddr    ia_dstaddr
  //  struct  sockaddr_lpx ia_netmask;    /* space for my network mask */
	struct  sockaddr_lpx ia_sockmask; /* reserve space for general netmask */
};

struct lpx_rtentry {
  struct lpx_addr ia_faddr;
  ifnet_t rt_ifp;
};


extern struct lpx_rtentry g_RtTbl[];
extern int g_iRtLastIndex;

struct  lpx_aliasreq {
    char    ifra_name[IFNAMSIZ];        /* if name, e.g. "en0" */
    struct  sockaddr_lpx ifra_addr;
    struct  sockaddr_lpx ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
};
/*
 * Given a pointer to an lpx_ifaddr (ifaddr),
 * return a pointer to the addr as a sockadd_lpx.
 */

#define IA_SLPX(ia) (&(((struct lpx_ifaddr *)(ia))->ia_addr))

#define ETHERTYPE_LPX       0x88AD  /* Only  Ethernet_II Available */

#ifdef  LPXIP	/* Not supported */
struct lpxip_req {
    struct sockaddr rq_lpx; /* must be lpx format destination */
    struct sockaddr rq_ip;  /* must be ip format gateway */
    short rq_flags;
};
#endif

#ifdef  KERNEL
extern struct		lpx_ifaddr *lpx_ifaddrs;
extern u_int32_t	min_mtu;

#endif

ifnet_t lpx_find_ifnet_by_addr(struct lpx_addr* lpx_addr);
errno_t lpx_ifnet_attach_protocol();
errno_t lpx_ifnet_detach_protocol();
errno_t lpx_ifnet_output(
	ifnet_t interface,
	mbuf_t	packet,
	struct sockaddr *dest
	);

#endif /* !_NETLPX_LPX_IF_H_ */
