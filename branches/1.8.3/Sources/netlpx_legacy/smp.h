/***************************************************************************
 *
 *  smp.h
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
 *  @(#)smp.h
 *
 * $FreeBSD: src/sys/netlpx/smp.h,v 1.17 2002/03/20 02:39:13 alfred Exp $
 */

#ifndef _NETLPX_SMP_H_
#define _NETLPX_SMP_H_

/*
 * Definitions for LPX style Sequenced Packet Protocol
 */

struct smphdr {
    u_char  smp_cc;     /* connection control */
    u_char  smp_dt;     /* datastream type */
#define SMP_SP  0x80        /* system packet */
#define SMP_SA  0x40        /* send acknowledgement */
#define SMP_OB  0x20        /* attention (out of band data) */
#define SMP_EM  0x10        /* end of message */
    u_short smp_sid;    /* source connection identifier */
    u_short smp_did;    /* destination connection identifier */
    u_short smp_seq;    /* sequence number */
    u_short smp_ack;    /* acknowledge number */
    u_short smp_alo;    /* allocation number */
	u_char  smp_tag;
};

/*
 * Definitions for NS(tm) Internet Datagram Protocol
 * containing a Sequenced Packet Protocol packet.
 */
struct smp {
    struct lpx  si_i;
    struct smphdr   si_s;
};
struct smp_q {
    struct smp_q    *si_next;
    struct smp_q    *si_prev;
};
#define SI(x)   ((struct smp *)x)
#define si_sum  si_i.lpx_sum
#define si_len  si_i.lpx_len
#define si_tc   si_i.lpx_tc
#define si_pt   si_i.lpx_pt
#define si_dna  si_i.lpx_dna
#define si_sna  si_i.lpx_sna
#define si_sport    si_i.lpx_sna.x_port
#define si_cc   si_s.smp_cc
#define si_dt   si_s.smp_dt
#define si_sid  si_s.smp_sid
#define si_did  si_s.smp_did
#define si_seq  si_s.smp_seq
#define si_ack  si_s.smp_ack
#define si_alo  si_s.smp_alo

/*
 * SMP control block, one per connection
 */
struct smppcb {
    struct  smp_q   s_q;        /* queue for out-of-order receipt */
    struct  lpxpcb  *s_lpxpcb;  /* backpointer to internet pcb */
    u_char  s_state;
    u_short  s_flags;
#define SF_ACKNOW   0x0001        /* Ack peer immediately */
#define SF_DELACK   0x0002        /* Ack, but try to delay it */
#define SF_ACKREQ   0x0004        /* Ack peer immediately */
#define SF_HI       0x0004        /* Show headers on input */
#define SF_HO       0x0008        /* Show headers on output */
#define SF_PI       0x0010        /* Packet (datagram) interface */
#define SF_WIN      0x0020        /* Window info changed */
#define SF_RXT      0x0040        /* Rxt info changed */
#define SF_RVD      0x0080        /* Calling from read usrreq routine */

#define SF_NEEDFIN  0x0100	

    u_short s_mtu;          /* Max packet size for this stream */
/* use sequence fields in headers to store sequence numbers for this
   connection */
    struct  lpx *s_lpx;
    struct  smphdr  s_shdr;     /* prototype header to transmit */
#define s_cc s_shdr.smp_cc      /* connection control (for EM bit) */
#define s_dt s_shdr.smp_dt      /* datastream type */
#define s_sid s_shdr.smp_sid        /* source connection identifier */
#define s_did s_shdr.smp_did        /* destination connection identifier */
#define s_seq s_shdr.smp_seq        /* sequence number */
#define s_ack s_shdr.smp_ack        /* acknowledge number */
#define s_alo s_shdr.smp_alo        /* allocation number */
#define s_dport s_lpx->lpx_dna.x_port   /* where we are sending */
#define s_tag s_shdr.smp_tag			/* Tag Field */
    struct smphdr s_rhdr;       /* last received header (in effect!)*/
    u_short s_rack;         /* their acknowledge number */
    u_short s_ralo;         /* their allocation number */
    u_short s_smax;         /* highest packet # we have sent */
    u_short s_snxt;         /* which packet to send next */

    u_short s_fseq;
/* congestion control */
#define CUNIT   1024            /* scaling for ... */
    int s_cwnd;         /* Congestion-controlled window */
                    /* in packets * CUNIT */
    short   s_swnd;         /* == tcp snd_wnd, in packets */
    short   s_smxw;         /* == tcp max_sndwnd */
                    /* difference of two smp_seq's can be
                       no bigger than a short */
    u_short s_swl1;         /* == tcp snd_wl1 */
    u_short s_swl2;         /* == tcp snd_wl2 */
    int s_cwmx;         /* max allowable cwnd */
    int s_ssthresh;     /* s_cwnd size threshold for
                     * slow start exponential-to-
                     * linear switch */
/* transmit timing stuff
 * srtt and rttvar are stored as fixed point, for convenience in smoothing.
 * srtt has 3 bits to the right of the binary point, rttvar has 2.
 */
    short   s_idle;         /* time idle */
#define SMPT_NTIMERS    4
    short   s_timer[SMPT_NTIMERS];  /* timers */
    short   s_rxtshift;     /* log(2) of rexmt exp. backoff */
    short   s_rxtcur;       /* current retransmit value */
    u_short s_rtseq;        /* packet being timed */
    short   s_rtt;          /* timer for round trips */
    short   s_srtt;         /* averaged timer */
    short   s_rttvar;       /* variance in round trip time */
    char    s_force;        /* which timer expired */
    char    s_dupacks;      /* counter to intuit xmt loss */

/* out of band data */
    char    s_oobflags;
#define SF_SOOB 0x08            /* sending out of band data */
#define SF_IOOB 0x10            /* receiving out of band data */
    char    s_iobc;         /* input characters */
/* debug stuff */
    u_short s_want;         /* Last candidate for sending */
    char    s_outx;         /* exit taken from smp_output */
    char    s_inx;          /* exit taken from smp_input */
    u_short s_flags2;       /* more flags for testing */
#define SF_NEWCALL  0x100       /* for new_recvmsg */
#define SO_NEWCALL  10      /* for new_recvmsg */

    __u16   sequence;

    int s_softerror;        /* possible error not yet reported */     
    
    char AlreadySendFin;
};

#define lpxtosmppcb(np) ((struct smppcb *)(np)->lpxp_pcb)
#define sotosmppcb(so)  (lpxtosmppcb(sotolpxpcb(so)))
#define smptolpxpcb(smp) ((struct lpxpcb *)(smp)->s_lpxpcb)

#ifdef KERNEL

extern struct pr_usrreqs smp_usrreqs;
extern struct pr_usrreqs smp_usrreq_sps;

#endif /* _KERNEL */

#endif /* !_NETLPX_SMP_H_ */
