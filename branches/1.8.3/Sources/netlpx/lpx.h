/***************************************************************************
 *
 *  lpx.h
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
 *  @(#)lpx.h
 *
 * $FreeBSD: src/sys/netlpx/lpx.h,v 1.17 2002/03/20 02:39:13 alfred Exp $
 */

#ifndef _NETLPX_LPX_H_
#define _NETLPX_LPX_H_

#define PF_LPX 127  /* TEMP - move to socket.h */
#define AF_LPX PF_LPX   /* If it ain't broke... */
#define LPX_NODE_LEN    6
#define MAX_LPX_ENTRY 128

#ifdef KERNEL

#define CHECK 0

typedef unsigned char   __u8;
typedef signed char     __s8;
typedef unsigned short  __u16;
typedef signed short    __s16;
typedef uint32_t   __u32;
typedef int32_t     __s32;
typedef uint64_t    __u64;
typedef int64_t     __s64;

#define NETISR_LPX  PF_LPX
#define IPPROTO_LPX 4
#define IPPROTO_SMP 5

extern  __u8 LPX_BROADCAST_NODE[];

/*
 * Constants and Structures
 */

/*
 * Protocols
 */
#define LPXPROTO_UNKWN      0   /* Unknown */
#define LPXPROTO_DGRAM      4   /* UDP Style */
#define LPXPROTO_STREAM     5   /* TCP Style  */
#define LPXPROTO_RAW        255 /* Placemarker*/
#define LPXPROTO_MAX        256 /* Placemarker*/

/*
 * Port/Socket numbers: network standard functions
 */

#define LPXPORT_RI      1   /* NS RIP Routing Information */
#define LPXPORT_ECHO        2   /* NS Echo */
#define LPXPORT_RE      3   /* NS Router Error */
#define LPXPORT_NCP     0x0451  /* NW NCP Core Protocol */
#define LPXPORT_SAP     0x0452  /* NW SAP Service Advertising */
#define LPXPORT_RIP     0x0453  /* NW RIP Routing Information */
#define LPXPORT_NETBIOS     0x0455  /* NW NetBIOS */
#define LPXPORT_DIAGS       0x0456  /* NW Diagnostics */
/*
 * Ports < LPXPORT_RESERVED are reserved for privileged
 */
#define LPXPORT_RESERVED    0x3000
/*
 * Ports > LPXPORT_WELLKNOWN are reserved for privileged
 * processes (e.g. root).
 */
#define LPXPORT_WELLKNOWN   0x6000

/* flags passed to Lpx_OUT_outputfl as last parameter */

#define LPX_FORWARDING      0x1 /* most of lpx header exists */
#define LPX_ROUTETOIF       0x10    /* same as SO_DONTROUTE */
#define LPX_ALLOWBROADCAST  SO_BROADCAST    /* can send broadcast packets */

#define LPX_MAXHOPS     15

/* flags passed to get/set socket option */
#define SO_HEADERS_ON_INPUT 1
#define SO_HEADERS_ON_OUTPUT    2
#define SO_DEFAULT_HEADERS  3
#define SO_LAST_HEADER      4
#define SO_LPXIP_ROUTE      5
#define SO_SEQNO        6
#define SO_ALL_PACKETS      7
#define SO_MTU          8
#define SO_LPXTUN_ROUTE     9
#define SO_LPX_CHECKSUM     10


#endif

// if SMALL_LENGTH_DELAYED_ACK was set, We must send ack every 8th packets sent.
#define DEFAULT_WINDOW_SIZE	8

/*
 * LPX addressing
 */
union lpx_host {
    u_char  c_host[6];
    u_short s_host[3];
};

union lpx_net {
    u_char  c_net[4];
    u_short s_net[2];
};

union lpx_net_u {
    union   lpx_net net_e;
    uint32_t      long_e;
};

struct lpx_addr {
    union {
        struct {
            union lpx_net   ux_net;
            union lpx_host  ux_host;
        } i;
        struct {
            u_char  ux_zero[4];
            u_char  ux_node[LPX_NODE_LEN];
        } l;
    } u;
        u_short     x_port;
}__attribute__ ((packed));

#define x_net   u.i.ux_net
#define x_host  u.i.ux_host
#define x_node  u.l.ux_node

/*
 * Socket address
 */

struct sockaddr_lpx {
    u_char      slpx_len;
    u_char      slpx_family;
    union {
        struct {
            u_char      uslpx_zero[4];
            unsigned char   uslpx_node[LPX_NODE_LEN];
            unsigned short  uslpx_port;
            u_char      uslpx_zero2[2];
        } n;
        struct {
            struct lpx_addr usipx_addr;
            char        usipx_zero[2];
        } o;
    } u;
}__attribute__ ((packed));

