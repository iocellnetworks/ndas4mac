/***************************************************************************
 *
 *  lpx_usrreq.c
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
 *  @(#)lpx_usrreq.c
 *
 * $FreeBSD: src/sys/netlpx/lpx_usrreq.c,v 1.36 2003/02/19 05:47:36 imp Exp $
 */

//#include "opt_lpx.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
//#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
//#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_pcb.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>
#include <netlpx/lpx_outputfl.h>
#include <netlpx/lpx_usrreq.h>
#include <netlpx/lpx_usrreq_int.h>

#ifdef LPXIP	/* Not supported */
#include <netlpx/lpx_ip.h>
#endif /* LPXIP */

#include <net/ethernet.h>
#include <machine/spl.h>
#include <string.h>

/*
 * LPX protocol implementation.
 */

static int lpxsendspace = LPXSNDQ;
SYSCTL_INT(_net_lpx_lpx, OID_AUTO, lpxsendspace, CTLFLAG_RW,
            &lpxsendspace, 0, "");
static int lpxrecvspace = LPXRCVQ;
SYSCTL_INT(_net_lpx_lpx, OID_AUTO, lpxrecvspace, CTLFLAG_RW,
            &lpxrecvspace, 0, "");

#if 1
struct  pr_usrreqs lpx_usrreqs = {
    lpx_USER_abort, pru_accept_notsupp, lpx_USER_attach, lpx_USER_bind,
    lpx_USER_connect, pru_connect2_notsupp, Lpx_CTL_control, lpx_USER_detach,
    lpx_USER_disconnect, pru_listen_notsupp, Lpx_USER_peeraddr, pru_rcvd_notsupp,
    pru_rcvoob_notsupp, lpx_USER_send, pru_sense_null, lpx_USER_shutdown,
    Lpx_USER_sockaddr, sosend, soreceive, sopoll
};

struct  pr_usrreqs rlpx_usrreqs = {
    lpx_USER_abort, pru_accept_notsupp, lpx_USER_r_attach, lpx_USER_bind,
    lpx_USER_connect, pru_connect2_notsupp, Lpx_CTL_control, lpx_USER_detach,
    lpx_USER_disconnect, pru_listen_notsupp, Lpx_USER_peeraddr, pru_rcvd_notsupp,
    pru_rcvoob_notsupp, lpx_USER_send, pru_sense_null, lpx_USER_shutdown,
    Lpx_USER_sockaddr, sosend, soreceive, sopoll
};
#else /* if 1 */
struct  pr_usrreqs lpx_usrreqs = {
    lpx_USER_abort, pru_accept_notsupp, lpx_USER_attach, NULL,
    lpx_USER_connect, pru_connect2_notsupp, Lpx_CTL_control, lpx_USER_detach,
    lpx_USER_disconnect, pru_listen_notsupp, Lpx_USER_peeraddr, pru_rcvd_notsupp,
    pru_rcvoob_notsupp, lpx_USER_send, pru_sense_null, lpx_USER_shutdown,
    Lpx_USER_sockaddr, sosend, soreceive, sopoll
};

struct  pr_usrreqs rlpx_usrreqs = {
    lpx_USER_abort, pru_accept_notsupp, lpx_USER_r_attach,bind,
    lpx_USER_connect, pru_connect2_notsupp, Lpx_CTL_control, lpx_USER_detach,
    lpx_USER_disconnect, pru_listen_notsupp, Lpx_USER_peeraddr, pru_rcvd_notsupp,
    pru_rcvoob_notsupp, lpx_USER_send, pru_sense_null, lpx_USER_shutdown,
    Lpx_USER_sockaddr, sosend, soreceive, sopoll
};
#endif /* if 1 else */

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

/*
 *  This may also be called for raw listeners.
 */
void Lpx_USER_input( struct mbuf *m,
                     register struct lpxpcb *lpxp,
                     struct ether_header *frame_header)
{
    register struct lpxhdr *lpxh = mtod(m, struct lpxhdr *);
//  struct ifnet *ifp = m->m_pkthdr.rcvif;
    struct sockaddr_lpx lpx_lpx;
    struct ether_header *eh;
//  struct lpx_addr sna_addr;

#if 0
    eh = (struct ether_header *) m->m_pktdat;
#else /* if 0 */
    eh = frame_header;
#endif /* if 0 else */

