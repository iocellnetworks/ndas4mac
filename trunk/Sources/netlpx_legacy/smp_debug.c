/***************************************************************************
 *
 *  smp_debug.c
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
 *  @(#)smp_debug.c
 *
 * $FreeBSD: src/sys/netlpx/smp_debug.c,v 1.14 1999/08/28 00:49:43 peter Exp $
 */

//#include "opt_inet.h"
//#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/protosw.h>

#include <netinet/in_systm.h>
#include <netinet/tcp_fsm.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>
#include <netlpx/smp.h>
#define SMPTIMERS
#include <netlpx/smp_timer.h>
#define SANAMES
#include <netlpx/smp_debug.h>

#ifdef TCPDEBUG
static  int smpconsdebug = 0;
static struct smp_debug smp_debug[SMP_NDEBUG];
static int smp_debx;
#endif

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

/*
 * smp debug routines
 */
void
Smp_trace(act, ostate, sp, si, req)
    short act;
    u_char ostate;
    struct smppcb *sp;
    struct smp *si;
    int req;
{
#ifdef INET
#ifdef TCPDEBUG
    u_short seq, ack, len, alo;
    int flags;
    struct smp_debug *sd = &smp_debug[smp_debx++];

    if (smp_debx == SMP_NDEBUG)
        smp_debx = 0;
    sd->sd_time = iptime();
    sd->sd_act = act;
    sd->sd_ostate = ostate;
    sd->sd_cb = (caddr_t)sp;
    if (sp != NULL)
        sd->sd_sp = *sp;
    else
        bzero((caddr_t)&sd->sd_sp, sizeof(*sp));
    if (si != NULL)
        sd->sd_si = *si;
    else
        bzero((caddr_t)&sd->sd_si, sizeof(*si));
    sd->sd_req = req;
    if (smpconsdebug == 0)
        return;
    if (ostate >= TCP_NSTATES)
        ostate = 0;
    if (act >= SA_DROP)
        act = SA_DROP;
    if (sp != NULL)
        DEBUG_CALL(2, ("%p %s:", (void *)sp, tcpstates[ostate]));
    else
        DEBUG_CALL(2, ("???????? "));
    DEBUG_CALL(2, ("%s ", smpnames[act]));
    switch (act) {

    case SA_RESPOND:
    case SA_INPUT:
    case SA_OUTPUT:
    case SA_DROP:
        if (si == NULL)
            break;
        seq = si->si_seq;
        ack = si->si_ack;
        alo = si->si_alo;
        len = si->si_len;
        if (act == SA_OUTPUT) {
            seq = ntohs(seq);
            ack = ntohs(ack);
            alo = ntohs(alo);
            len = ntohs(len);
        }
#ifndef lint
#define p1(f)  { DEBUG_CALL(2, ("%s = %x, ", "f", f)); }
        p1(seq); p1(ack); p1(alo); p1(len);
#endif
        flags = si->si_cc;
        if (flags) {
            char *cp = "<";
#ifndef lint
#define pf(f) { if (flags & SMP_ ## f) { DEBUG_CALL(2, ("%s%s", cp, "f")); cp = ","; } }
            pf(SP); pf(SA); pf(OB); pf(EM);
#else
            cp = cp;
#endif
            DEBUG_CALL(2, (">"));
        }
#ifndef lint
#define p2(f)  { DEBUG_CALL(2, ("%s = %x, ", "f", si->si_ ## f)); }
        p2(sid);p2(did);p2(dt);p2(pt);
#endif
        Lpx_CTL_printhost(&si->si_sna);
        Lpx_CTL_printhost(&si->si_dna);

        if (act == SA_RESPOND) {
            DEBUG_CALL(2, ("lpx_len = %x, ",
                ((struct lpx *)si)->lpx_len));
        }
        break;

    case SA_USER:
        DEBUG_CALL(2, ("%s", prurequests[req&0xff]));
        if ((req & 0xff) == PRU_SLOWTIMO)
            DEBUG_CALL(2, ("<%s>", smptimers[req>>8]));
        break;
    }
    if (sp)
        DEBUG_CALL(2, (" -> %s", tcpstates[sp->s_state]));
    /* print out internal state of sp !?! */
    DEBUG_CALL(2, ("\n"));
    if (sp == 0)
        return;
#ifndef lint
#define p3(f)  { DEBUG_CALL(2, ("%s = %x, ", "f", sp->s_ ## f)); }
    DEBUG_CALL(2, ("\t")); p3(rack);p3(ralo);p3(smax);p3(flags); DEBUG_CALL(2, ("\n"));
#endif
#endif
#endif
}
