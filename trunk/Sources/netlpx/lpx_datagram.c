#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/dlil.h>

#include <net/ethernet.h>

#include <mach/mach_types.h>

#include "lpx.h"
#include "lpx_if.h"
#include "lpx_pcb.h"
#include "lpx_var.h"
#include "lpx_datagram.h"
#include "lpx_base.h"

#include "Utilities.h"

static int lpxsendspace = LPXSNDQ;
SYSCTL_INT(_net_lpx_datagram, OID_AUTO, lpxsendspace, CTLFLAG_RW,
		   &lpxsendspace, 0, "");
static int lpxrecvspace = LPXRCVQ;
SYSCTL_INT(_net_lpx_datagram, OID_AUTO, lpxrecvspace, CTLFLAG_RW,
		   &lpxrecvspace, 0, "");

struct  pr_usrreqs lpx_datagram_usrreqs = {
    lpx_datagram_user_abort, 
	pru_accept_notsupp, 
	lpx_datagram_user_attach, 
	lpx_datagram_user_bind,
    lpx_datagram_user_connect, 
	pru_connect2_notsupp, 
	NULL, //Lpx_CTL_control, 
	lpx_datagram_user_detach,
    lpx_datagram_user_disconnect, 
	pru_listen_notsupp, 
	lpx_peeraddr, 
	pru_rcvd_notsupp,
    pru_rcvoob_notsupp, 
	lpx_datagram_user_send, 
	pru_sense_null, 
	lpx_datagram_user_shutdown,
    lpx_sockaddr, 
	sosend, 
	soreceive, 
	pru_sopoll_notsupp
};

//
// Lock
//
lck_attr_t			*datagram_mtx_attr;      // mutex attributes
lck_grp_t           *datagram_mtx_grp;       // mutex group definition 
lck_grp_attr_t		*datagram_mtx_grp_attr;  // mutex group attributes
lck_rw_t            *datagram_mtx;           // global mutex for the pcblist

/***************************************************************************
*
*  APIs
*
**************************************************************************/

/*
 *  This may also be called for raw listeners.
 */
void lpx_datagram_input( struct mbuf *m,
							   register struct lpxpcb *lpxp,
							   struct ether_header *frame_header)
{
    register struct lpxhdr *lpxh = mtod(m, struct lpxhdr *);
	//  struct ifnet *ifp = m->m_pkthdr.rcvif;
    struct sockaddr_lpx lpx_lpx;
    struct ether_header *eh;
	//  struct lpx_addr sna_addr;
	int	error;
	
    eh = frame_header;
	
    DEBUG_PRINT(DEBUG_MASK_DGRAM_TRACE, ("Lpx_USER_datagram_input: Entered\n"));
		
    if (lpxp == NULL)
		panic("Lpx_USER_datagram_input: No lpxpcb");
/*    
	DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_datagram_input: D Port 0x%x, S Port 0x%x\n",
										ntohs(lpxh->dest_port), ntohs(lpxh->source_port)
										));
	
	DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_datagram_input: Message ID : 0x%x, Total Length : 0x%x, Frag ID : 0x%x, Frag Length : 0x%x\n",
										ntohs(lpxh->u.d.message_id),
										ntohs(lpxh->u.d.message_length),
										ntohs(lpxh->u.d.fragment_id),
										ntohs(lpxh->u.d.frament_length)
										));
*/	
	/*
     * Construct sockaddr format source address.
     * Stuff source address and datagram in user buffer.
     */
	
    bzero(&lpx_lpx, sizeof(lpx_lpx));
    lpx_lpx.slpx_len = sizeof(lpx_lpx);
    lpx_lpx.slpx_family = AF_LPX;
    bcopy(eh->ether_shost, lpx_lpx.slpx_node, LPX_NODE_LEN);
    lpx_lpx.slpx_port = lpxh->source_port;
  	
	/*
	DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_datagram_input: %02x:%02x:%02x:%02x:%02x:%02x:%d\n", eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5], lpxh->source_port));
    DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_datagram_input: %02x:%02x:%02x:%02x:%02x:%02x:%d\n", lpx_lpx.slpx_node[0], lpx_lpx.slpx_node[1], lpx_lpx.slpx_node[2], lpx_lpx.slpx_node[3], lpx_lpx.slpx_node[4], lpx_lpx.slpx_node[5], lpx_lpx.slpx_port));
	*/
	
	// Remove Lpx Header. 
	m_adj(m, sizeof(struct lpxhdr));
  
	// Drop Bad Packet.
	if (m->m_len < ntohs(lpxh->u.d.message_length)) {
		goto bad;
	}
	
	// Trim data.
	if (m->m_len > ntohs(lpxh->u.d.message_length)) {
		m_adj(m, - (m->m_len - ntohs(lpxh->u.d.message_length)));
	}

