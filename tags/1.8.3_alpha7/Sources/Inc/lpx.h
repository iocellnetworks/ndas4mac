/***************************************************************************
 *
 *  lpx.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef _LPX_H_
#define _LPX_H_

#define PF_LPX 		127  /* TEMP - move to socket.h */
#define AF_LPX 		PF_LPX   /* If it ain't broke... */
#define LPX_NODE_LEN    6
	
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
    u_long  long_e;
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
            struct lpx_addr  usipx_addr;
            char             usipx_zero[2];
        } o;
    } u;
}__attribute__ ((packed));

#define slpx_port   u.n.uslpx_port
#define slpx_node   u.n.uslpx_node

#define sipx_addr   u.o.usipx_addr
#define sipx_zero   u.o.usipx_zero
#define sipx_port   sipx_addr.x_port

#endif /* !_LPX_H_ */