    DEBUG_CALL(4, ("Lpx_USER_input\n"));

#if 0
    {
      int iter=0;
      DEBUG_CALL(6,("Lpx_USER_input:"));
      for (iter=0; iter<16; iter++) {
        DEBUG_CALL(6,(" %02x",((char*)eh)[iter]));
      }
      DEBUG_CALL(6,("\n"));
    }
#endif /* if 0 */

    if (lpxp == NULL)
      panic("No lpxpcb");
    /*
     * Construct sockaddr format source address.
     * Stuff source address and datagram in user buffer.
     */

    bzero(&lpx_lpx, sizeof(lpx_lpx));
    lpx_lpx.slpx_len = sizeof(lpx_lpx);
    lpx_lpx.slpx_family = AF_LPX;
    bcopy(eh->ether_shost, lpx_lpx.slpx_node, LPX_NODE_LEN);
    lpx_lpx.slpx_port = lpxh->source_port;
    DEBUG_CALL(6,("Lpx_USER_input: %02x:%02x:%02x:%02x:%02x:%02x:%d\n", eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5], lpxh->source_port));
    DEBUG_CALL(6,("Lpx_USER_input: %02x:%02x:%02x:%02x:%02x:%02x:%d\n", lpx_lpx.slpx_node[0], lpx_lpx.slpx_node[1], lpx_lpx.slpx_node[2], lpx_lpx.slpx_node[3], lpx_lpx.slpx_node[4], lpx_lpx.slpx_node[5], lpx_lpx.slpx_port));

#if 0        
    if (lpx_neteqnn(lpx->lpx_sna.x_net, lpx_zeronet) && ifp != NULL) {
    register struct ifaddr *ifa;

        for (ifa = TAILQ_FIRST(&ifp->if_addrhead); ifa != NULL; 
            ifa = TAILQ_NEXT(ifa, ifa_link)) {
            if (ifa->ifa_addr->sa_family == AF_LPX) {
                lpx_lpx.sipx_addr.x_net =
                IA_SLPX(ifa)->sipx_addr.x_net;
                break;
            }
        }
    }
#endif

//  lpxp->lpxp_rpt = lpx->lpx_pt;
    if (!(lpxp->lpxp_flags & LPXP_RAWIN) ) {
        m_adj(m, sizeof(struct lpxhdr));
//  m->m_len -= sizeof(struct lpxhdr);
//  m->m_pkthdr.len -= sizeof(struct lpxhdr);
//  m->m_data += sizeof(struct lpxhdr);
    }
    if (sbappendaddr(&lpxp->lpxp_socket->so_rcv, (struct sockaddr *)&lpx_lpx,
                     m, (struct mbuf *)NULL) == 0) {
        DEBUG_CALL(2, ("!!!Lpx_USER_input: sbappendaddr error: socket=@(%lx), rcv@(%lx)!",lpxp->lpxp_socket, &(lpxp->lpxp_socket->so_rcv)));
        goto bad;
    }
//    m->m_len += sizeof(struct sockaddr);
//    m->m_pkthdr.len += sizeof(struct sockaddr);
//    m->m_data -= sizeof(struct sockaddr);

    DEBUG_CALL(4, ("Lpx_USER_input: socket sbflag=%x\n",lpxp->lpxp_socket->so_rcv.sb_flags));
#if 1
    sorwakeup(lpxp->lpxp_socket);
    DEBUG_CALL(4, ("Lpx_USER_input: wokeup socket @(%lx)\n",lpxp->lpxp_socket));
#else /* if 0 */
    if (lpxp->lpxp_socket->so_rcv.sb_flags & (SB_WAIT|SB_SEL|SB_ASYNC|SB_UPCALL)) {
      DEBUG_CALL(4, ("Lpx_USER_input: sowokeup socket @(%lx)\n",lpxp->lpxp_socket));
	  sowakeup(lpxp->lpxp_socket, &lpxp->lpxp_socket->so_rcv); 
	}
#endif /* if 0 else */
    return;
bad:
    m_freem(m);
}


