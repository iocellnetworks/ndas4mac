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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>

//#include "ExcludedByApple.h"

//#include "netlpx2.h"
#include "lpx.h"
#include "lpx_var.h"
//#include "lpx_input.h"
//#include "lpx_usrreq.h"
//#include "smp.h"
//#include "smp_usrreq.h"
#include "lpx_pcb.h"

#include "lpx_base.h"
#include "lpx_datagram.h"
#include "lpx_stream.h"
#include "lpx_stream_timer.h"

extern  struct domain lpxdomain;
static  struct pr_usrreqs nousrreqs;

/*
 * LPX protocol family: LPX, ERR, PXP, SMP, ROUTE.
 */

struct protosw lpxsw[] = {		
{	0,		&lpxdomain,		0,		0,
	0,		0,		0,		0,
	0,
	lpx_init,	0,		0,		0,
	0,	
	&nousrreqs,
	0,		0,		0,	{ 0, 0 },	0,	{ 0 }
},
{ SOCK_DGRAM,	&lpxdomain,	LPXPROTO_DGRAM,	PR_ATOMIC|PR_ADDR,
	0,	0,		0,	0,
	0,
	lpx_datagram_init,	0,		0,		0,
	0,
	&lpx_datagram_usrreqs,
	0,		0,		0,	{ 0, 0 },	0,	{ 0 }
},
{ SOCK_STREAM,	&lpxdomain,	LPXPROTO_STREAM, PR_CONNREQUIRED|PR_WANTRCVD,
	0,	0,		0,	0,
	0,
	lpx_stream_init,	lpx_stream_fasttimo,	lpx_stream_slowtimo,	0,
	0,
	&lpx_stream_usrreqs,
	0,		0,		0,	{ 0, 0 },	0,	{ 0 }
}
};
/*
struct protosw lpxsw[] = {
	{	0,		&lpxdomain,		0,		0,
		0,		0,		0,		0,
		0,
		lpx_init,	0,		0,		0,
		0,	
		&nousrreqs,
		0,		0,		0,	{ 0, 0 },	0,	{ 0 }
	},
	{ SOCK_DGRAM,	&lpxdomain,	LPXPROTO_DGRAM,	PR_ATOMIC|PR_ADDR|PR_PCBLOCK|PR_PROTOLOCK,
		0,	0,		0,	0,
		0,
		lpx_datagram_init,	0,		0,		0,
		0,
		&lpx_datagram_usrreqs,
		lpx_datagram_lock, lpx_datagram_unlock,	lpx_datagram_getlock,	{ 0, 0 },	0,	{ 0 }
	},
	{ SOCK_STREAM,	&lpxdomain,	LPXPROTO_STREAM, PR_CONNREQUIRED|PR_WANTRCVD|PR_PCBLOCK|PR_PROTOLOCK,
		0,	0,		0,	0,
		0,
		lpx_stream_init,	lpx_stream_fasttimo,	lpx_stream_slowtimo,	0,
		0,
		&lpx_stream_usrreqs,
		lpx_stream_lock, lpx_stream_unlock,	lpx_stream_getlock,	{ 0, 0 },	0,	{ 0 }
	}
};
*/

int lpx_proto_count = (sizeof(lpxsw) / sizeof(struct protosw));

struct  domain lpxdomain = 
{ AF_LPX, 
	"LPX", 
	lpx_domain_init, 
	0, 
	0, 
	NULL, 
	0,
	NULL,  
	0, 
	sizeof(struct sockaddr_lpx),
	sizeof(struct lpxhdr), 
	0,
	0,
	0,
	{ 0, 0 }
};

DOMAIN_SET(inet);

SYSCTL_NODE(_net,   PF_LPX,     lpx,    CTLFLAG_RW, 0,
    "LPXFamily");

SYSCTL_NODE(_net_lpx,   LPXPROTO_DGRAM,   datagram,    CTLFLAG_RW, 0, "LPXDatagram");
SYSCTL_NODE(_net_lpx,   LPXPROTO_STREAM,  stream,    CTLFLAG_RW, 0, "LPXStream");