	if (sbappendaddr(&lpxp->lpxp_socket->so_rcv, (struct sockaddr *)&lpx_lpx,
                     m, (struct mbuf *)NULL, &error) == 0) {
        DEBUG_PRINT(DEBUG_MASK_DGRAM_WARING, ("Lpx_USER_input: Maybe No space for this packet. sbappendaddr error: socket=@(%lx), rcv@(%lx)!",lpxp->lpxp_socket, &(lpxp->lpxp_socket->so_rcv)));
		
		// sbappendaddr() free m when the error occured.

    } else {
		
		//    m->m_len += sizeof(struct sockaddr);
		//    m->m_pkthdr.len += sizeof(struct sockaddr);
		//    m->m_data -= sizeof(struct sockaddr);
		
		DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_input: socket sbflag=%x\n",lpxp->lpxp_socket->so_rcv.sb_flags));
		
		sorwakeup(lpxp->lpxp_socket);
		DEBUG_PRINT(DEBUG_MASK_DGRAM_INFO, ("Lpx_USER_input: wokeup socket @(%lx)\n",lpxp->lpxp_socket));
	}
	
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

/***************************************************************************
*
*  Internal functions
*
**************************************************************************/

extern struct lpxpcb	lpx_datagram_pcb;

void lpx_datagram_init()
{
	DEBUG_PRINT(DEBUG_MASK_DGRAM_TRACE, ("lpx_datagram_init: Entered.\n"));
	
	// Init Lock.
	datagram_mtx_grp_attr = lck_grp_attr_alloc_init();
	lck_grp_attr_setdefault(datagram_mtx_grp_attr);
	
	datagram_mtx_grp = lck_grp_alloc_init("datagrampcb", datagram_mtx_grp_attr);
	
	datagram_mtx_attr = lck_attr_alloc_init();
	lck_attr_setdefault(datagram_mtx_attr);
	
	if ((lpx_datagram_pcb.lpxp_list_rw = lck_rw_alloc_init(datagram_mtx_grp, datagram_mtx_attr)) == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_datagram_init: Can't alloc mtx\n"));
	}
	
	return;
}

void lpx_datagram_free()
{
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_datagram_free: Entered\n"));
	
	// Release Lock.			
	lck_rw_free(lpx_datagram_pcb.lpxp_list_rw, datagram_mtx_grp);
	
	lck_grp_attr_free(datagram_mtx_grp_attr);
	
	lck_grp_free(datagram_mtx_grp);
	
	lck_attr_free(datagram_mtx_attr);	
}

static int lpx_datagram_user_abort( struct socket *so )
{
//	int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
	if (lpxp == 0)
		panic("lpx_datagram_user_abort: so %x null lpxpcb\n", so);
	
	soisdisconnected(so);

	Lpx_PCB_detach(lpxp);

//   s = splnet();
//	sofree(so);

//    splx(s);

	so->so_flags |= SOF_PCBCLEARING;

	Lpx_PCB_dispense(lpxp);
	
    return (0);
}


static int lpx_datagram_user_attach( struct socket *so,
                            int proto,
                            struct proc *td )
{
    int error;
//    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
	if (lpxp != 0)
		panic("lpx_datagram_user_attach: so %x lpxpcb=%x\n", so, lpxp);

//    s = splnet();
    error = Lpx_PCB_alloc(so, &lpx_datagram_pcb, td);
//    splx(s);
    if (error == 0) {
        int lpxsends, lpxrecvs;
		
        lpxsends = 256 * 1024;
        lpxrecvs = 256 * 1024;
		
        while (lpxsends > 0 && lpxrecvs > 0) {
            error = soreserve(so, lpxsends, lpxrecvs);
            if (error == 0)
                break;
            lpxsends -= (1024*2);
            lpxrecvs -= (1024*2);
        }
		
        DEBUG_PRINT(4, ("lpxsends = %d\n", lpxsends/1024));
		
        //        error = soreserve(so, lpxsendspace, lpxrecvspace);
    }
    
    return (error);
}


static int lpx_datagram_user_bind( struct socket *so,
                          struct sockaddr *nam,
                          struct proc *td )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;
	
    if (slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    return (Lpx_PCB_bind(lpxp, nam, td));
}


static int lpx_datagram_user_connect( struct socket *so,
                             struct sockaddr *nam,
                             struct proc *td )
{
    int error;
//    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;
	