int Lpx_USER_ctloutput( struct socket *so,
                        struct sockopt *sopt )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
    int mask, error, optval;
    short soptval;
    struct lpx ioptval;

    error = 0;
    if (lpxp == NULL)
        return (EINVAL);

    switch (sopt->sopt_dir) {
    case SOPT_GET:
        switch (sopt->sopt_name) {
        case SO_ALL_PACKETS:
            mask = LPXP_ALL_PACKETS;
            goto get_flags;

        case SO_HEADERS_ON_INPUT:
            mask = LPXP_RAWIN;
            goto get_flags;

        case SO_LPX_CHECKSUM:
            mask = LPXP_CHECKSUM;
            goto get_flags;
            
        case SO_HEADERS_ON_OUTPUT:
            mask = LPXP_RAWOUT;
        get_flags:
            soptval = lpxp->lpxp_flags & mask;
            error = sooptcopyout(sopt, &soptval, sizeof soptval);
            break;

        case SO_DEFAULT_HEADERS:
            ioptval.lpx_len = 0;
            ioptval.lpx_sum = 0;
            ioptval.lpx_tc = 0;
            ioptval.lpx_pt = lpxp->lpxp_dpt;
            ioptval.lpx_dna = lpxp->lpxp_faddr;
            ioptval.lpx_sna = lpxp->lpxp_laddr;
            error = sooptcopyout(sopt, &soptval, sizeof soptval);
            break;

        case SO_SEQNO:
            error = sooptcopyout(sopt, &lpx_pexseq, 
                         sizeof lpx_pexseq);
            lpx_pexseq++;
            break;

        default:
            error = EINVAL;
        }
        break;

    case SOPT_SET:
        switch (sopt->sopt_name) {
        case SO_ALL_PACKETS:
            mask = LPXP_ALL_PACKETS;
            goto set_head;

        case SO_HEADERS_ON_INPUT:
            mask = LPXP_RAWIN;
            goto set_head;

        case SO_LPX_CHECKSUM:
            mask = LPXP_CHECKSUM;

        case SO_HEADERS_ON_OUTPUT:
            mask = LPXP_RAWOUT;
        set_head:
            error = sooptcopyin(sopt, &optval, sizeof optval,
                        sizeof optval);
            if (error)
                break;
            if (optval)
                lpxp->lpxp_flags |= mask;
            else
                lpxp->lpxp_flags &= ~mask;
            break;

        case SO_DEFAULT_HEADERS:
            error = sooptcopyin(sopt, &ioptval, sizeof ioptval,
                        sizeof ioptval);
            if (error)
                break;
            lpxp->lpxp_dpt = ioptval.lpx_pt;
            break;
#ifdef LPXIP	/* Not supported */
        case SO_LPXIP_ROUTE:
            error = lpxip_route(so, sopt);
            break;
#endif /* LPXIP */
#ifdef IPTUNNEL
#if 0
        case SO_LPXTUNNEL_ROUTE:
            error = lpxtun_route(so, sopt);
            break;
#endif
#endif
        default:
            error = EINVAL;
        }
        break;
    }
    return (error);
}


int Lpx_USER_peeraddr( struct socket *so,
                       struct sockaddr **nam )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);

    Lpx_PCB_setpeeraddr(lpxp, nam); /* XXX what if alloc fails? */
    return (0);
}


int Lpx_USER_sockaddr( struct socket *so,
                       struct sockaddr **nam )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);

    Lpx_PCB_setsockaddr(lpxp, nam); /* XXX what if alloc fails? */
    return (0);
}


/***************************************************************************
 *
 *  Internal functions
 *
 **************************************************************************/

static int lpx_USER_abort( struct socket *so )
{
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    s = splnet();
    Lpx_PCB_detach(lpxp);
    splx(s);
        
        sofree(so);

    soisdisconnected(so);
    return (0);
}


static int lpx_USER_attach( struct socket *so,
                            int proto,
                            struct proc *td )
{
    int error;
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    if (lpxp != NULL)
        return (EINVAL);
    s = splnet();
    error = Lpx_PCB_alloc(so, &lpxpcb, td);
    splx(s);
    if (error == 0) {
        int lpxsends, lpxrecvs;

        lpxsends = 256 * 1024;
        lpxrecvs = 256 * 1024;

        while(lpxsends > 0 && lpxrecvs > 0) {
            error = soreserve(so, lpxsends, lpxrecvs);
            if(error == 0)
                break;
            lpxsends -= (1024*2);
            lpxrecvs -= (1024*2);
        }

        DEBUG_CALL(4, ("lpxsends = %d\n", lpxsends/1024));
    
        //        error = soreserve(so, lpxsendspace, lpxrecvspace);
    }
    
    return (error);
}


static int lpx_USER_bind( struct socket *so,
                          struct sockaddr *nam,
                          struct proc *td )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;

    if(slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    return (Lpx_PCB_bind(lpxp, nam, td));
}


