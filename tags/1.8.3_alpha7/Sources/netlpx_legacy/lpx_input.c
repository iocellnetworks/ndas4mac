/***************************************************************************
 *
 *  lpx_input.c
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
 *  @(#)lpx_input.c
 *
 * $FreeBSD: src/sys/netlpx/lpx_input.c,v 1.31 2003/03/04 23:19:53 jlemon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/route.h>
//#include <net/netisr.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_pcb.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>
#include <netlpx/lpx_input.h>
#include <netlpx/lpx_usrreq.h>
#include <netlpx/smp.h>
#include <netlpx/smp_usrreq.h>

//#include <netlpx/lpx_outputfl.h>

#include <net/if_arp.h>
#include <net/ethernet.h>
#include <string.h>
#include <net/dlil.h>


int lpxcksum = 0;
SYSCTL_INT(_net_lpx_lpx, OID_AUTO, checksum, CTLFLAG_RW,
       &lpxcksum, 0, "");

static int  lpxprintfs = 0;     /* printing forwarding information */
SYSCTL_INT(_net_lpx_lpx, OID_AUTO, lpxprintfs, CTLFLAG_RW,
       &lpxprintfs, 0, "");

static int  lpxforwarding = 0;
SYSCTL_INT(_net_lpx_lpx, OID_AUTO, lpxforwarding, CTLFLAG_RW,
        &lpxforwarding, 0, "");

static int  lpxnetbios = 0;
SYSCTL_INT(_net_lpx, OID_AUTO, lpxnetbios, CTLFLAG_RW,
       &lpxnetbios, 0, "");

union   lpx_net lpx_zeronet;
union   lpx_host lpx_zerohost;

union   lpx_net lpx_broadnet;
union   lpx_host lpx_broadhost;

struct  lpxstat lpxstat;
struct  sockaddr_lpx lpx_netmask, lpx_hostmask;

struct  lpxpcb lpxpcb;
struct  lpxpcb lpxrawpcb;

struct ifqueue lpxintrq;

long    lpx_pexseq;

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

/*
 * LPX input routine.  Pass to next level.
 */
void Lpx_IN_intr (struct mbuf *m)
{
    register struct lpxhdr *lpxh;
    register struct lpxpcb *lpxp;
	//  struct lpx_ifaddr *ia;
    int len;
    struct ether_header *ethr;
    struct ether_header frame_header;
    
    DEBUG_CALL(4, ("Lpx_IN_intr\n"));
	
    ethr = mtod(m, struct ether_header *);
    bcopy(ethr, &frame_header, ETHER_HDR_LEN);
	
    m_adj(m, sizeof(struct ether_header));
	
    DEBUG_CALL(4, ("%02x%2x %02x%02x %02x%02x <- %02x%02x %02x%02x %02x%02x : %02x%02x\n", ((u_char*)&frame_header)[0] , ((u_char*)&frame_header)[1] , ((u_char*)&frame_header)[2] , ((u_char*)&frame_header)[3] , ((u_char*)&frame_header)[4] , ((u_char*)&frame_header)[5] , ((u_char*)&frame_header)[6] , ((u_char*)&frame_header)[7] , ((u_char*)&frame_header)[8] , ((u_char*)&frame_header)[9] , ((u_char*)&frame_header)[10] , ((u_char*)&frame_header)[11] , ((u_char*)&frame_header)[12] , ((u_char*)&frame_header)[13] ));
	
    /*
     * If no LPX addresses have been set yet but the interfaces
     * are receiving, can't do anything with incoming packets yet.
     */
    if (lpx_ifaddr == NULL)
        goto bad;
	
    lpxstat.lpxs_total++;
	
    if ((m->m_flags & M_EXT || (unsigned long)m->m_len < sizeof(struct lpxhdr)) &&
        (m = m_pullup(m, sizeof(struct lpxhdr))) == 0) {
        lpxstat.lpxs_toosmall++;
        return;
    }
	
    /*
     * Give any raw listeners a crack at the packet
     */
    for (lpxp = lpxrawpcb.lpxp_next; lpxp != &lpxrawpcb;
         lpxp = lpxp->lpxp_next) {
        struct mbuf *m1 = m_copy(m, 0, (int)M_COPYALL);
        if (m1 != NULL)
            Lpx_USER_input(m1, lpxp, &frame_header);
    }
	
    lpxh = mtod(m, struct lpxhdr *);
    len = ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK);
	//    DEBUG_CALL(2, ("len = %x lpxh->pu.pktsize = %x\n", len, lpxh->pu.pktsize));
    
    /*
     * Check that the amount of data in the buffers
     * is as at least much as the LPX header would have us expect.
     * Trim mbufs if longer than we expect.
     * Drop packet if shorter than we expect.
     */
    if (m->m_pkthdr.len < len) {
        DEBUG_CALL(2, ("len = %x (m->m_pkthdr.len = %x\n", len, m->m_pkthdr.len));
        lpxstat.lpxs_tooshort++;
        goto bad;
    }
    DEBUG_CALL(4, ("1 len = %x lpxh->pu.pktsize = %x\n", len, lpxh->pu.pktsize));
    if (m->m_pkthdr.len > len) {
        if (m->m_len == m->m_pkthdr.len) {
            m->m_len = len;
            m->m_pkthdr.len = len;
        } else {
            m_adj(m, len - m->m_pkthdr.len);
        }
    }
	//    DEBUG_CALL(2, ("2 len = %x lpxh->pu.pktsize = %x\n", len, lpxh->pu.pktsize));
