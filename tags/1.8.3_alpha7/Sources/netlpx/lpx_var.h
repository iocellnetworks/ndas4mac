#ifndef _NETLPX_LPX_VAR_H_
#define _NETLPX_LPX_VAR_H_

/*
 * LPX Kernel Structures and Variables
 */
struct  lpxstat {
    u_long  lpxs_total;     /* total packets received */
    u_long  lpxs_badsum;        /* checksum bad */
    u_long  lpxs_tooshort;      /* packet too short */
    u_long  lpxs_toosmall;      /* not enough data */
    u_long  lpxs_forward;       /* packets forwarded */
    u_long  lpxs_cantforward;   /* packets rcvd for unreachable dest */
    u_long  lpxs_delivered;     /* datagrams delivered to upper level*/
    u_long  lpxs_localout;      /* total lpx packets generated here */
    u_long  lpxs_odropped;      /* lost packets due to nobufs, etc. */
    u_long  lpxs_noroute;       /* packets discarded due to no route */
    u_long  lpxs_mtutoosmall;   /* the interface mtu is too small */
};

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_lpx);
SYSCTL_DECL(_net_lpx_datagram);
#endif

extern int lpxcksum;
extern long lpx_pexseq;
extern struct lpxstat lpxstat;

extern struct pr_usrreqs lpx_datagram_usrreqs;
extern struct pr_usrreqs lpx_stream_usrreqs;

extern struct sockaddr_lpx lpx_netmask;
extern struct sockaddr_lpx lpx_hostmask;

extern union lpx_net lpx_zeronet;
extern union lpx_host lpx_zerohost;
extern union lpx_net lpx_broadnet;
extern union lpx_host lpx_broadhost;

/* Tag for M buffer. */
extern mbuf_tag_id_t	lpxTagId;

enum {
	LPX_TAG_TYPE_NONE				= 0,
	LPX_TAG_TYPE_ETHERNET_ADDR		= 1
};

#endif /* !_NETLPX_LPX_VAR_H_ */