static int lpx_USER_connect( struct socket *so,
                             struct sockaddr *nam,
                             struct proc *td )
{
    int error;
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;

    if(slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    if (!lpx_nullhost(lpxp->lpxp_faddr))
        return (EISCONN);
    s = splnet();
    error = Lpx_PCB_connect(lpxp, nam, td);
    splx(s);
    if (error == 0)
        soisconnected(so);
    return (error);
}


static int lpx_USER_detach( struct socket *so )
{
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    if (lpxp == NULL)
        return (ENOTCONN);
    s = splnet();
    Lpx_PCB_detach(lpxp);
    splx(s);
    return (0);
}

static int lpx_USER_disconnect( struct socket *so )
{
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    if (lpx_nullhost(lpxp->lpxp_faddr))
        return (ENOTCONN);
    s = splnet();
    Lpx_PCB_disconnect(lpxp);
    splx(s);
    soisdisconnected(so);
    return (0);
}

static int lpx_USER_send( struct socket *so,
                          int flags,
                          struct mbuf *m,
                          struct sockaddr *nam,
                          struct mbuf *control,
                          struct proc *td )
{
    int error;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct lpx_addr laddr;
    int s = 0;

    DEBUG_CALL(4, ("lpx_USER_send\n"));

    if (nam != NULL) {
        laddr = lpxp->lpxp_laddr;
        if (!lpx_nullhost(lpxp->lpxp_faddr)) {
            error = EISCONN;
            goto send_release;
        }
        /*
         * Must block input while temporarily connected.
         */
        s = splnet();
        error = Lpx_PCB_connect(lpxp, nam, td);
        if (error) {
            splx(s);
            DEBUG_CALL(0,("lpx_USER_send: error(%d) in Lpx_PCB_connect: so=%lx, lpxp=%lx, nam=%lx\n",error,so, lpxp, nam));
            goto send_release;
        }
    } else {
        if (lpx_nullhost(lpxp->lpxp_faddr)) {
            error = ENOTCONN;
            DEBUG_CALL(0,("lpx_USER_send: error(%d): so=%lx, lpxp=%lx\n",error,so, lpxp ));
            goto send_release;
        }
    }
    error = lpx_USER_output(lpxp, m);
    if (error) {
      DEBUG_CALL(0,("lpx_USER_send: error(%d) in lpx_USER_output: so=%lx, lpxp=%lx, m=%lx\n",error, so, lpxp, m));
//      debug_level=4;
    }
    m = NULL;
    if (nam != NULL) {
        Lpx_PCB_disconnect(lpxp);
        splx(s);
        lpxp->lpxp_laddr = laddr;
    }

send_release:
    if (m != NULL)
      m_freem(m);

    return (error);
}


static int lpx_USER_shutdown( struct socket *so )
{
    socantsendmore(so);
    return (0);
}


static int lpx_USER_r_attach( struct socket *so,
                            int proto,
                            struct proc *td )
{
    int error = 0;
    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);


        if (td != NULL && (error = suser(td->p_ucred, &td->p_acflag)) != 0)
        return (error);

    s = splnet();
    error = Lpx_PCB_alloc(so, &lpxrawpcb, td);
    splx(s);
    if (error)
        return (error);
    error = soreserve(so, lpxsendspace, lpxrecvspace);
    if (error)
        return (error);
    lpxp = sotolpxpcb(so);
    lpxp->lpxp_faddr.x_host = lpx_broadhost;
    lpxp->lpxp_flags = LPXP_RAWIN | LPXP_RAWOUT;
    return (error);
}


static int lpx_USER_output( struct lpxpcb *lpxp,
                            struct mbuf *m0 )
{
    register struct lpxhdr *lpxh;
    register struct socket *so;
    register int len = 0;
    register struct route *ro;
    struct mbuf *m;
    struct mbuf *mprev = NULL;

    DEBUG_CALL(4, ("lpx_USER_output\n"));

    /*
     * Calculate data length.
     */
    for (m = m0; m != NULL; m = m->m_next) {
        mprev = m;
        len += m->m_len;
    }
    /*
     * Make sure packet is actually of even length.
     */
    