#if 0        
    /*
     * Is this a directed broadcast?
     */
	
	if (lpx_hosteqnh(lpx_broadhost,lpx->lpx_dna.x_host)) {
        if ((!lpx_neteq(lpx->lpx_dna, lpx->lpx_sna)) &&
            (!lpx_neteqnn(lpx->lpx_dna.x_net, lpx_broadnet)) &&
            (!lpx_neteqnn(lpx->lpx_sna.x_net, lpx_zeronet)) &&
            (!lpx_neteqnn(lpx->lpx_dna.x_net, lpx_zeronet)) ) {
            /*
             * If it is a broadcast to the net where it was
             * received from, treat it as ours.
             */
            for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
                if((ia->ia_ifa.ifa_ifp == m->m_pkthdr.rcvif) &&
                   lpx_neteq(ia->ia_addr.sipx_addr, 
							 lpx->lpx_dna))
                    goto ours;
			
            /*
             * Look to see if I need to eat this packet.
             * Algorithm is to forward all young packets
             * and prematurely age any packets which will
             * by physically broadcasted.
             * Any very old packets eaten without forwarding
             * would die anyway.
             *
             * Suggestion of Bill Nesheim, Cornell U.
             */
            if (lpx->lpx_tc < LPX_MAXHOPS) {
                lpx_forward(m);
                return;
            }
        }
    }
	
	
ours:
#endif        
		
	{
		struct lpx_addr sna_addr;
		struct lpx_addr	dna_addr;
		
		bzero(&sna_addr, sizeof(sna_addr));
		
		bcopy(frame_header.ether_shost, sna_addr.x_node, LPX_NODE_LEN);
		sna_addr.x_port = lpxh->source_port;
		
		bzero(&dna_addr, sizeof(dna_addr));
		
		bcopy(frame_header.ether_dhost, dna_addr.x_node, LPX_NODE_LEN);
		dna_addr.x_port = lpxh->dest_port;
		
#if 0
		int i;
		for (i=0; i<6; i++)
			DEBUG_CALL(2, ("frame_header.ether_shost[i] = %x ", frame_header.ether_shost[i]));
#endif /* if 0 */
		
		/*
		 * Locate pcb for datagram.
		 */
		lpxp = Lpx_PCB_lookup(&sna_addr, &dna_addr, LPX_WILDCARD);
	}
	//DEBUG_CALL(2, ("3 lpxh = %p, len = %x lpxh->pu.pktsize = %x, lpxh->dest_port = %x, lpxh->source_port = %x\n", lpxh, len, lpxh->pu.pktsize, lpxh->dest_port, lpxh->source_port));
    /*
     * Switch out to protocol's input routine.
     */
    if (lpxp != NULL) {
        lpxstat.lpxs_delivered++;
		
        DEBUG_CALL(2, ("Lpx_IN_intr: lpxp->lpxp_flags %x should not be %x, packet type %x should be %x\n", lpxp->lpxp_flags, LPXP_ALL_PACKETS, lpxh->pu.p.type, LPX_TYPE_STREAM));
        if ((lpxp->lpxp_flags & LPXP_ALL_PACKETS) == 0)
            switch (lpxh->pu.p.type) {
                case LPX_TYPE_STREAM:
                    Smp_input(m, lpxp, (void *)&frame_header);
                    return;
            }
				
				Lpx_USER_input(m, lpxp, &frame_header);
		
    } else {
		//        DEBUG_CALL(2, ("4 len = %x lpxh->pu.pktsize = %x\n", len, lpxh->pu.pktsize));
        DEBUG_CALL(4, ("no ipxp to get it\n"));
        goto bad;
    }
	
    return;
	
bad:
		m_freem(m);
}

void Lpx_IN_ctl( int cmd,
                 struct sockaddr *arg_as_sa, /* XXX should be swapped with dummy */
                 void *dummy)
{
    caddr_t arg = (/* XXX */ caddr_t)arg_as_sa;
    struct lpx_addr *lpx;

    if (cmd < 0 || cmd > PRC_NCMDS)
        return;
    switch (cmd) {
        struct sockaddr_lpx *slpx;

    case PRC_IFDOWN:
    case PRC_HOSTDEAD:
    case PRC_HOSTUNREACH:
        slpx = (struct sockaddr_lpx *)arg;
        if (slpx->slpx_family != AF_LPX)
            return;
        lpx = &slpx->sipx_addr;
        break;

    default:
        if (lpxprintfs)
            DEBUG_CALL(2, ("lpx_ctlinput: cmd %d.\n", cmd));
        break;
    }
}

