/***************************************************************************
 *
 *  lpx_ether_attach.c
 *
 *    Attach lpx into ether layer
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1982, 1989, 1993
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
 */



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
//#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
//#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <sys/socketvar.h>

#include <net/dlil.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_input.h>
#include <netlpx/lpx_ether_attach.h>
#include <netlpx/lpx_ether_attach_int.h>

#include <machine/spl.h>
#include <string.h>



static __u8 etherbroadcastaddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

u_long Lpx_EA_ether_detach(struct ifnet *ifp)
{
    u_long          lpx_dl_tag=0;
    int             stat;

    DEBUG_CALL(4, ("ether_detach_lpx\n"));

    stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_LPX, &lpx_dl_tag);
    if (stat != 0) 
     return stat;

    stat = dlil_detach_protocol(lpx_dl_tag);
    if (stat) {
      DEBUG_CALL(4, ("WARNING: ether_attach_lpx can't attach lpx to interface\n"));
      return stat;
    }
    
    return stat;
}


u_long Lpx_EA_ether_attach(struct ifnet *ifp)
{
    struct dlil_proto_reg_str   reg;
    struct dlil_demux_desc      desc;
    u_long          lpx_dl_tag=0;
    u_short             en_native=ETHERTYPE_LPX;
    int             stat;

    DEBUG_CALL(4, ("ether_attach_lpx\n"));

    stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_LPX, &lpx_dl_tag);
    if (stat == 0)
     return lpx_dl_tag;

    TAILQ_INIT(&reg.demux_desc_head);
    desc.type = DLIL_DESC_RAW;
    desc.variants.bitmask.proto_id_length = 0;
    desc.variants.bitmask.proto_id = 0;
    desc.variants.bitmask.proto_id_mask = 0;
    desc.native_type = (char *) &en_native;
    TAILQ_INSERT_TAIL(&reg.demux_desc_head, &desc, next);
    reg.interface_family = ifp->if_family;
    reg.unit_number      = ifp->if_unit;
    reg.input            = lpx_EA_ether_input;
    reg.pre_output       = lpx_EA_ether_pre_output;
    reg.event            = 0;
    reg.offer            = 0;
    reg.ioctl            = lpx_EA_ether_prmod_ioctl;
    reg.default_proto    = 0;
    reg.protocol_family  = PF_LPX;

    stat = dlil_attach_protocol(&reg, &lpx_dl_tag);
    if (stat) {
    DEBUG_CALL(2, ("WARNING: ether_attach_lpx can't attach lpx to interface\n"));
    return stat;
    }
    return lpx_dl_tag;
}

/* Partial structure of IEEE 802.3 encapsulation header */
struct partial_IEEE_header {
  u_char ucDSAP;
  u_char ucSSAP;
  u_char ucCntl;
  u_char aucOrg[3];
  u_short usType;
};

/***************************************************************************
 *
 *  Work routines (Registered)
 *
 **************************************************************************/

