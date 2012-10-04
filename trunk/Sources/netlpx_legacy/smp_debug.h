/***************************************************************************
 *
 *  smp_debug.h
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
 *  @(#)smp_debug.h
 *
 * $FreeBSD: src/sys/netlpx/smp_debug.h,v 1.14 2002/07/27 23:15:08 dwmalone Exp $
 */

#ifndef _NETLPX_SMP_DEBUG_H_
#define _NETLPX_SMP_DEBUG_H_

struct  smp_debug {
    u_long  sd_time;
    short   sd_act;
    short   sd_ostate;
    caddr_t sd_cb;
    short   sd_req;
    struct  smp sd_si;
    struct  smppcb sd_sp;
};

#define SA_INPUT    0
#define SA_OUTPUT   1
#define SA_USER     2
#define SA_RESPOND  3
#define SA_DROP     4

#ifdef SANAMES
const char *smpnames[] =
    { "input", "output", "user", "respond", "drop" };
#endif

#define SMP_NDEBUG 100
#ifndef KERNEL
/* XXX common variables for broken applications. */
struct  smp_debug smp_debug[SMP_NDEBUG];
int smp_debx;
#endif

#ifdef KERNEL
extern char *prurequests[];
extern char *sanames[];
extern char *tcpstates[];

void    Smp_trace(int act, int ostate, struct smppcb *sp, struct smp *si,
               int req);
#endif

#endif /* !_NETLPX_SMP_DEBUG_H_ */
