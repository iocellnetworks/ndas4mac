/***************************************************************************
 *
 *  smp_timer.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *  @(#)smp_timer.h
 *
 * $FreeBSD: src/sys/netlpx/smp_timer.h,v 1.10 1999/08/28 00:49:44 peter Exp $
 */

#ifndef _NETLPX_SMP_TIMER_H_
#define _NETLPX_SMP_TIMER_H_

/*
 * Definitions of the SMP timers.  These timers are counted
 * down PR_SLOWHZ times a second.
 */
#define SMPT_REXMT  0       /* retransmit */
//#define SMPT_PERSIST    1       /* retransmit persistence */
#define SMPT_KEEP   2       /* keep alive */
#define SMPT_2MSL   3       /* 2*msl quiet time timer */
#define SMPT_DISCON	4

/*
 * The SMPT_REXMT timer is used to force retransmissions.
 * The SMP has the SMPT_REXMT timer set whenever segments
 * have been sent for which ACKs are expected but not yet
 * received.  If an ACK is received which advances tp->snd_una,
 * then the retransmit timer is cleared (if there are no more
 * outstanding segments) or reset to the base value (if there
 * are more ACKs expected).  Whenever the retransmit timer goes off,
 * we retransmit one unacknowledged segment, and do a backoff
 * on the retransmit timer.
 *
 * The SMPT_PERSIST timer is used to keep window size information
 * flowing even if the window goes shut.  If all previous transmissions
 * have been acknowledged (so that there are no retransmissions in progress),
 * and the window is too small to bother sending anything, then we start
 * the SMPT_PERSIST timer.  When it expires, if the window is nonzero,
 * we go to transmit state.  Otherwise, at intervals send a single byte
 * into the peer's window to force him to update our window information.
 * We do this at most as often as SMPT_PERSMIN time intervals,
 * but no more frequently than the current estimate of round-trip
 * packet time.  The SMPT_PERSIST timer is cleared whenever we receive
 * a window update from the peer.
 *
 * The SMPT_KEEP timer is used to keep connections alive.  If an
 * connection is idle (no segments received) for SMPTV_KEEP amount of time,
 * but not yet established, then we drop the connection.  If the connection
 * is established, then we force the peer to send us a segment by sending:
 *  <SEQ=SND.UNA-1><ACK=RCV.NXT><CTL=ACK>
 * This segment is (deliberately) outside the window, and should elicit
 * an ack segment in response from the peer.  If, despite the SMPT_KEEP
 * initiated segments we cannot elicit a response from a peer in SMPT_MAXIDLE
 * amount of time, then we drop the connection.
 */

#define SMP_TTL     30      /* default time to live for SMP segs */
/*
 * Time constants.
 */
#define SMPTV_MSL   ( 4 * PR_SLOWHZ)     /* max seg lifetime */
#define SMPTV_SRTTBASE  0           /* base roundtrip time;
                           if 0, no idea yet */
#define SMPTV_SRTTDFLT  (  3*PR_SLOWHZ)     /* assumed RTT if no info */

#define SMPTV_PERSMIN   (  5*PR_SLOWHZ)     /* retransmit persistence */
#define SMPTV_PERSMAX   ( 60*PR_SLOWHZ)     /* maximum persist interval */

#define SMPTV_KEEP		( PR_SLOWHZ)     /* keep alive - 500 msecs */
#define SMPTV_MAXIDLE   ( 5 * SMPTV_KEEP)    /* maximum allowable idle time before drop conn */
#define SMPTV_DISCON	( 4 * PR_SLOWHZ )	// Disconnect timeout

#define SMPTV_MIN   (  1 * PR_SLOWHZ)     /* minimum allowable value */
#define SMPTV_REXMTMAX  ( 64*PR_SLOWHZ)     /* max allowable REXMT value */

#define SMP_LINGERTIME  120         /* linger at most 2 minutes */

#define SMP_MAXRXTSHIFT 6          /* maximum retransmits */

#define SMP_MAXRXTSHIFTOFCONNECT 2	/* maximum retransmits */

#ifdef  SMPTIMERS
char *smptimers[] =
    { "REXMT", "PERSIST", "KEEP", "2MSL" };
#endif

/*
 * Force a time value to be in a certain range.
 */
#define SMPT_RANGESET(tv, value, tvmin, tvmax) { \
    (tv) = (value); \
    if ((tv) < (tvmin)) \
        (tv) = (tvmin); \
    else if ((tv) > (tvmax)) \
        (tv) = (tvmax); \
}

void lpx_stream_fasttimo();
void lpx_stream_slowtimo();

#endif /* !_NETLPX_SMP_TIMER_H_ */