/*
 * Forward a packet. If some error occurs drop the packet. LPX don't
 * have a way to return errors to the sender.
 */
#if 0

static struct route lpx_droute;
static struct route lpx_sroute;

static void
lpx_forward(m)
struct mbuf *m;
{
    register struct lpx *lpx = mtod(m, struct lpx *);
    register int error;
    struct mbuf *mcopy = NULL;
    int agedelta = 1;
    int flags = LPX_FORWARDING;
    int ok_there = 0;
    int ok_back = 0;

    if (lpxforwarding == 0) {
        /* can't tell difference between net and host */
        lpxstat.lpxs_cantforward++;
        m_freem(m);
        goto cleanup;
    }
    lpx->lpx_tc++;
    if (lpx->lpx_tc > LPX_MAXHOPS) {
        lpxstat.lpxs_cantforward++;
        m_freem(m);
        goto cleanup;
    }

    if ((ok_there = lpx_do_route(&lpx->lpx_dna,&lpx_droute)) == 0) {
        lpxstat.lpxs_noroute++;
        m_freem(m);
        goto cleanup;
    }
    /*
     * Here we think about  forwarding  broadcast packets,
     * so we try to insure that it doesn't go back out
     * on the interface it came in on.  Also, if we
     * are going to physically broadcast this, let us
     * age the packet so we can eat it safely the second time around.
     */
    if (lpx->lpx_dna.x_host.c_host[0] & 0x1) {
        struct lpx_ifaddr *ia = Lpx_CTL_iaonnetof(&lpx->lpx_dna);
        struct ifnet *ifp;
        if (ia != NULL) {
            /* I'm gonna hafta eat this packet */
            agedelta += LPX_MAXHOPS - lpx->lpx_tc;
            lpx->lpx_tc = LPX_MAXHOPS;
        }
        if ((ok_back = lpx_do_route(&lpx->lpx_sna,&lpx_sroute)) == 0) {
            /* error = ENETUNREACH; He'll never get it! */
            lpxstat.lpxs_noroute++;
            m_freem(m);
            goto cleanup;
        }
        if (lpx_droute.ro_rt &&
            (ifp = lpx_droute.ro_rt->rt_ifp) &&
            lpx_sroute.ro_rt &&
            (ifp != lpx_sroute.ro_rt->rt_ifp)) {
            flags |= LPX_ALLOWBROADCAST;
        } else {
            lpxstat.lpxs_noroute++;
            m_freem(m);
            goto cleanup;
        }
    }
    /*
     * We don't need to recompute checksum because lpx_tc field
     * is ignored by checksum calculation routine, however
     * it may be desirable to reset checksum if lpxcksum == 0
     */
#if 0
    if (!lpxcksum)
        lpx->lpx_sum = 0xffff;
#endif

#if CHECK == 1
        error = Lpx_OUT_outputfl(m, &lpx_droute, flags);
#else
        error = 0;
#endif        
        if (error == 0) {
        lpxstat.lpxs_forward++;

        if (lpxprintfs) {
            DEBUG_CALL(2, ("forward: "));
            Lpx_CTL_printhost(&lpx->lpx_sna);
            DEBUG_CALL(2, (" to "));
            Lpx_CTL_printhost(&lpx->lpx_dna);
            DEBUG_CALL(2, (" hops %d\n", lpx->lpx_tc));
        }
    } else if (mcopy != NULL) {
        lpx = mtod(mcopy, struct lpx *);
        switch (error) {

        case ENETUNREACH:
        case EHOSTDOWN:
        case EHOSTUNREACH:
        case ENETDOWN:
        case EPERM:
            lpxstat.lpxs_noroute++;
            break;

        case EMSGSIZE:
            lpxstat.lpxs_mtutoosmall++;
            break;

        case ENOBUFS:
            lpxstat.lpxs_odropped++;
            break;
        }
        mcopy = NULL;
        m_freem(m);
    }
cleanup:
    if (ok_there)
        lpx_undo_route(&lpx_droute);
    if (ok_back)
        lpx_undo_route(&lpx_sroute);
    if (mcopy != NULL)
        m_freem(mcopy);
}

static int
lpx_do_route(src, ro)
struct lpx_addr *src;
struct route *ro;
{
    struct sockaddr_lpx *dst;

    bzero((caddr_t)ro, sizeof(*ro));
    dst = (struct sockaddr_lpx *)&ro->ro_dst;

    dst->slpx_len = sizeof(*dst);
    dst->slpx_family = AF_LPX;
    dst->sipx_addr = *src;
    dst->sipx_addr.x_port = 0;
    rtalloc(ro);
    if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL) {
        return (0);
    }
    ro->ro_rt->rt_use++;
    return (1);
}

static void
lpx_undo_route(ro)
register struct route *ro;
{
    if (ro->ro_rt != NULL) {
        RTFREE(ro->ro_rt);
    }
}
#endif

