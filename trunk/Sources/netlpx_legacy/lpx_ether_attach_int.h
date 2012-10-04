#ifndef _LPX_ETHER_ATTACH_INT_H_
#define _LPX_ETHER_ATTACH_INT_H_

/***************************************************************************
 *
 *  lpx_ether_attach_int.h
 *
 *    Attach lpx into ether layer
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

int  lpx_EA_ether_input(struct mbuf  *m,
                        char *frame_header,
                        struct ifnet *ifp,
                        u_long dl_tag,
                        int sync_ok);

int  lpx_EA_ether_pre_output(struct ifnet *ifp,
                             struct mbuf **m0,
                             struct sockaddr *dst_netaddr,
                             caddr_t route,
                             char *type,
                             char *edst,
                             u_long dl_tag);

int  lpx_EA_ether_prmod_ioctl(u_long dl_tag,
                              struct ifnet *ifp,
                              u_long command,
                              caddr_t data);

void lpx_EA_intr_call(void);

#endif	/* !_LPX_ETHER_ATTACH_INT_H_ */
