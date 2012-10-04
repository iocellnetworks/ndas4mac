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
 *  @(#)smp_var.h
 *
 * $FreeBSD: src/sys/netlpx/smp_var.h,v 1.9 1999/08/28 00:49:44 peter Exp $
 */

#ifndef _NETLPX_SMP_VAR_H_
#define _NETLPX_SMP_VAR_H_

struct  smpstat {
    long    smps_connattempt;   /* connections initiated */
    long    smps_accepts;       /* connections accepted */
    long    smps_connects;      /* connections established */
    long    smps_drops;     /* connections dropped */
    long    smps_conndrops;     /* embryonic connections dropped */
    long    smps_closed;        /* conn. closed (includes drops) */
    long    smps_segstimed;     /* segs where we tried to get rtt */
    long    smps_rttupdated;    /* times we succeeded */
    long    smps_delack;        /* delayed acks sent */
    long    smps_timeoutdrop;   /* conn. dropped in rxmt timeout */
    long    smps_rexmttimeo;    /* retransmit timeouts */
    long    smps_persisttimeo;  /* persist timeouts */
    long    smps_keeptimeo;     /* keepalive timeouts */
    long    smps_keepprobe;     /* keepalive probes sent */
    long    smps_keepdrops;     /* connections dropped in keepalive */

    long    smps_sndtotal;      /* total packets sent */
    long    smps_sndpack;       /* data packets sent */
    long    smps_sndbyte;       /* data bytes sent */
    long    smps_sndrexmitpack; /* data packets retransmitted */
    long    smps_sndrexmitbyte; /* data bytes retransmitted */
    long    smps_sndacks;       /* ack-only packets sent */
    long    smps_sndprobe;      /* window probes sent */
    long    smps_sndurg;        /* packets sent with URG only */
    long    smps_sndwinup;      /* window update-only packets sent */
    long    smps_sndctrl;       /* control (SYN|FIN|RST) packets sent */
    long    smps_sndvoid;       /* couldn't find requested packet*/

    long    smps_rcvtotal;      /* total packets received */
    long    smps_rcvpack;       /* packets received in sequence */
    long    smps_rcvbyte;       /* bytes received in sequence */
    long    smps_rcvbadsum;     /* packets received with ccksum errs */
    long    smps_rcvbadoff;     /* packets received with bad offset */
    long    smps_rcvshort;      /* packets received too short */
    long    smps_rcvduppack;    /* duplicate-only packets received */
    long    smps_rcvdupbyte;    /* duplicate-only bytes received */
    long    smps_rcvpartduppack;    /* packets with some duplicate data */
    long    smps_rcvpartdupbyte;    /* dup. bytes in part-dup. packets */
    long    smps_rcvoopack;     /* out-of-order packets received */
    long    smps_rcvoobyte;     /* out-of-order bytes received */
    long    smps_rcvpackafterwin;   /* packets with data after window */
    long    smps_rcvbyteafterwin;   /* bytes rcvd after window */
    long    smps_rcvafterclose; /* packets rcvd after "close" */
    long    smps_rcvwinprobe;   /* rcvd window probe packets */
    long    smps_rcvdupack;     /* rcvd duplicate acks */
    long    smps_rcvacktoomuch; /* rcvd acks for unsent data */
    long    smps_rcvackpack;    /* rcvd ack packets */
    long    smps_rcvackbyte;    /* bytes acked by rcvd acks */
    long    smps_rcvwinupd;     /* rcvd window update packets */
};
struct  smp_istat {
    short   hdrops;
    short   badsum;
    short   badlen;
    short   slotim;
    short   fastim;
    short   nonucn;
    short   noconn;
    short   notme;
    short   wrncon;
    short   bdreas;
    short   gonawy;
    short   notyet;
    short   lstdup;
    struct smpstat newstats;
};

#define SMP_ISSINCR 128
/*
 * smp sequence numbers are 16 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define SSEQ_LT(a,b)    (((short)((a)-(b))) < 0)
#define SSEQ_LEQ(a,b)   (((short)((a)-(b))) <= 0)
#define SSEQ_GT(a,b)    (((short)((a)-(b))) > 0)
#define SSEQ_GEQ(a,b)   (((short)((a)-(b))) >= 0)

#endif /* !_NETLPX_SMP_VAR_H_ */
