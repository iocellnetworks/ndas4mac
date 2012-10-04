/***************************************************************************
 *
 *  lpx_outputfl.c
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
 *  @(#)lpx_outputfl.c
 *
 * $FreeBSD: src/sys/netlpx/lpx_outputfl.c,v 1.18 2003/02/19 05:47:36 imp Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>
#include <netlpx/lpx_outputfl_int.h>
#include <netlpx/lpx_outputfl.h>
#include <netlpx/lpx_usrreq.h>

#include <netlpx/lpx_pcb.h>
#include <netlpx/lpx_cksum.h>
#include <net/dlil.h>

#if 0
static int lpx_copy_output = 0;
#endif /* if 0 */

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

int Lpx_OUT_outputfl( struct mbuf *m0,
                      struct route *ro,
                      int flags,
                      struct lpxpcb *lpxp)
{
    register struct lpxhdr *lpxh = mtod(m0, struct lpxhdr *);
    register struct ifnet *ifp = NULL;
    int error = 0;
    struct sockaddr_lpx *dst;
    struct route lpxroute;

    DEBUG_CALL(4, ("Lpx_OUT_outputfl\n"));

    if(lpxp == NULL) {
        DEBUG_CALL(1, ("lpxp is NULL\n"));
        return -1;
    }
    /*
     * Route packet.
     */

    if (ro == NULL) {
        ro = &lpxroute;
        bzero((caddr_t)ro, sizeof(*ro));
    }
    dst = (struct sockaddr_lpx *)&ro->ro_dst;
    if (ro->ro_rt == NULL) {
        dst->slpx_family = AF_LPX;
        dst->slpx_len = sizeof(*dst);
        dst->sipx_addr = lpxp->lpxp_faddr;
        dst->sipx_addr.x_port = 0;
        /*
         * If routing to interface only,
         * short circuit routing lookup.
         */
        if (flags & LPX_ROUTETOIF) {
			/*
            struct lpx_ifaddr *ia = Lpx_CTL_iaonnetof(&((*lpxp).lpxp_faddr));
            if (ia == NULL) {
                DEBUG_CALL(0, ("ia is NULL\n"));
                lpxstat.lpxs_noroute++;
                error = ENETUNREACH;
                goto bad;
            }
			*/
			struct lpx_ifaddr *ia = NULL;
			struct sockaddr_lpx laddr = { 0 };
			
			laddr.slpx_family = AF_LPX;
			laddr.slpx_len = sizeof(struct sockaddr_lpx);
			laddr.sipx_addr = lpxp->lpxp_laddr; 
			laddr.slpx_port = 0;
			
			DEBUG_CALL(4, ("0x%x:0x%x:0x%x:0x%x:0x%x:0x%x[%d]\n",
						   laddr.slpx_node[0], laddr.slpx_node[1], laddr.slpx_node[2],
						   laddr.slpx_node[3], laddr.slpx_node[4], laddr.slpx_node[5],
						   laddr.slpx_port
						   ));
			
			
			ia = (struct lpx_ifaddr *)ifa_ifwithaddr((struct sockaddr *)&laddr);
            if (ia == NULL) {
                DEBUG_CALL(0, ("ia is NULL\n"));
                lpxstat.lpxs_noroute++;
                error = ENETUNREACH;
                goto bad;
            }
			
            ifp = ia->ia_ifp;
            goto gotif;
        }
        rtalloc(ro);
    } else if ((ro->ro_rt->rt_flags & RTF_UP) == 0) {
        /*
         * The old route has gone away; try for a new one.
         */
        rtfree(ro->ro_rt);
        ro->ro_rt = NULL;
        rtalloc(ro);
    }
    if (ro->ro_rt == NULL || (ifp = ro->ro_rt->rt_ifp) == NULL) {
        DEBUG_CALL(0, ("ro->ro_rt = %p no route\n", ro->ro_rt));
        lpxstat.lpxs_noroute++;
        error = ENETUNREACH;
        goto bad;
    }
    ro->ro_rt->rt_use++;
    if (ro->ro_rt->rt_flags & (RTF_GATEWAY|RTF_HOST))
        dst = (struct sockaddr_lpx *)ro->ro_rt->rt_gateway;
gotif:
    /*
     * Look for multicast addresses and
     * and verify user is allowed to send
     * such a packet.
     */
    if (dst->sipx_addr.x_host.c_host[0]&1) {
        if ((ifp->if_flags & (IFF_BROADCAST | IFF_LOOPBACK)) == 0) {
            error = EADDRNOTAVAIL;
			
			DEBUG_CALL(2, ("multicast Error 1\n"));
			
            goto bad;
        }
        if ((flags & LPX_ALLOWBROADCAST) == 0) {
            error = EACCES;
			
			DEBUG_CALL(2, ("multicast Error 2\n"));
			
          //  goto bad;
        }
        m0->m_flags |= M_BCAST;
        DEBUG_CALL(2, ("multicast\n"));
    }
    
    if (ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK) <= (short)ifp->if_mtu) {
        int    stat;
        u_long lpx_dl_tag;
        struct sockaddr_lpx dest_addr;

        
        stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_LPX, &lpx_dl_tag);

        if (stat != 0) {
			DEBUG_CALL(0, ("Error when dlil_find_dltag.\n"));
            return stat;
		}
        
        lpxstat.lpxs_localout++;
#if 0
        if (lpx_copy_output) {
            lpx_OUT_watch_output(m0, ifp);
        }
#endif /* if 0 */

//      error = (*ifp->if_output)(ifp, m0);
        dest_addr.slpx_family = AF_LPX;
        dest_addr.slpx_len = sizeof(dest_addr);
        dest_addr.sipx_addr = lpxp->lpxp_faddr;

        error = dlil_output(lpx_dl_tag, m0, NULL, (struct sockaddr *) &dest_addr, 0);