int lpx_EA_ether_input(struct mbuf  *m, char *frame_header,  struct ifnet *ifp,
                            u_long dl_tag, int sync_ok)
{
    register struct ether_header *eh = (struct ether_header *) frame_header;
    u_short ether_type;
    int bNeedIEEEadjust = 0;
    struct partial_IEEE_header *temp = NULL;
    int s;
#if 0
    register struct lpx_ifaddr *ia;
#endif /* if 0 */
    int iIndex=0;


#if ISO || LLC || NETAT
    register struct llc *l;
#endif

    DEBUG_CALL(4, ("lpx_EA_ether_input\n"));

    if ((ifp->if_flags & IFF_UP) == 0) {
     m_freem(m);
     return EJUSTRETURN;
    }

    DEBUG_CALL(4, ("%02x%2x %02x%02x %02x%02x <- %02x%02x %02x%02x %02x%02x : %02x%02x\n", ((u_char*)eh)[0] , ((u_char*)eh)[1] , ((u_char*)eh)[2] , ((u_char*)eh)[3] , ((u_char*)eh)[4] , ((u_char*)eh)[5] , ((u_char*)eh)[6] , ((u_char*)eh)[7] , ((u_char*)eh)[8] , ((u_char*)eh)[9] , ((u_char*)eh)[10] , ((u_char*)eh)[11] , ((u_char*)eh)[12] , ((u_char*)eh)[13] ));
    ifp->if_lastchange = time;

    if (eh->ether_dhost[0] & 1) {
      if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
           sizeof(etherbroadcastaddr)) == 0)
        m->m_flags |= M_BCAST;
      else
        m->m_flags |= M_MCAST;
      }
      if (m->m_flags & (M_BCAST|M_MCAST))
      ifp->if_imcasts++;

      ether_type = ntohs(eh->ether_type);

      DEBUG_CALL(4, ("Ether type = %04x\n", ether_type));
      if (ether_type <= 1500) {
        if (temp = mtod(m, struct partial_IEEE_header *)) {
          ether_type = ntohs(temp->usType);
          bNeedIEEEadjust = 1;
        }
      }

      DEBUG_CALL(4, ("lpx_EA_ether_input: ether_type = %x\n", ether_type));
 
      switch (ether_type) {

      case ETHERTYPE_LPX:
#if 1
        /* Look for peer address table for interface, and register if it's new */
        for (iIndex = 0; iIndex < MAX_LPX_ENTRY; iIndex++) {
          DEBUG_CALL(6,("Lpx_EA_ether_input: g_RtTbl[%d]=%02x:%02x:%02x:%02x:%02x:%02x <=> %02x:%02x:%02x:%02x:%02x:%02x \n",iIndex, g_RtTbl[iIndex].ia_faddr.x_node[0], g_RtTbl[iIndex].ia_faddr.x_node[1], g_RtTbl[iIndex].ia_faddr.x_node[2], g_RtTbl[iIndex].ia_faddr.x_node[3], g_RtTbl[iIndex].ia_faddr.x_node[4],g_RtTbl[iIndex].ia_faddr.x_node[5], eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5]));
          if (bcmp((caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, (caddr_t)eh->ether_shost,  LPX_NODE_LEN) == 0) {
            /* found peer address already in the table -> just update ifp */
            DEBUG_CALL(6,("Lpx_EA_ether_input: found address at g_RtTbl[%d]\n",iIndex));
            g_RtTbl[iIndex].rt_ifp = ifp;
            break;
          } else {
            if (lpx_nullhost(g_RtTbl[iIndex].ia_faddr)) {
              /* Searched to the end of entries, register to entry */
              bcopy((caddr_t)eh->ether_shost, (caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, LPX_NODE_LEN);
              DEBUG_CALL(2,("Lpx_EA_ether_input: New address written to g_RtTbl[%d]\n",iIndex));
              g_RtTbl[iIndex].rt_ifp = ifp;
              break;
            }
          }
        }
        if (iIndex == MAX_LPX_ENTRY) {
          /* Peer address table is full */
          /* Replace with oldest entry : TODO: This is not true */
          if ((g_iRtLastIndex >= MAX_LPX_ENTRY) || (g_iRtLastIndex < 0)) {
            g_iRtLastIndex = 0;
          }
          iIndex = g_iRtLastIndex++;
          bcopy((caddr_t)eh->ether_shost, (caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, LPX_NODE_LEN);
          DEBUG_CALL(2,("Lpx_EA_ether_input: Table is full, written to g_RtTbl[%d], g_iRtLastIndex=%d\n",iIndex,g_iRtLastIndex));
          g_RtTbl[iIndex].rt_ifp = ifp;
        }
#endif /* if 0 */
        break;

      default: 
        return ENOENT;
    }

    
    if(m->m_pkthdr.header) {
       DEBUG_CALL(0, ("m = %p, frame_header = %p, m->m_pkthdr.header = %p\n", m, frame_header, m->m_pkthdr.header));
    }

{
        struct ether_header *ethr;

//        data = mtod(m, char *);
//        DEBUG_CALL(2, ("data = %p\n", data));
        /* Remove header parts exclusive for IEEE802.3 encapsulation */
        if (bNeedIEEEadjust) {
          eh->ether_type = temp->usType;	/* replace type in frame_header */
          m_adj(m, sizeof(struct partial_IEEE_header));
        }

        m = m_prepend_2(m, sizeof(struct ether_header), M_NOWAIT);

        if(m == NULL)
            return 0;
        
        ethr = mtod(m, struct ether_header *);
        bcopy(frame_header, ethr, ETHER_HDR_LEN);
		
		// BROADCAST!!!!
		if(m->m_flags & M_BCAST) {
			// Change Dest Addr from broadcast to Interface.
			bcopy( ((struct arpcom *)ifp)->ac_enaddr, ethr->ether_dhost, ETHER_ADDR_LEN);
			
			DEBUG_CALL(1, ("Change Address. %d:%d:%d\n", ethr->ether_dhost[0], ethr->ether_dhost[1], ethr->ether_dhost[2]));
		}
}

    s = splimp();
    
    if (IF_QFULL(&lpxintrq)) {
      IF_DROP(&lpxintrq);
      m_freem(m);
      splx(s);
      return EJUSTRETURN;
    } else {
      IF_ENQUEUE(&lpxintrq, m);
    }
    
    splx(s);

    s = splnet();
    lpx_EA_intr_call();
    splx(s);

    return 0;
}


int lpx_EA_ether_pre_output(struct ifnet *ifp, struct mbuf **m0, struct sockaddr *dst_netaddr,
                                caddr_t route, char *type, char *edst, u_long dl_tag)
{
    struct sockaddr_lpx *dest_addr;
    
    DEBUG_CALL(4, ("lpx_EA_ether_pre_output\n"));
    
    if(!dst_netaddr)
        return EAFNOSUPPORT;
   
    dest_addr = (struct sockaddr_lpx *)dst_netaddr;
    
    if(dest_addr->slpx_family != AF_LPX)
        return EAFNOSUPPORT;

    *(__u16 *)type = htons(ETHERTYPE_LPX);
    bcopy(dest_addr->slpx_node, edst, LPX_NODE_LEN); 
    
    (*m0)->m_flags |= M_LOOP;

    return 0;
}


int lpx_EA_ether_prmod_ioctl(u_long dl_tag, struct ifnet *ifp, u_long command, caddr_t data)
{
    DEBUG_CALL(2, ("lpx_EA_ether_prmod_ioctl\n"));

    return 0;
}


/***************************************************************************
 *
 *  Internal helper functions
 *
 **************************************************************************/

void lpx_EA_intr_call(void)
{
    int s;
    struct mbuf *m = NULL;
    
    DEBUG_CALL(4, ("lpx_EA_intr_call\n"));

    while(1) {
        s = splimp();
        IF_DEQUEUE(&lpxintrq, m);
        splx(s);
        if(m == NULL)
            return;
            
        Lpx_IN_intr(m);
    }
    
    return;
}


#if 0

#if LLC && CCITT
extern struct ifqueue pkintrq;
#endif


#if BRIDGE
#include <net/bridge.h>
#endif

/* #include "vlan.h" */
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

static u_long lo_dlt = 0;
static ivedonethis = 0;
static u_char   etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#define IFP2AC(IFP) ((struct arpcom *)IFP)




/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */
int
inet_ether_input(m, frame_header, ifp, dl_tag, sync_ok)
    struct mbuf  *m;
    char         *frame_header;
    struct ifnet *ifp;
    u_long       dl_tag;
    int          sync_ok;

{
    register struct ether_header *eh = (struct ether_header *) frame_header;
    register struct ifqueue *inq=0;
    u_short ether_type;
    int s;
    u_int16_t ptype = -1;
    unsigned char buf[18];

#if ISO || LLC || NETAT
    register struct llc *l;
#endif

    if ((ifp->if_flags & IFF_UP) == 0) {
     m_freem(m);
     return EJUSTRETURN;
    }

    ifp->if_lastchange = time;

    if (eh->ether_dhost[0] & 1) {
    if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
         sizeof(etherbroadcastaddr)) == 0)
        m->m_flags |= M_BCAST;
    else
        m->m_flags |= M_MCAST;
    }
    if (m->m_flags & (M_BCAST|M_MCAST))
    ifp->if_imcasts++;

    ether_type = ntohs(eh->ether_type);