#define slpx_port   u.n.uslpx_port
#define slpx_node   u.n.uslpx_node

#define sipx_addr   u.o.usipx_addr
#define sipx_zero   u.o.usipx_zero
#define sipx_port   sipx_addr.x_port


#ifdef KERNEL

/*
 * Definitions for LPX Internetwork Packet Exchange Protocol
 */
/* 16 bit align  total  16 bytes */

struct  lpxhdr
{
    union {

#if BYTE_ORDER == LITTLE_ENDIAN

        struct {
            __u8    pktsizeMsb:6;
            __u8    type:2;
#define LPX_TYPE_RAW        0x0
#define LPX_TYPE_DATAGRAM   0x2
#define LPX_TYPE_STREAM     0x3

            __u8    pktsizeLsb;
        }p;

        __u16   pktsize;
#define LPX_TYPE_MASK       (unsigned short)0x00C0

#elif BYTE_ORDER == BIG_ENDIAN

        struct {
            __u8    type:2;
#define LPX_TYPE_RAW        0x0
#define LPX_TYPE_DATAGRAM   0x2
#define LPX_TYPE_STREAM     0x3

            __u8    pktsizeMsb:6;
            __u8    pktsizeLsb;

        }p;
        __u16   pktsize;
#define LPX_TYPE_MASK       (unsigned short)0xC000
#else
        error
#endif

    }pu;

    __u16   dest_port;
    __u16   source_port;

    union {
        struct {
			__u8 reserved_r[9]; 
			__u8 option; 
			
#define LPX_OPTION_PACKETS_CONTINUE_BIT ((__u8)0x80) 
#define LPX_OPTION_SMALL_LENGTH_DELAYED_ACK ((__u8)0x40) 
			
        }r;

        struct {
            __u16   message_id;
            __u16   message_length;
            __u16   fragment_id;
			
			__u16   frament_length;
            __u16   reserved_d1;
            
//#if BYTE_ORDER == LITTLE_ENDIAN
//            __u16   reserved_d1:8;
//            __u16   fragment_last:1;
//            __u16   reserved_d2:7;
//#elif BYTE_ORDER == BIG_ENDIAN
//            __u16   reserved_u1:8;
//            __u16   reserved_u2:7;
//            __u16   fragment_last:1;
//#endif
        }d;
        struct {
            __u16   lsctl;

            /* Lpx Stream Control bits */
#define LSCTL_CONNREQ       0x0001
#define LSCTL_DATA      0x0002
#define LSCTL_DISCONNREQ    0x0004
#define LSCTL_ACKREQ        0x0008
#define LSCTL_ACK       0x1000

            __u16   sequence;
            __u16   ackseq;
			__u8	tag;			// Use This Field since Chip Rev. 1.1
			__u8	reserved_s1;
            __u16   window_size; /* not implemented */
        }s;
    }u;

} __attribute__ ((packed));

#define LPX_PKT_LEN (sizeof(struct lpxhdr))

struct lpx {
    u_short lpx_sum;    /* Checksum */
    u_short lpx_len;    /* Length, in bytes, including header */
    u_char  lpx_tc;     /* Transport Control (i.e. hop count) */
    u_char  lpx_pt;     /* Packet Type (i.e. level 2 protocol) */
    struct lpx_addr lpx_dna;    /* Destination Network Address */
    struct lpx_addr lpx_sna;    /* Source Network Address */
};

#define satolpx_addr(sa) (((struct sockaddr_lpx *)&(sa))->sipx_addr)

#define lpx_hosteqnh(s,t) ((s).c_host[0] == (t).c_host[0] && \
						   (s).c_host[1] == (t).c_host[1] && \
						   (s).c_host[2] == (t).c_host[2] && \
						   (s).c_host[3] == (t).c_host[3] && \
						   (s).c_host[4] == (t).c_host[4] && \
						   (s).c_host[5] == (t).c_host[5] )

#define lpx_hosteq(s,t) (lpx_hosteqnh((s).x_host, (t).x_host))

#define lpx_nullhost(x) (((x).x_host.s_host[0] == 0) && \
						 ((x).x_host.s_host[1] == 0) && \
						 ((x).x_host.s_host[2] == 0) )

#include <sys/cdefs.h>

__BEGIN_DECLS
struct  lpx_addr lpx_addr(const char *);
char    *lpx_ntoa(struct lpx_addr);
__END_DECLS

extern struct ifqueue lpxintrq;

#endif

#endif /* !_NETLPX_LPX_H_ */