//        DEBUG_CALL(0, ("error = %d\n", error));
        goto done;
    } else {
        lpxstat.lpxs_mtutoosmall++;
        error = EMSGSIZE;
    }
bad:
#if 0
    if (lpx_copy_output) {
        lpx_OUT_watch_output(m0, ifp);
    }
#endif /* if 0 */
    m_freem(m0);
done:
    if (ro == &lpxroute && (flags & LPX_ROUTETOIF) == 0 &&
        ro->ro_rt != NULL) {
        RTFREE(ro->ro_rt);
        ro->ro_rt = NULL;
    }

    return (error);
}

#if 0
void lpx_OUT_watch_output( struct mbuf *m,
                           struct ifnet *ifp)
{
    register struct lpxpcb *lpxp;
    register struct ifaddr *ifa;
    register struct lpx_ifaddr *ia;
    /*
     * Give any raw listeners a crack at the packet
     */
    for (lpxp = lpxrawpcb.lpxp_next; lpxp != &lpxrawpcb;
         lpxp = lpxp->lpxp_next) {
        struct mbuf *m0 = m_copy(m, 0, (int)M_COPYALL);
        if (m0 != NULL) {
            register struct lpx *lpx;

            M_PREPEND(m0, sizeof(*lpx), M_DONTWAIT);
            if (m0 == NULL)
                continue;
            lpx = mtod(m0, struct lpx *);
            lpx->lpx_sna.x_net = lpx_zeronet;
            for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
                if (ifp == ia->ia_ifp)
                    break;
            if (ia == NULL)
                lpx->lpx_sna.x_host = lpx_zerohost;  
            else 
                lpx->lpx_sna.x_host =
                    ia->ia_addr.sipx_addr.x_host;

            if (ifp != NULL && (ifp->if_flags & IFF_POINTOPOINT))
                TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
                if (ifa->ifa_addr->sa_family == AF_LPX) {
                    lpx->lpx_sna = IA_SLPX(ifa)->sipx_addr;
                    break;
                }
                }
            lpx->lpx_len = ntohl(m0->m_pkthdr.len);
            Lpx_USER_input(m0, lpxp);
        }
    }
}
#endif /* if 0 */

#if 0
/*
 * This will broadcast the type 20 (Netbios) packet to all the interfaces
 * that have lpx configured and isn't in the list yet.
 */
int
lpx_output_type20(m)
    struct mbuf *m;
{
    register struct lpx *lpx;
    union lpx_net *nbnet;
    struct lpx_ifaddr *ia, *tia = NULL;
    int error = 0;
    struct mbuf *m1;
    int i;
    struct ifnet *ifp;
    struct sockaddr_lpx dst;

    /*
     * We have to get to the 32 bytes after the lpx header also, so
     * that we can fill in the network address of the receiving
     * interface.
     */
    if ((m->m_flags & M_EXT || (unsigned long)m->m_len < (sizeof(struct lpx) + 32)) &&
        (m = m_pullup(m, sizeof(struct lpx) + 32)) == NULL) {
        lpxstat.lpxs_toosmall++;
        return (0);
    }
    lpx = mtod(m, struct lpx *);
    nbnet = (union lpx_net *)(lpx + 1);

    if (lpx->lpx_tc >= 8)
        goto bad;
    /*
     * Now see if we have already seen this.
     */
    for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
        if(ia->ia_ifa.ifa_ifp == m->m_pkthdr.rcvif) {
            if(tia == NULL)
                tia = ia;

            for (i=0;i<lpx->lpx_tc;i++,nbnet++)
                if(lpx_neteqnn(ia->ia_addr.sipx_addr.x_net,
                            *nbnet))
                    goto bad;
        }
    /*
     * Don't route the packet if the interface where it come from
     * does not have an LPX address.
     */
    if(tia == NULL)
        goto bad;

    /*
     * Add our receiving interface to the list.
     */
        nbnet = (union lpx_net *)(lpx + 1);
    nbnet += lpx->lpx_tc;
    *nbnet = tia->ia_addr.sipx_addr.x_net;

    /*
     * Increment the hop count.
     */
    lpx->lpx_tc++;
    lpxstat.lpxs_forward++;

    /*
     * Send to all directly connected ifaces not in list and
     * not to the one it came from.
     */
    m->m_flags &= ~M_BCAST;
    bzero(&dst, sizeof(dst));
    dst.slpx_family = AF_LPX;
    dst.slpx_len = 12;
    dst.sipx_addr.x_host = lpx_broadhost;

    for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
        if(ia->ia_ifa.ifa_ifp != m->m_pkthdr.rcvif) {
                nbnet = (union lpx_net *)(lpx + 1);
            for (i=0;i<lpx->lpx_tc;i++,nbnet++)
                if(lpx_neteqnn(ia->ia_addr.sipx_addr.x_net,
                            *nbnet))
                    goto skip_this;

            /*
             * Insert the net address of the dest net and
             * calculate the new checksum if needed.
             */
            ifp = ia->ia_ifa.ifa_ifp;
            dst.sipx_addr.x_net = ia->ia_addr.sipx_addr.x_net;
            lpx->lpx_dna.x_net = dst.sipx_addr.x_net;
            if(lpx->lpx_sum != 0xffff)
                lpx->lpx_sum = Lpx_cksum(m, ntohs(lpx->lpx_len));

            m1 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
            if(m1) {
                error = (*ifp->if_output)(ifp, m1);
                /* XXX lpxstat.lpxs_localout++; */
            }
skip_this: ;
        }

bad:
    m_freem(m);
    return (error);
}
#endif /* if 0 */



// End of file 

