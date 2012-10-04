/***************************************************************************
 *
 *  lpx_proto.c
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
 *  @(#)lpx_proto.c
 *
 * $FreeBSD: src/sys/netlpx/lpx_proto.c,v 1.15 1999/08/28 00:49:41 peter Exp $
 */

//#include "opt_lpx.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/radix.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_module.h>
#include <netlpx/lpx_input.h>
#include <netlpx/lpx_usrreq.h>
#include <netlpx/smp.h>
#include <netlpx/smp_usrreq.h>

extern  struct domain lpxdomain;
static  struct pr_usrreqs nousrreqs;

/*
 * LPX protocol family: LPX, ERR, PXP, SMP, ROUTE.
 */

struct protosw lpxsw[] = {
{ 0,        &lpxdomain, 0,      0,
  0,        0,      0,      0,
  0,
  Lpx_MODULE_init, 0,      0,      0,
  0,        &nousrreqs
},
{ SOCK_DGRAM,   &lpxdomain, 0,      PR_ATOMIC|PR_ADDR,
  0,        0,      Lpx_IN_ctl,   Lpx_USER_ctloutput,
  0,
  0,        0,      0,      0,
  0,        &lpx_usrreqs
},
{ SOCK_STREAM,  &lpxdomain, LPXPROTO_SMP,   PR_CONNREQUIRED|PR_WANTRCVD,
  0,        0,      Smp_ctlinput,   Smp_ctloutput,
  0,
  Smp_init, Smp_fasttimo,   Smp_slowtimo,   0,
  0,        &smp_usrreqs
},

#if 0

{ SOCK_SEQPACKET,&lpxdomain,    LPXPROTO_SMP,   PR_CONNREQUIRED|PR_WANTRCVD|PR_ATOMIC,
  0,        0,      Smp_ctlinput,   Smp_ctloutput,
  0,
  0,        0,      0,      0,
  0,        &smp_usrreq_sps
},
#endif
{ SOCK_RAW, &lpxdomain, LPXPROTO_RAW,   PR_ATOMIC|PR_ADDR,
  0,        0,      0,      Lpx_USER_ctloutput,
  0,
  0,        0,      0,      0,
  0,        &rlpx_usrreqs
},
#ifdef IPTUNNEL
#if 0
{ SOCK_RAW, &lpxdomain, IPPROTO_LPX,    PR_ATOMIC|PR_ADDR,
  iptun_input,  rip_output, iptun_ctlinput, 0,
  0,
  0,        0,      0,      0,
  &rip_usrreqs
},
#endif
#endif
};

struct  domain lpxdomain =
    { AF_LPX, "LPX", 0, 0, 0, 
      NULL, 0,
      rn_inithead, 32, sizeof(struct sockaddr_lpx), sizeof(struct lpxhdr), 0};

DOMAIN_SET(lpx);
SYSCTL_NODE(_net,   PF_LPX,     lpx,    CTLFLAG_RW, 0,
    "LPX/SMP");

SYSCTL_NODE(_net_lpx,   LPXPROTO_RAW,   lpx,    CTLFLAG_RW, 0, "LPX");
SYSCTL_NODE(_net_lpx,   LPXPROTO_SMP,   smp,    CTLFLAG_RW, 0, "SMP");