#if NVLAN > 0
    if (ether_type == vlan_proto) {
        if (vlan_input(eh, m) < 0)
            ifp->if_data.ifi_noproto++;
        return EJUSTRETURN;
    }
#endif /* NVLAN > 0 */

    switch (ether_type) {

    case ETHERTYPE_IP:
    if (ipflow_fastforward(m))
        return EJUSTRETURN;
    ptype = mtod(m, struct ip *)->ip_p;
    if ((sync_ok == 0) || 
        (ptype != IPPROTO_TCP && ptype != IPPROTO_UDP)) {
        schednetisr(NETISR_IP); 
    }

    inq = &ipintrq;
    break;

    case ETHERTYPE_ARP:
    schednetisr(NETISR_ARP);
    inq = &arpintrq;
    break;

    default: {
    return ENOENT;
    }
    }

    if (inq == 0)
    return ENOENT;

    s = splimp();
    if (IF_QFULL(inq)) {
        IF_DROP(inq);
        m_freem(m);
        splx(s);
        return EJUSTRETURN;
    } else
        IF_ENQUEUE(inq, m);
    splx(s);

    if ((sync_ok) && 
    (ptype == IPPROTO_TCP || ptype == IPPROTO_UDP)) {
        extern void ipintr(void);
        
        s = splnet();
        ipintr();
        splx(s);
    }

    return 0;
}




