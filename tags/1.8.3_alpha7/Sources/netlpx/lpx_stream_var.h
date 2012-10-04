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
    u_char  lpx_stream_cc;     /* connection control */
    u_char  lpx_stream_dt;     /* datastream type */
#define SMP_SP  0x80        /* system packet */
#define SMP_SA  0x40        /* send acknowledgement */
#define SMP_OB  0x20        /* attention (out of band data) */
#define SMP_EM  0x10        /* end of message */
    u_short lpx_stream_sid;    /* source connection identifier */
    u_short lpx_stream_did;    /* destination connection identifier */
    u_short lpx_stream_seq;    /* sequence number */
    u_short lpx_stream_ack;    /* acknowledge number */
    u_short lpx_stream_alo;    /* allocation number */
	u_char  lpx_stream_tag;
};

/*
 * Definitions for NS(tm) Internet Datagram Protocol
 * containing a Sequenced Packet Protocol packet.
 */
struct smp {
    struct lpx  si_i;
    struct smphdr   si_s;
};
struct lpx_stream_q {
    struct lpx_stream_q    *si_next;
    struct lpx_stream_q    *si_prev;
};
#define SI(x)   ((struct smp *)x)
#define si_sum  si_i.lpx_sum
#define si_len  si_i.lpx_len
#define si_tc   si_i.lpx_tc
#define si_pt   si_i.lpx_pt
#define si_dna  si_i.lpx_dna
#define si_sna  si_i.lpx_sna
#define si_sport    si_i.lpx_sna.x_port
#define si_cc   si_s.lpx_stream_cc
#define si_dt   si_s.lpx_stream_dt
#define si_sid  si_s.lpx_stream_sid
#define si_did  si_s.lpx_stream_did
#define si_seq  si_s.lpx_stream_seq
#define si_ack  si_s.lpx_stream_ack
#define si_alo  si_s.lpx_stream_alo

/*
 * SMP control block, one per connection
 */
struct stream_pcb {
    struct  lpx_stream_q   s_q;        /* queue for out-of-order receipt */
    struct  lpxpcb  *s_lpxpcb;  /* backpointer to internal pcb */
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

	u_short  s_flags_control;

#define SF_NEEDFREE		0x0001
#define SF_NEEDRELEASE	0x0002
#define SF_SOCKETFREED	0x0004

    u_short s_mtu;          /* Max packet size for this stream */
/* use sequence fields in headers to store sequence numbers for this
   connection */
    struct  lpx *s_lpx;
    struct  smphdr  s_shdr;     /* prototype header to transmit */
#define s_cc s_shdr.lpx_stream_cc      /* connection control (for EM bit) */
#define s_dt s_shdr.lpx_stream_dt      /* datastream type */
#define s_sid s_shdr.lpx_stream_sid        /* source connection identifier */
#define s_did s_shdr.lpx_stream_did        /* destination connection identifier */
#define s_seq s_shdr.lpx_stream_seq        /* sequence number */
#define s_ack s_shdr.lpx_stream_ack        /* acknowledge number */
#define s_alo s_shdr.lpx_stream_alo        /* allocation number */
#define s_dport s_lpx->lpx_dna.x_port   /* where we are sending */
#define s_tag s_shdr.lpx_stream_tag			/* Tag Field */
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
                    /* difference of two lpx_stream_seq's can be
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
    char    s_outx;         /* exit taken from lpx_stream_output */
    char    s_inx;          /* exit taken from lpx_stream_input */
    u_short s_flags2;       /* more flags for testing */
#define SF_NEWCALL  0x100       /* for new_recvmsg */
#define SO_NEWCALL  10      /* for new_recvmsg */

    __u16   sequence;

    int s_softerror;        /* possible error not yet reported */     
    
    char AlreadySendFin;
	
	char		bSmallLengthDelayedAck;
	__u8		delayedAckCount;
};

#define lpxpcbtostreampcb(np) ((struct stream_pcb *)(np)->lpxp_pcb)
#define sotostreampcb(so)  (lpxpcbtostreampcb(sotolpxpcb(so)))
#define streampcbtolpxpcb(smp) ((struct lpxpcb *)(smp)->s_lpxpcb)

#ifdef KERNEL

extern struct pr_usrreqs lpx_stream_usrreqs;
extern struct pr_usrreqs lpx_stream_usrreq_sps;

#endif /* _KERNEL */

struct  lpx_stream_stat {
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
struct  lpx_stream_istat {
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
    struct lpx_stream_stat newstats;
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

//
// LPX Stream state definitions.
//

#define	LPX_STREAM_NSTATES	11

#define	LPX_STREAM_CLOSED		0	/* closed */
#define	LPX_STREAM_LISTEN		1	/* listening for connection */
#define	LPX_STREAM_SYN_SENT		2	/* active, have sent syn */
#define	LPX_STREAM_SYN_RECEIVED	3	/* have send and received syn */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define	LPX_STREAM_ESTABLISHED	4	/* established */
#define	LPX_STREAM_CLOSE_WAIT		5	/* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define	LPX_STREAM_FIN_WAIT_1		6	/* have closed, sent fin */
#define	LPX_STREAM_CLOSING		7	/* closed xchd FIN; await FIN ACK */
#define	LPX_STREAM_LAST_ACK		8	/* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define	LPX_STREAM_FIN_WAIT_2		9	/* have closed, fin is acked */
#define	LPX_STREAM_TIME_WAIT		10	/* in 2*msl quiet wait after close */

#endif /* !_NETLPX_SMP_H_ */