    if (len & 1) {
        m = mprev;
        if ((m->m_flags & M_EXT) == 0 &&
            (m->m_len + m->m_data < &m->m_dat[MLEN])) {
            mtod(m, char*)[m->m_len++] = 0;
        } else {
            struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);
            if (m1 == NULL) {
                m_freem(m0);
                return (ENOBUFS);
            }
            m1->m_len = 1;
            * mtod(m1, char *) = 0;
            m->m_next = m1;
        }
        m0->m_pkthdr.len++;
    }

    /*
     * Fill in mbuf with extended LPX header
     * and addresses and length put into network format.
     */
    m = m0;
    if (lpxp->lpxp_flags & LPXP_RAWOUT) {
        lpxh = mtod(m, struct lpxhdr *);
    } else {
        M_PREPEND(m, sizeof(struct lpxhdr), M_DONTWAIT);
        if (m == NULL) {
            DEBUG_CALL(0, ("m is NULL\n"));
            return (ENOBUFS);
        }

        lpxh = mtod(m, struct lpxhdr *);

        bzero(lpxh, sizeof(*lpxh));

        lpxh->source_port = lpxp->lpxp_laddr.x_port;
        lpxh->dest_port = lpxp->lpxp_faddr.x_port;

        lpxh->u.d.message_id = lpxp->lpxp_messageid++;
        lpxh->u.d.message_length = htons(len);
        lpxh->u.d.fragment_id = 0; //htons(fragment_id);
//        lpxh->u.d.fragment_last = 1;

        
        len += sizeof(struct lpxhdr);
    }

    lpxh->pu.pktsize = htons((__u16)(len));
    lpxh->pu.p.type = LPX_TYPE_DATAGRAM;
    DEBUG_CALL(4, ("pktsize = %d, lpxh = %p\n", ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK), lpxh));


    /*
     * Output datagram.
     */
    so = lpxp->lpxp_socket;
    if (so->so_options & SO_DONTROUTE)
        return (Lpx_OUT_outputfl(m, (struct route *)NULL,
                                 (so->so_options & SO_BROADCAST) | LPX_ROUTETOIF, lpxp));
    /*
     * Use cached route for previous datagram if
     * possible.  If the previous net was the same
     * and the interface was a broadcast medium, or
     * if the previous destination was identical,
     * then we are ok.
     *
     * NB: We don't handle broadcasts because that
     *     would require 3 subroutine calls.
     */
    ro = &lpxp->lpxp_route;
#ifdef ancient_history
    /*
     * I think that this will all be handled in Lpx_PCB_connect!
     */
    if (ro->ro_rt != NULL) {
        if(lpx_neteq(lpxp->lpxp_lastdst, lpx->lpx_dna)) {
            /*
             * This assumes we have no GH type routes
             */
            if (ro->ro_rt->rt_flags & RTF_HOST) {
                if (!lpx_hosteq(lpxp->lpxp_lastdst, lpx->lpx_dna))
                    goto re_route;
            }
            if ((ro->ro_rt->rt_flags & RTF_GATEWAY) == 0) {
                register struct lpx_addr *dst =
                &satolpx_addr(ro->ro_dst);
                dst->x_host = lpx->lpx_dna.x_host;
            }
            /* 
             * Otherwise, we go through the same gateway
             * and dst is already set up.
             */
        } else {
re_route:
            RTFREE(ro->ro_rt);
            ro->ro_rt = NULL;
        }
    }
    lpxp->lpxp_lastdst = lpx->lpx_dna;
#endif /* ancient_history */
    return (Lpx_OUT_outputfl(m, ro, so->so_options & SO_BROADCAST, lpxp));
}




#if 0
void
lpx_abort(lpxp)
    struct lpxpcb *lpxp;
{
    struct socket *so = lpxp->lpxp_socket;

    Lpx_PCB_disconnect(lpxp);
    soisdisconnected(so);
}

/*
 * Drop connection, reporting
 * the specified error.
 */
void
lpx_drop(lpxp, errno)
    register struct lpxpcb *lpxp;
    int errno;
{
    struct socket *so = lpxp->lpxp_socket;

    /*
     * someday, in the LPX world
     * we will generate error protocol packets
     * announcing that the socket has gone away.
     *
     * XXX Probably never. LPX does not have error packets.
     */
    /*if (TCPS_HAVERCVDSYN(tp->t_state)) {
        tp->t_state = TCPS_CLOSED;
        tcp_output(tp);
    }*/
    so->so_error = errno;
    Lpx_PCB_disconnect(lpxp);
    soisdisconnected(so);
}

#endif /* if 0 */