    if (slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    if (!lpx_nullhost(lpxp->lpxp_faddr))
        return (EISCONN);
//    s = splnet();
    error = Lpx_PCB_connect(lpxp, nam, td);
//    splx(s);
    if (error == 0)
        soisconnected(so);
    return (error);
}


static int lpx_datagram_user_detach( struct socket *so )
{
//    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
    if (lpxp == NULL)
        return (ENOTCONN);
//    s = splnet();    
	
	Lpx_PCB_detach(lpxp);

//	sofree(so);

	so->so_flags |= SOF_PCBCLEARING;
		
	Lpx_PCB_dispense(lpxp);

	if (lpxp->lpxp_flags & LPXP_NEEDRELEASE) {
		
		// Unlock. sock_inject_data_in will lock. 
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
		
		// It will call sbappend and sorwakeup.
		sock_release((socket_t)so);
		
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
	}
//    splx(s);
    return (0);
}

static int lpx_datagram_user_disconnect( struct socket *so )
{
//    int s;
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
    if (lpx_nullhost(lpxp->lpxp_faddr))
        return (ENOTCONN);
//    s = splnet();
    Lpx_PCB_disconnect(lpxp);
//    splx(s);
    soisdisconnected(so);
	
	// Unlock. sock_inject_data_in will lock. 
	lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
	lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
	
	// It will call sbappend and sorwakeup.
	sock_retain((socket_t)so);
	
	lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
	
	lpxp->lpxp_flags |= LPXP_NEEDRELEASE;
	
    return (0);
}

static int lpx_datagram_user_send( struct socket *so,
                          int flags,
                          struct mbuf *m,
                          struct sockaddr *nam,
                          struct mbuf *control,
                          struct proc *td )
{
    int error;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct lpx_addr laddr;
//    int s = 0;
	
    DEBUG_PRINT(DEBUG_MASK_DGRAM_TRACE, ("lpx_datagram_user_send: Entered.\n"));
		
    if (nam != NULL) {
        laddr = lpxp->lpxp_laddr;
        if (!lpx_nullhost(lpxp->lpxp_faddr)) {
            error = EISCONN;
            goto send_release;
        }
        /*
         * Must block input while temporarily connected.
         */
//        s = splnet();
        error = Lpx_PCB_connect(lpxp, nam, td);
        if (error) {
//            splx(s);
            DEBUG_PRINT(DEBUG_MASK_DGRAM_ERROR, ("lpx_USER_send: error(%d) in Lpx_PCB_connect: so=%lx, lpxp=%lx, nam=%lx\n",error,so, lpxp, nam));
            goto send_release;
        }
    } else {		
        if (lpx_nullhost(lpxp->lpxp_faddr)) {
            error = ENOTCONN;
            DEBUG_PRINT(DEBUG_MASK_DGRAM_ERROR, ("lpx_USER_send: error(%d): so=%lx, lpxp=%lx\n",error,so, lpxp ));
            goto send_release;
        }
    }
    error = lpx_datagram_user_output(lpxp, m);
    if (error) {
		DEBUG_PRINT(DEBUG_MASK_DGRAM_ERROR, ("lpx_USER_send: error(%d) in lpx_USER_output: so=%lx, lpxp=%lx, m=%lx\n",error, so, lpxp, m));
    }
    m = NULL;
    if (nam != NULL) {
        Lpx_PCB_disconnect(lpxp);
 //       splx(s);
        lpxp->lpxp_laddr = laddr;
    }
	
send_release:
		if (m != NULL)
			m_freem(m);
	
    return (error);
}


static int lpx_datagram_user_shutdown( struct socket *so )
{
    socantsendmore(so);
    return (0);
}

static int lpx_datagram_user_output( struct lpxpcb *lpxp,
                            struct mbuf *m0 )
{
    register struct lpxhdr *lpxh;
    register struct socket *so;
    register int len = 0;
    struct mbuf *m;
    struct mbuf *mprev = NULL;
	
    DEBUG_PRINT(DEBUG_MASK_DGRAM_TRACE, ("lpx_datagram_user_output: Entered\n"));
	
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
            DEBUG_PRINT(0, ("m is NULL\n"));
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
    DEBUG_PRINT(4, ("pktsize = %d, lpxh = %p\n", ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK), lpxh));
	
    /*
     * Output datagram.
     */
    so = lpxp->lpxp_socket;

    return (lpx_output(m, so->so_options & SO_BROADCAST, lpxp));
}

int lpx_datagram_lock(struct socket *so, int refcount, int debug) {
	
	if (so->so_pcb) {
		lck_mtx_assert(((struct lpxpcb *)so->so_pcb)->lpxp_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(((struct lpxpcb *)so->so_pcb)->lpxp_mtx);
	}
	else {
		panic("lpx_datagram_lock: so=%x NO PCB!\n", so);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
	}
	
	if (refcount) 
		so->so_usecount++;
	
	return (0);
}

int lpx_datagram_unlock(struct socket *so, int refcount, int debug) {

	if (refcount) {
		so->so_usecount--;
	}
	if (so->so_pcb == NULL) {
		panic("lpx_datagram_unlock: so=%x NO PCB!\n", so);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
	}
	else {
		lck_mtx_assert(((struct lpxpcb *)so->so_pcb)->lpxp_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(((struct lpxpcb *)so->so_pcb)->lpxp_mtx);
	}
		
	return (0);
}

lck_mtx_t *lpx_datagram_getlock(struct socket *so, int locktype) {
	
	struct lpxpcb *lpxp = sotolpxpcb(so);
	
	if (so->so_pcb)
		return(lpxp->lpxp_mtx);
	else {
//		panic("lpx_datagram_getlock: so=%x NULL so_pcb\n", so);
		return (so->so_proto->pr_domain->dom_mtx);
	}
}