int
inet_ether_pre_output(ifp, m0, dst_netaddr, route, type, edst, dl_tag )
    struct ifnet    *ifp;
    struct mbuf     **m0;
    struct sockaddr *dst_netaddr;
    caddr_t     route;
    char        *type;
    char            *edst;
    u_long      dl_tag;
{
    struct rtentry  *rt0 = (struct rtentry *) route;
    int s;
    register struct mbuf *m = *m0;
    register struct rtentry *rt;
    register struct ether_header *eh;
    int off, len = m->m_pkthdr.len;
    int hlen;   /* link layer header lenght */
    struct arpcom *ac = IFP2AC(ifp);



    if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) 
    return ENETDOWN;

    rt = rt0;
    if (rt) {
    if ((rt->rt_flags & RTF_UP) == 0) {
        rt0 = rt = rtalloc1(dst_netaddr, 1, 0UL);
        if (rt0)
        rtunref(rt);
        else
        return EHOSTUNREACH;
    }

    if (rt->rt_flags & RTF_GATEWAY) {
        if (rt->rt_gwroute == 0)
        goto lookup;
        if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
        rtfree(rt); rt = rt0;
        lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1,
                          0UL);
        if ((rt = rt->rt_gwroute) == 0)
            return (EHOSTUNREACH);
        }
    }

    
    if (rt->rt_flags & RTF_REJECT)
        if (rt->rt_rmx.rmx_expire == 0 ||
        time_second < rt->rt_rmx.rmx_expire)
        return (rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
    }

    hlen = ETHER_HDR_LEN;

    /*
     * Tell ether_frameout it's ok to loop packet unless negated below.
     */
    m->m_flags |= M_LOOP;

    switch (dst_netaddr->sa_family) {

    case AF_INET:
    if (!arpresolve(ac, rt, m, dst_netaddr, edst, rt0))
        return (EJUSTRETURN);   /* if not yet resolved */
    off = m->m_pkthdr.len - m->m_len;
    *(u_short *)type = htons(ETHERTYPE_IP);
    break;

    case AF_UNSPEC:
    m->m_flags &= ~M_LOOP;
    eh = (struct ether_header *)dst_netaddr->sa_data;
    bcopy(eh->ether_dhost, edst, 6);
    *(u_short *)type = eh->ether_type;
    break;

    default:
    kprintf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
           dst_netaddr->sa_family);

        return EAFNOSUPPORT;
    }

    return (0);
}


int
ether_inet_prmod_ioctl(dl_tag, ifp, command, data)
    u_long       dl_tag;
    struct ifnet *ifp;
    int          command;
    caddr_t      data;
{
    struct ifaddr *ifa = (struct ifaddr *) data;
    struct ifreq *ifr = (struct ifreq *) data;
    struct rslvmulti_req *rsreq = (struct rslvmulti_req *) data;
    int error = 0;
    boolean_t funnel_state;
    struct arpcom *ac = (struct arpcom *) ifp;
    struct sockaddr_dl *sdl;
    struct sockaddr_in *sin;
    u_char *e_addr;


#if 0
    /* No tneeded at soo_ioctlis already funnelled */
    funnel_state = thread_funnel_set(network_flock,TRUE);
#endif

    switch (command) {
    case SIOCRSLVMULTI: {
    switch(rsreq->sa->sa_family) {

    case AF_INET:
        sin = (struct sockaddr_in *)rsreq->sa;
        if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
            return EADDRNOTAVAIL;
        MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
               M_WAITOK);
        sdl->sdl_len = sizeof *sdl;
        sdl->sdl_family = AF_LINK;
        sdl->sdl_index = ifp->if_index;
        sdl->sdl_type = IFT_ETHER;
        sdl->sdl_nlen = 0;
        sdl->sdl_alen = ETHER_ADDR_LEN;
        sdl->sdl_slen = 0;
        e_addr = LLADDR(sdl);
        ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
        *rsreq->llsa = (struct sockaddr *)sdl;
        return EJUSTRETURN;

    default:
        /* 
         * Well, the text isn't quite right, but it's the name
         * that counts...
         */
        return EAFNOSUPPORT;
    }

    }
    case SIOCSIFADDR:
     if ((ifp->if_flags & IFF_RUNNING) == 0) {
          ifp->if_flags |= IFF_UP;
          dlil_ioctl(0, ifp, SIOCSIFFLAGS, (caddr_t) 0);
     }

     switch (ifa->ifa_addr->sa_family) {

     case AF_INET:

        if (ifp->if_init)
          ifp->if_init(ifp->if_softc);    /* before arpwhohas */

        //
        // See if another station has *our* IP address.
        // i.e.: There is an address conflict! If a
        // conflict exists, a message is sent to the
        // console.
        //
        if (IA_SIN(ifa)->sin_addr.s_addr != 0)
        {
         /* don't bother for 0.0.0.0 */
         ac->ac_ipaddr = IA_SIN(ifa)->sin_addr;
         arpwhohas(ac, &IA_SIN(ifa)->sin_addr);
        }

        arp_ifinit(IFP2AC(ifp), ifa);

        /*
         * Register new IP and MAC addresses with the kernel debugger
         * for the en0 interface.
         */
        if (ifp->if_unit == 0)
          kdp_set_ip_and_mac_addresses(&(IA_SIN(ifa)->sin_addr), &(IFP2AC(ifp)->ac_enaddr));

        break;

    default:
        break;
    }

    break;

    case SIOCGIFADDR:
    {
    struct sockaddr *sa;

    sa = (struct sockaddr *) & ifr->ifr_data;
    bcopy(IFP2AC(ifp)->ac_enaddr,
          (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
    }
    break;

    case SIOCSIFMTU:
    /*
     * IOKit IONetworkFamily will set the right MTU according to the driver
     */

     return (0);

    default:
     return EOPNOTSUPP;
    }

    //(void) thread_funnel_set(network_flock, FALSE);

    return (error);
}




int  ether_detach_inet(struct ifnet *ifp)
{
    u_long      ip_dl_tag = 0;
    int         stat;

    stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_INET, &ip_dl_tag);
    if (stat == 0) {
        stat = dlil_detach_protocol(ip_dl_tag);
        if (stat) {
            DEBUG_CALL(2, ("WARNING: ether_detach_inet can't detach ip from interface\n"));
        }
    }
    return stat;
}


#endif
