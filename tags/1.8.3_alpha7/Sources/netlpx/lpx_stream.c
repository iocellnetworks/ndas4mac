/***************************************************************************
 *
 *  lpx_stream_usrreq.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

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
#include <sys/queue.h>
#include <sys/kpi_socketfilter.h>

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
#include "lpx_stream_var.h"
#include "lpx_stream.h"
#include "lpx_base.h"
#include "lpx_stream_timer.h"

#include "Utilities.h"


struct   lpx_stream_istat lpx_stream_istat;
u_short  lpx_stream_iss;
//static int  traceallsmps = 0;
//static struct   smp     lpx_stream_savesi;
//static int  lpx_stream_use_delack = 0;

static u_short  lpx_stream_newchecks[50];
static int lpx_stream_backoff[SMP_MAXRXTSHIFT+1] =
    { 1, 2, 4, 8, 16, 32, 64 };
//static int lpx_stream_do_persist_panics = 0;

//
// Lock
//
lck_attr_t			*stream_mtx_attr;      // mutex attributes
lck_grp_t           *stream_mtx_grp;       // mutex group definition 
lck_grp_attr_t		*stream_mtx_grp_attr;  // mutex group attributes
lck_rw_t            *stream_mtx;           // global mutex for the pcblist


#define SMPDEBUG0
#define SMPDEBUG1()
#define SMPDEBUG2(req)

#define COMMON_START()  SMPDEBUG0; \
						do { \
							if (lpxp == 0) { \
								return EINVAL; \
							} \
							cb = lpxpcbtostreampcb(lpxp); \
								SMPDEBUG1(); \
						} while (0)

#define COMMON_END(req) out: SMPDEBUG2(req); return error; goto out

/*
#define COMMON_START()  SMPDEBUG0; \
            do { \
                     if (lpxp == 0) { \
						  splx(s); \
                         return EINVAL; \
                     } \
                     cb = lpxpcbtostreampcb(lpxp); \
                     SMPDEBUG1(); \
             } while (0)
                 
#define COMMON_END(req) out: SMPDEBUG2(req);  splx(s); return error; goto out
*/

#define SMPS_HAVERCVDSYN(s) ((s) >= LPX_STREAM_SYN_RECEIVED)

#define SPINC sizeof(struct smphdr)

struct  pr_usrreqs lpx_stream_usrreqs = {
    lpx_stream_user_abort, 
	lpx_stream_user_accept, 
	lpx_stream_user_attach, 
	lpx_stream_user_bind,
    lpx_stream_user_connect, 
	pru_connect2_notsupp, 
	NULL, //Lpx_CTL_control, 
	lpx_stream_user_detach,
    lpx_stream_user_disconnect, 
	lpx_stream_user_listen, 
	lpx_peeraddr, 
	lpx_stream_user_rcvd,
    pru_rcvoob_notsupp, 
	lpx_stream_user_send, 
	pru_sense_null, 
	lpx_stream_user_shutdown,
    lpx_sockaddr, 
	sosend, 
	soreceive, 
	pru_sopoll_notsupp
};

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

extern struct lpxpcb	lpx_stream_pcb;

void lpx_stream_init()
{
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_init: Entered\n"));

    lpx_stream_iss = 1; /* WRONG !! should fish it out of TODR */
	
	// Init Lock.
	stream_mtx_grp_attr = lck_grp_attr_alloc_init();
	lck_grp_attr_setdefault(stream_mtx_grp_attr);
	
	stream_mtx_grp = lck_grp_alloc_init("streampcb", stream_mtx_grp_attr);
	
	stream_mtx_attr = lck_attr_alloc_init();
	lck_attr_setdefault(stream_mtx_attr);
	
	if ((lpx_stream_pcb.lpxp_list_rw = lck_rw_alloc_init(stream_mtx_grp, stream_mtx_attr)) == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_init: Can't alloc rw lock\n"));
	}
	
	if ((lpx_stream_pcb.lpxp_mtx = lck_mtx_alloc_init(stream_mtx_grp, stream_mtx_attr)) == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_init: Can't alloc mtx\n"));
	}
	
}

void lpx_stream_free()
{
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_release: Entered\n"));
		
	// Release Lock.			
	lck_rw_free(lpx_stream_pcb.lpxp_list_rw, stream_mtx_grp);

	lck_mtx_free(lpx_stream_pcb.lpxp_mtx, stream_mtx_grp);

	lck_grp_attr_free(stream_mtx_grp_attr);
	
	lck_grp_free(stream_mtx_grp);

	lck_attr_free(stream_mtx_attr);	
}

#if 0
int Smp_ctloutput( struct socket *so, struct sockopt *sopt )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct stream_pcb *cb;
    int mask, error;
    short soptval;
    u_short usoptval;
    int optval;

    DEBUG_PRINT(4, ("Smp_ctloutput\n"));

    error = 0;

    if (sopt->sopt_level != LPXPROTO_STREAM) {
        /* This will have to be changed when we do more general
           stacking of protocols */
        return (Lpx_USER_ctloutput(so, sopt));
    }
    if (lpxp == NULL)
        return (EINVAL);
    else
        cb = lpxpcbtostreampcb(lpxp);

    switch (sopt->sopt_dir) {
    case SOPT_GET:
        switch (sopt->sopt_name) {
        case SO_HEADERS_ON_INPUT:
            mask = SF_HI;
            goto get_flags;

        case SO_HEADERS_ON_OUTPUT:
            mask = SF_HO;
        get_flags:
            soptval = cb->s_flags & mask;
            error = sooptcopyout(sopt, &soptval, sizeof soptval);
            break;

        case SO_MTU:
            usoptval = cb->s_mtu;
            error = sooptcopyout(sopt, &usoptval, sizeof usoptval);
            break;

        case SO_LAST_HEADER:
            error = sooptcopyout(sopt, &cb->s_rhdr, 
                         sizeof cb->s_rhdr);
            break;

        case SO_DEFAULT_HEADERS:
            error = sooptcopyout(sopt, &cb->s_shdr, 
                         sizeof cb->s_shdr);
            break;

        default:
            error = ENOPROTOOPT;
        }
        break;

    case SOPT_SET:
        switch (sopt->sopt_name) {
            /* XXX why are these shorts on get and ints on set?
               that doesn't make any sense... */
        case SO_HEADERS_ON_INPUT:
            mask = SF_HI;
            goto set_head;

        case SO_HEADERS_ON_OUTPUT:
            mask = SF_HO;
        set_head:
            error = sooptcopyin(sopt, &optval, sizeof optval,
                        sizeof optval);
            if (error)
                break;

            if (cb->s_flags & SF_PI) {
                if (optval)
                    cb->s_flags |= mask;
                else
                    cb->s_flags &= ~mask;
            } else error = EINVAL;
            break;

        case SO_MTU:
            error = sooptcopyin(sopt, &usoptval, sizeof usoptval,
                        sizeof usoptval);
            if (error)
                break;
            cb->s_mtu = usoptval;
            break;

#ifdef SF_NEWCALL
        case SO_NEWCALL:
            error = sooptcopyin(sopt, &optval, sizeof optval,
                        sizeof optval);
            if (error)
                break;
            if (optval) {
                cb->s_flags2 |= SF_NEWCALL;
                lpx_stream_newchecks[5]++;
            } else {
                cb->s_flags2 &= ~SF_NEWCALL;
                lpx_stream_newchecks[6]++;
            }
            break;
#endif

        case SO_DEFAULT_HEADERS:
            {
                struct smphdr sp;

                error = sooptcopyin(sopt, &sp, sizeof sp,
                            sizeof sp);
                if (error)
                    break;
                cb->s_dt = sp.lpx_stream_dt;
                cb->s_cc = sp.lpx_stream_cc & SMP_EM;
            }
            break;

        default:
            error = ENOPROTOOPT;
        }
        break;
    }
    return (error);
}

void Smp_ctlinput( int cmd,
                   struct sockaddr *arg_as_sa, /* XXX should be swapped with dummy */
                   void *dummy )
{
    caddr_t arg = (/* XXX */ caddr_t)arg_as_sa;
    struct lpx_addr *na;
    struct sockaddr_lpx *slpx;

    DEBUG_PRINT(4, ("lpx_stream_ctlinput\n"));

    if (cmd < 0 || cmd > PRC_NCMDS)
        return;

    switch (cmd) {

    case PRC_ROUTEDEAD:
        return;

    case PRC_IFDOWN:
    case PRC_HOSTDEAD:
    case PRC_HOSTUNREACH:
        slpx = (struct sockaddr_lpx *)arg;
        if (slpx->slpx_family != AF_LPX)
            return;
        na = &slpx->sipx_addr;
        break;

    default:
        break;
    }
}
#endif


void lpx_stream_input( register struct mbuf *m, 
					   register struct lpxpcb *lpxp,
					   void *frame_header)
{
    register struct stream_pcb *cb;
    register struct lpxhdr *lpxh = NULL;
    register struct socket *so;
    struct ether_header *eh;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_input: Entered.\n"));
        	
	lpx_stream_stat.smps_rcvtotal++;
    if (lpxp == NULL) {
        panic("lpx_stream_input: No lpxpcb in lpx_stream_input\n");
    }
    
	// Get stream PCB.
	cb = lpxpcbtostreampcb(lpxp);
    if (cb == NULL)
        goto bad;
    
    eh = (struct ether_header *)frame_header;
	/*
	// If the received packet has EXT cluster, We have to call m_pullup to arrange that packet. 
	if (m->m_flags & M_EXT) {
		if ((m = m_pullup(m, sizeof(struct lpxhdr))) == 0) {
			lpx_stream_stat.smps_rcvshort++;
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: pullup failed!!!\n"));

			return;
		}
	}
*/
	// We pullup-ed at lpx_input() already.
	lpxh = mtod(m, struct lpxhdr *);

    lpxh->u.s.sequence = ntohs(lpxh->u.s.sequence);
    lpxh->u.s.ackseq = ntohs(lpxh->u.s.ackseq);

	DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: size %d\n", ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK)));

	DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: cb->s_state = %x\n", cb->s_state));
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: lpxp->lpxp_laddr.x_port = %x\n", lpxp->lpxp_laddr.x_port));
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: cb = %p, lpxh->dest_port = %x, lpxh->u.s.sequence = %x, lpxh->u.s.ackseq = %x, cb->s_rack = %x, cb->s_seq = %x, cb->s_ack = %x\n", cb, ntohs(lpxh->dest_port), lpxh->u.s.sequence, lpxh->u.s.ackseq, cb->s_rack, cb->s_seq, cb->s_ack));
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));
	
    so = lpxp->lpxp_socket;
		
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input : to so@%p\n",so));
 
    if (so && so->so_options & SO_ACCEPTCONN) {
        struct stream_pcb *ocb = cb;

        so = sonewconn(so, 0, NULL);
        if (so == NULL) {
            goto drop;
        }
      
        /*
         * This is ugly, but ....
         *
         * Mark socket as temporary until we're
         * committed to keeping it. We mark the socket as discardable until
         * we're committed to it below in LPX_STREAM_LISTEN.
         */
        
        lpxp = (struct lpxpcb *)so->so_pcb;
        bcopy(eh->ether_dhost, lpxp->lpxp_laddr.x_node, LPX_NODE_LEN);
        lpxp->lpxp_laddr.x_port = ntohs(lpxh->dest_port);
                
        cb = lpxpcbtostreampcb(lpxp);
        cb->s_mtu = ocb->s_mtu;     /* preserve sockopts */
        cb->s_flags = ocb->s_flags; /* preserve sockopts */
        cb->s_flags2 = ocb->s_flags2;   /* preserve sockopts */
        cb->s_state = LPX_STREAM_LISTEN;
    }

    /*
     * Packet received on connection.
     * reset idle time and keep-alive timer;
     */
    cb->s_idle = 0;
    cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;

    switch (cb->s_state) 
	{
		
		case LPX_STREAM_LISTEN:
		{
			if (ntohs(lpxh->u.s.lsctl) | LSCTL_CONNREQ) {
				/*
				 * If somebody here was carrying on a conversation
				 * and went away, and his pen pal thinks he can
				 * still talk, we get the misdirected packet.
				 */
				{
					struct sockaddr_lpx slpx;
					struct lpx_addr laddr;
					struct proc *proc0 = proc_self();
					
					bzero(&slpx, sizeof(slpx));
					slpx.slpx_len = sizeof(slpx);
					slpx.slpx_family = AF_LPX;
					bcopy(eh->ether_shost, slpx.slpx_node, LPX_NODE_LEN);
					slpx.slpx_port = ntohs(lpxh->source_port);
					laddr = lpxp->lpxp_laddr;
					if (lpx_nullhost(laddr)) {
						bcopy(eh->ether_dhost, lpxp->lpxp_laddr.x_node, LPX_NODE_LEN);
						lpxp->lpxp_laddr.x_port = ntohs(lpxh->dest_port);
					}
					
					if (Lpx_PCB_connect(lpxp, (struct sockaddr *)&slpx, proc0)) {
						lpxp->lpxp_laddr = laddr;
						lpx_stream_istat.noconn++;
						goto drop;
					}
				}
				
				lpx_stream_template(cb);
				
				cb->s_rack = lpxh->u.s.ackseq;
				cb->s_state = LPX_STREAM_SYN_RECEIVED;
				cb->s_force = 1 + SMPT_KEEP;
				lpx_stream_stat.smps_accepts++;
				cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;					
				
				if (lpxh->u.r.option & LPX_OPTION_SMALL_LENGTH_DELAYED_ACK) {
					cb->bSmallLengthDelayedAck = TRUE;
					cb->delayedAckCount = DEFAULT_WINDOW_SIZE;
				}
			}
        }
			break;
			/*
			 * This state means that we have heard a response
			 * to our acceptance of their connection
			 * It is probably logically unnecessary in this
			 * implementation.
			 */
		case LPX_STREAM_SYN_RECEIVED: {
			
			if (!cb->s_lpxpcb->lpxp_socket) {
				DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_input : LPX_STREAM_SYN_RECEIVED No socket\n"));
				
				goto drop;
			}
			
			// Tag
			cb->s_tag = lpxh->u.s.tag;
			lpxp->lpxp_fport = lpxh->source_port; 
			cb->s_timer[SMPT_REXMT] = 0;
			cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
			soisconnected(so);
			cb->s_state = LPX_STREAM_ESTABLISHED;
			
			cb->s_lpxpcb->lpxp_socket->so_options |= SO_KEEPALIVE;
			cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;

			lpx_stream_stat.smps_accepts++;
/*
			lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
			lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
			
			// It will call sbappend and sorwakeup.
//			sock_retain((socket_t)so);

			lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
			lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);					
 */
		}
			break;
			
			/*
			 * This state means that we have gotten a response
			 * to our attempt to establish a connection.
			 * We fill in the data from the other side,
			 * telling us which port to respond to, instead of the well-
			 * known one we might have sent to in the first place.
			 * We also require that this is a response to our
			 * connection id.
			 */
		case LPX_STREAM_SYN_SENT: 
		{
			if (!cb->s_lpxpcb->lpxp_socket) {
				DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_input : LPX_STREAM_SYN_SENT No socket\n"));
				
				goto drop;
			}
						
			if (ntohs(lpxh->u.s.lsctl) | LSCTL_CONNREQ) {

				lpx_stream_stat.smps_connects++;
				cb->s_rack = lpxh->u.s.ackseq;
				cb->s_dport = lpxp->lpxp_fport = lpxh->source_port;
				cb->s_timer[SMPT_REXMT] = 0;
				cb->s_flags |= SF_ACKNOW;
				
				cb->s_seq = 1;
				cb->s_snxt = 1;
				cb->s_smax = 0;
				cb->s_ack = 1;
				cb->s_tag = lpxh->u.s.tag;
				
				soisconnected(so);
				cb->s_state = LPX_STREAM_ESTABLISHED;
				cb->s_lpxpcb->lpxp_socket->so_options |= SO_KEEPALIVE;
				cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
				DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: cb = %p time = %d\n", cb, cb->s_timer[SMPT_KEEP]));
				
				/*
				 lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
				 lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
				 
				 // It will call sbappend and sorwakeup.
				 //			sock_retain((socket_t)so);
				 
				 lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
				 lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);					
				 */
				
				if (lpxh->u.r.option & LPX_OPTION_SMALL_LENGTH_DELAYED_ACK) {
					cb->bSmallLengthDelayedAck = TRUE;
					cb->delayedAckCount = DEFAULT_WINDOW_SIZE;
				}
				
			}
			
			 break;
		}
			
		case LPX_STREAM_ESTABLISHED: 
		{
			switch (ntohs(lpxh->u.s.lsctl)) {
				case LSCTL_DISCONNREQ:
				case LSCTL_DISCONNREQ | LSCTL_ACK:
					cb->s_ack ++;
					cb->s_flags |= SF_ACKNOW;
					cb->s_state = LPX_STREAM_CLOSE_WAIT;
					break;
				case LSCTL_ACKREQ:
				case LSCTL_ACKREQ | LSCTL_ACK:
					cb->s_flags |= SF_ACKNOW;
			}
			
			break;
		}
			
		case LPX_STREAM_LAST_ACK:
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: LPX_STREAM_LAST_ACK\n"));
			
			if (ntohs(lpxh->u.s.lsctl) == LSCTL_ACK) {
				cb->s_state = LPX_STREAM_TIME_WAIT; 
				soisdisconnected(so);
				lpx_stream_close(cb);		

				cb->s_timer[SMPT_2MSL] = SMPTV_MSL;
				
				cb->s_lpxpcb->lpxp_socket->so_options &= ~SO_KEEPALIVE;
				cb->s_timer[SMPT_KEEP] = 0;
			}
				
				break;
			
		case LPX_STREAM_FIN_WAIT_1: 
		{
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: LPX_STREAM_FIN_WAIT_1, ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));
			
			switch (ntohs(lpxh->u.s.lsctl)) 
			{
				case LSCTL_DATA:
				case LSCTL_DATA|LSCTL_ACK:
					
					if (!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK))
						break;
					
				case LSCTL_ACK:
					if (lpxh->u.s.ackseq == (cb->s_fseq+1)) {
						cb->s_seq++;          
						cb->s_state = LPX_STREAM_FIN_WAIT_2;
					}
					
					break;
					
				case LSCTL_DISCONNREQ:
				case LSCTL_DISCONNREQ|LSCTL_ACK:
				{
					cb->s_ack ++;
					cb->s_flags |= SF_ACKNOW;
					
					cb->s_state = LPX_STREAM_CLOSING;
					
					if (!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK)) {
						break;
					}
					
					if (lpxh->u.s.ackseq == (cb->s_fseq+1)) {
						cb->s_seq++;
						cb->s_state = LPX_STREAM_TIME_WAIT;
						soisdisconnected(so);
						lpx_stream_close(cb);		

						cb->s_timer[SMPT_2MSL] = SMPTV_MSL;
						
						cb->s_lpxpcb->lpxp_socket->so_options &= ~SO_KEEPALIVE;
						cb->s_timer[SMPT_KEEP] = 0;
					}
				}		
			}
		}
			break;
			
		case LPX_STREAM_FIN_WAIT_2: 
		{
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_input: LPX_STREAM_FIN_WAIT_2\n"));
			
			switch (ntohs(lpxh->u.s.lsctl)) 
			{
				case LSCTL_DATA:
				case LSCTL_DATA|LSCTL_ACK:
					
					break;
					
				case LSCTL_DISCONNREQ:
				case LSCTL_DISCONNREQ|LSCTL_ACK:
				{
					cb->s_ack ++;
					cb->s_flags |= SF_ACKNOW;
					
					if (!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK))
						break;
					
					cb->s_state = LPX_STREAM_TIME_WAIT; 
					soisdisconnected(so);
					lpx_stream_close(cb);		

					cb->s_timer[SMPT_2MSL] = SMPTV_MSL;
					
					cb->s_lpxpcb->lpxp_socket->so_options &= ~SO_KEEPALIVE;
					cb->s_timer[SMPT_KEEP] = 0;
				}
					break;
			}
		}
		case LPX_STREAM_CLOSING:
		{
			if (lpxh->u.s.ackseq == (cb->s_fseq+1)) {
				cb->s_seq++;
				cb->s_state = LPX_STREAM_TIME_WAIT;
				soisdisconnected(so);
				lpx_stream_close(cb);		
				
				cb->s_timer[SMPT_2MSL] = SMPTV_MSL;
				
				cb->s_lpxpcb->lpxp_socket->so_options &= ~SO_KEEPALIVE;
				cb->s_timer[SMPT_KEEP] = 0;
			}			
		}
			break;
	}
    	
    if (so  && lpx_stream_reass(cb, lpxh)) {
        m_freem(m);
    }
	
	// Check Packet Continue bit.
	if (cb->s_flags & SF_ACKNOW) {
		if ((lpxh->u.r.option & LPX_OPTION_PACKETS_CONTINUE_BIT) == 0) {
			if (cb->bSmallLengthDelayedAck) {
				cb->delayedAckCount = DEFAULT_WINDOW_SIZE;
			}				
		} else {
			if (cb->bSmallLengthDelayedAck) {
				if (cb->delayedAckCount > 0) {
					cb->delayedAckCount--;
					cb->s_flags &= ~SF_ACKNOW;
				} else {
					cb->delayedAckCount = DEFAULT_WINDOW_SIZE;
				}
			}
		}
	}
    
    if (cb->s_force || (cb->s_flags & (SF_ACKNOW|SF_WIN|SF_RXT))) {
        lpx_stream_output(cb, (struct mbuf *)NULL);
	}
	
    cb->s_flags &= ~(SF_WIN|SF_RXT);
    
	return;
		
drop:
bad:
	m_freem(m);
	
	return;
}


/***************************************************************************
 *
 *  Internal functions
 *
 **************************************************************************/

static int lpx_stream_user_attach(struct socket *so, int proto, struct proc *p)
{
//    int s = splnet();
    int error;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_user_attach: Entered. so@%p: lpxp@%p\n",so,lpxp));
	
    if (lpxp) {
        error = EISCONN;
        goto out;
    }

    error = lpx_stream_attach(so, p);
    if (error)
        goto out;

out:	
//   splx(s);
    return error;
}


static int lpx_stream_user_detach(struct socket *so)
{
	//int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_detach: Entered.\n"));
	
    if (lpxp == 0) {
		//lpx_stream_unlock(so, 0, 0);
        //splx(s);
        return EINVAL;  /* XXX */
    }
    cb = lpxpcbtostreampcb(lpxp);
    /* In case we got disconnected from the peer */
    if (cb == 0) 
        goto out;

//	DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_detach: so_userCount %d\n", lpxp->lpxp_socket->so_usecount));
	
    lpx_stream_disconnect(cb);
//    cb = lpx_stream_disconnect(cb);
	
//	soisdisconnected(so);
//	lpx_stream_close(cb);
	
//	Lpx_PCB_detach(cb->s_lpxpcb);
	
//	sofree(cb->s_lpxpcb->lpxp_socket);
	
out:
    //splx(s);
    return error;
}



static int lpx_stream_user_bind( struct socket *so,
                         struct sockaddr *nam,
                         struct proc *p )
{
    //int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;

	DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_user_bind: Entered.\n"));
	
    if (slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    COMMON_START();

    error = Lpx_PCB_bind(lpxp, nam, p);
    if (error) {
        DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_user_bind: !!!Bind failure!!!\n"));
        goto out;
    }
    COMMON_END(PRU_BIND);
}


static int lpx_stream_user_listen(struct socket *so, struct proc *p)
{
    //int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_listen\n"));
	
    COMMON_START();
    if (lpxp->lpxp_lport == 0)
        error = Lpx_PCB_bind(lpxp, (struct sockaddr *)0, p);
    if (error == 0) {
        cb->s_state = LPX_STREAM_LISTEN;
    } else {
        DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("!!!lpx_stream_usr_listen: Bind failure!!!\n"));
    }
    COMMON_END(PRU_LISTEN);
}


static int lpx_stream_user_connect( struct socket *so,
                            struct sockaddr *nam,
                            struct proc *td )
{
    int error;
 //   int s;
    struct lpxpcb *lpxp;
    struct stream_pcb *cb;
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;
	
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_connect\n"));

    if (slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
                  
    lpxp = sotolpxpcb(so);
    cb = lpxpcbtostreampcb(lpxp);
	
   //s = splnet();
    if (lpxp->lpxp_lport == 0) {

        error = Lpx_PCB_bind(lpxp, NULL, td);

        if (error) {
            DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("!!!lpx_stream_usr_connect: Bind failure!!!\n"));
            goto lpx_stream_connect_end;
        }
    }

    error = Lpx_PCB_connect(lpxp, nam, td);

    if (error)
        goto lpx_stream_connect_end;

    
    soisconnecting(so);
    lpx_stream_stat.smps_connattempt++;
    cb->s_state = LPX_STREAM_SYN_SENT;
    cb->s_did = 0;
    lpx_stream_template(cb);
    cb->s_timer[SMPT_KEEP] = 10 * SMPTV_KEEP;
    cb->s_force = 1 + SMPTV_KEEP;
    /*
     * Other party is required to respond to
     * the port I send from, but he is not
     * required to answer from where I am sending to,
     * so allow wildcarding.
     * original port I am sending to is still saved in
     * cb->s_dport.
     */
    lpxp->lpxp_fport = 0;
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_usr_connect for lpxp@%p, lpxp->lpxp_laddr.x_port = %x\n",
           lpxp, lpxp->lpxp_laddr.x_port));
    error = lpx_stream_output(cb, (struct mbuf *)NULL);
	
lpx_stream_connect_end:
    //splx(s);
    return (error);
}


static int lpx_stream_user_disconnect(struct socket *so)
{
//    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_disconnect: Entered.\n"));
	
	DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_disconnect: so_userCount %d\n", so->so_usecount));

    COMMON_START();
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    lpx_stream_disconnect(cb);
//    cb = lpx_stream_disconnect(cb);
	
//	soisdisconnected(so);
//	lpx_stream_close(cb);

    COMMON_END(PRU_DISCONNECT);
}


static int lpx_stream_user_accept(struct socket *so, struct sockaddr **nam)
{
//    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_accept\n"));

    if (so->so_state & SS_ISDISCONNECTED) {
        error = ECONNABORTED;
        goto out;
    }
    if (lpxp == 0) {
        //splx(s);
        return (EINVAL);
    }

	lpx_peeraddr(so, nam);
	
    COMMON_END(PRU_ACCEPT);
}


static int lpx_stream_user_shutdown(struct socket *so)
{
//    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_shutdown: Entered.\n"));
	
    COMMON_START();
    socantsendmore(so);
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    cb = lpx_stream_usrclosed(cb);
    if (cb)
        error = lpx_stream_output(cb, (struct mbuf *)NULL);

    COMMON_END(PRU_SHUTDOWN);
}


static int lpx_stream_user_rcvd(struct socket *so, int flags)
{
//    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_rcvd\n"));

    COMMON_START();
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    lpx_stream_output(cb, (struct mbuf *)NULL);
	
    COMMON_END(PRU_RCVD);
}


static int lpx_stream_user_send( struct socket *so,
                         int flags,
                         struct mbuf *m,
                         struct sockaddr *addr,
                         struct mbuf *controlp,
                         struct proc *td)
{
    int error;
//    int s;
    struct lpxpcb *lpxp;
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usr_send: Entered.\n"));

    lpxp = sotolpxpcb(so);
    cb = lpxpcbtostreampcb(lpxp);

 //   s = splnet();
	
    if (controlp != NULL) {
        u_short *p = mtod(controlp, u_short *);
        lpx_stream_newchecks[2]++;
        if ((p[0] == 5) && (p[1] == 1)) { // XXXX, for testing
            cb->s_shdr.lpx_stream_dt = *(u_char *)(&p[2]);
            lpx_stream_newchecks[3]++;
        }
        m_freem(controlp);
	}
    controlp = NULL;

	error = lpx_stream_output(cb, m);

	if (error) {
        DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR,("lpx_stream_usr_send: error(%d) in lpx_stream_output so@%p, cb@%p, m@%p\n",error,so,cb,m));
    }
    m = NULL;
lpx_stream_send_end:
    if (controlp != NULL)
        m_freem(controlp);
    if (m != NULL)
        m_freem(m);
	
    //splx(s);
    return (error);
}


static int lpx_stream_user_abort(struct socket *so)
{
//    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct stream_pcb *cb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_usr_abort\n"));

    COMMON_START();
        /* In case we got disconnected from the peer */
    if (cb == 0)
        goto out;
    lpx_stream_drop(cb, ECONNABORTED);
    COMMON_END(PRU_ABORT);
}

static int lpx_stream_attach( struct socket *so, struct proc *td )
{
    int error;
    struct lpxpcb *lpxp = NULL;
    struct stream_pcb *cb;
    struct mbuf *mm;
    struct sockbuf *sb;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_attach\n"));

    lpxp = sotolpxpcb(so);

    if (lpxp != NULL)
        return (EISCONN);
	
    error = Lpx_PCB_alloc(so, &lpx_stream_pcb, td);

    if (error) {
        goto lpx_stream_attach_end;
    }

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        int smpsends, smprecvs;
                            
        smpsends = 256 * 1024;
        smprecvs = 256 * 1024;

        while (smpsends > 0 && smprecvs > 0) {
            error = soreserve(so, smpsends, smprecvs);
            if (error == 0)
                break;
            
            smpsends -= (1024*2);
            smprecvs -= (1024*2);
        }

        DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("smpsends = %d\n", smpsends/1024));
    
        // error = soreserve(so, (u_long) 3072, (u_long) 3072);
        if (error)
            goto lpx_stream_attach_end;
    }
        
    lpxp = sotolpxpcb(so);

    MALLOC(cb, struct stream_pcb *, sizeof *cb, M_PCB, M_WAITOK);

    if (cb == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("cant alloc pcb.\n"));

        error = ENOBUFS;
        goto lpx_stream_attach_end;
    }

    bzero(cb, sizeof(*cb));
      
    sb = &so->so_snd;
	
	mm = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (mm == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("cant alloc by m_getclr()\n"));
		
        FREE(cb, M_PCB);
        error = ENOBUFS;
        goto lpx_stream_attach_end;
	}
			
    cb->s_lpx = mtod(mm, struct lpx *);
	bzero(cb->s_lpx, sizeof(*cb->s_lpx));
    cb->s_state = LPX_STREAM_CLOSED;
    cb->s_smax = -1;
    cb->s_swl1 = -1;
    cb->s_q.si_next = cb->s_q.si_prev = &cb->s_q;
    cb->s_lpxpcb = lpxp;
    cb->s_mtu = min_mtu - sizeof(struct smp);  //1400 - sizeof(struct smp)
    cb->s_cwnd = sbspace(sb) * CUNIT / cb->s_mtu;
    cb->s_ssthresh = cb->s_cwnd;
    cb->s_cwmx = sbspace(sb) * CUNIT / (2 * sizeof(struct smp));
    /* Above is recomputed when connecting to account
       for changed buffering or mtu's */
    cb->s_rtt = SMPTV_SRTTBASE;
    cb->s_rttvar = SMPTV_SRTTDFLT << 2;
    SMPT_RANGESET(cb->s_rxtcur,
        ((SMPTV_SRTTBASE >> 2) + (SMPTV_SRTTDFLT << 2)) >> 1,
        SMPTV_MIN, SMPTV_REXMTMAX);

    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("cb->s_rxtcur = %d\n", cb->s_rxtcur));
    cb->s_rxtcur = SMPTV_MIN;
    
    lpxp->lpxp_pcb = (caddr_t)cb; 
    DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_attach with so@%p: ==> lpxp@%p : pcb@%p\n",so,lpxp,lpxp->lpxp_pcb));
		
lpx_stream_attach_end:
	
    return (error);
}


static struct stream_pcb *lpx_stream_disconnect( register struct stream_pcb *cb )
{
    struct socket *so = cb->s_lpxpcb->lpxp_socket;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_disconnect: Entered.\n"));

	if (!so) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_disconnect: No socket.\n"));

		return cb;
	}
	
    if (cb->s_state < LPX_STREAM_ESTABLISHED) {
		soisdisconnected(so);

		lpx_stream_close(cb);		

		cb->s_flags_control |= SF_NEEDFREE;
		cb->s_flags &= ~SF_DELACK;
	} else if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_disconnect: Drop Connection.\n"));

        cb = lpx_stream_drop(cb, 0);
	} else {
        soisdisconnecting(so);
        sbflush(&so->so_rcv);
        cb = lpx_stream_usrclosed(cb);
		
        if (cb) {
            (void) lpx_stream_output(cb, (struct mbuf *)NULL);
        }

    }
    return (cb);
}


static struct stream_pcb *lpx_stream_usrclosed(register struct stream_pcb *cb)
{

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_usrclosed: Enterted.\n"));

	if (cb == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_usrclosed: No cb!!!\n"));

		return NULL;
	}
	
	if (!cb->s_lpxpcb->lpxp_socket) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_usrclosed: No socket!!!\n"));
		
		return NULL;		
	}
	
    switch (cb->s_state) {

    case LPX_STREAM_CLOSED:
    case LPX_STREAM_LISTEN:
        cb->s_state = LPX_STREAM_CLOSED;
 		lpx_stream_close(cb);
		cb->s_flags_control |= SF_NEEDFREE;
		cb->s_flags &= ~SF_DELACK;
		
		cb = NULL;
        break;

    case LPX_STREAM_SYN_SENT:
    case LPX_STREAM_SYN_RECEIVED:
        cb->s_flags |= SF_NEEDFIN;
        break;

    case LPX_STREAM_ESTABLISHED:
	{
		struct socket *so = cb->s_lpxpcb->lpxp_socket;
		
        cb->s_state = LPX_STREAM_FIN_WAIT_1;
        cb->s_fseq = cb->s_seq;
		
		cb->s_timer[SMPT_DISCON] = SMPTV_DISCON;
	
		cb->s_flags_control |= SF_NEEDRELEASE;
				
		// Unlock. sock_inject_data_in will lock. 
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
		
		// It will call sbappend and sorwakeup.
		sock_retain((socket_t)so);

		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);					

	}			
        break;

    case LPX_STREAM_CLOSE_WAIT: 
        cb->s_state = LPX_STREAM_LAST_ACK;
        cb->s_fseq = cb->s_seq;
        break;
    }
	
    if (cb && cb->s_state >= LPX_STREAM_FIN_WAIT_2) {
        soisdisconnected(cb->s_lpxpcb->lpxp_socket);
		lpx_stream_close(cb);		

        /* To prevent the connection hanging in FIN_WAIT_2 forever. */
        if (cb->s_state == LPX_STREAM_FIN_WAIT_2) {
            cb->s_timer[SMPT_2MSL] = SMPTV_MSL;
		}

    }
    return (cb);
}


static void lpx_stream_template(register struct stream_pcb *cb)
{
    register struct lpxpcb *lpxp = cb->s_lpxpcb;
    register struct lpx *lpx = cb->s_lpx;
    register struct sockbuf *sb;
	
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_template\n"));

	if (!lpxp->lpxp_socket) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_template: No Socket!\n"));
		return;
	}
	
	sb = &(lpxp->lpxp_socket->so_snd);
	
	
    lpx->lpx_pt = LPXPROTO_STREAM;
    lpx->lpx_sna = lpxp->lpxp_laddr;
    lpx->lpx_dna = lpxp->lpxp_faddr;
    cb->s_sid = htons(lpx_stream_iss);
    lpx_stream_iss += SMP_ISSINCR/2;
    cb->s_alo = 1;
    cb->s_cwnd = (sbspace(sb) * CUNIT) / cb->s_mtu;
    cb->s_ssthresh = cb->s_cwnd; /* Try to expand fast to full complement
                    of large packets */
    cb->s_cwmx = (sbspace(sb) * CUNIT) / (2 * sizeof(struct smp));
    cb->s_cwmx = max(cb->s_cwmx, cb->s_cwnd);
        /* But allow for lots of little packets as well */
}

int lpx_stream_output(register struct stream_pcb *cb, struct mbuf *m0)
{
    struct socket *so = cb->s_lpxpcb->lpxp_socket;
    register struct mbuf *m;
    register struct lpxhdr *lpxh = (struct lpxhdr *)NULL;
    register struct sockbuf *sb = &so->so_snd;
    int len = 0; //, win/*, rcv_win*/;
//    short /*span, off, */recordp = 0;
    int error = 0, sendalot;
    struct lpxpcb *lpxp = streampcbtolpxpcb(cb);
    struct mbuf *mprev;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_output: Entered. so@%p pcb@%p\n",so, cb));
	
	if (!so) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_output: No Socket!\n"));

		return EBADF;
	}
	
    if (m0 != NULL) {
        int mtu = cb->s_mtu;
        int datalen;
        /*
         * Make sure that packet isn't too big.
         */
        for (m = m0; m != NULL; m = m->m_next) {
            mprev = m;
            len += m->m_len;
//            if (m->m_flags & M_EOR)
//                recordp = 1;
        }
        datalen = (cb->s_flags & SF_HO) ?
                len - (int)(sizeof(struct smphdr)) : len;
		
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: datalen %d, mtu %d\n",datalen, mtu));

        if (datalen > mtu) {

			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: datalen is bigger than mtu\n"));
			
            if (cb->s_flags & SF_PI) {
				
				DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: SF_PI set\n"));
				
                m_freem(m0);
                return (EMSGSIZE);
            } else {
                int oldEM = cb->s_cc & SMP_EM;

                cb->s_cc &= ~SMP_EM;
                while (len > mtu) {
					
					DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: while loop len %d, mut %d\n", len, mtu));
					
                    /*
                     * Here we are only being called
                     * from usrreq(), so it is OK to
                     * block.
                     */
                    m = m_copym(m0, 0, mtu, M_DONTWAIT);

                    error = lpx_stream_output(cb, m);
                    if (error) {
                        cb->s_cc |= oldEM;
                        m_freem(m0);
                        return (error);
                    }
                    m_adj(m0, mtu);
                    len -= mtu;
                }
                cb->s_cc |= oldEM;
            }
        }
        /*
         * Force length even, by adding a "garbage byte" if
         * necessary.
         */
        if (len & 1) {
            m = mprev;
            if (M_TRAILINGSPACE(m) >= 1) {
                m->m_len++;
            } else {
                struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);

                if (m1 == NULL) {
                    m_freem(m0);
                    return (ENOBUFS);
                }
                m1->m_len = 1;
                *(mtod(m1, u_char *)) = 0;
                m->m_next = m1;
            }
        }
		
		//
		// Add padding to fix under-60byte bug of NDAS chip 2.0
		//
		
		int minPayload = 60 - sizeof(struct lpxhdr) - sizeof(struct ether_header);
		int	paddingLen = 0;
		
		if (len <= minPayload - 4) {
			caddr_t			padding = NULL;
			
			paddingLen = minPayload - len;
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: Adding padding to support NDAS chip 2.0 len = %d\n", len));
			
			m = m0;
			
			while (m->m_next) {
				m = m->m_next;
			}
			
			if (!(m->m_flags & M_EXT)
				&& M_TRAILINGSPACE(m) >= paddingLen) {
				
				padding = mtod(m, caddr_t) + m->m_len;
				m->m_len += paddingLen;				
				m0->m_pkthdr.len += paddingLen;
				
			} else {
				DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: M_TRAILINGSPACE len = %d\n", M_TRAILINGSPACE(m0)));		
				
				struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);
				
				if (m1 == NULL) {
                    m_freem(m0);
                    return (ENOBUFS);
				} else {
					
					padding = mtod(m1, caddr_t);
					
					m1->m_len = paddingLen;
					m1->m_next = NULL;
					m->m_next = m1;
//					m = m1;	
					m0->m_pkthdr.len += paddingLen;
				}
			}
			
			if (padding) {
				bzero(padding, paddingLen);
				
				m_copydata(m0, len - 4, 4, padding + paddingLen - 4);
			}
		}
		
        /*
         * Fill in mbuf with extended SP header
         * and addresses and length put into network format.
         */		
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
        if (m == NULL) {
            m_freem(m0);
            return (ENOBUFS);
        }
		
		MH_ALIGN(m, sizeof(struct lpxhdr));
        m->m_len = sizeof(struct lpxhdr);
        m->m_next = m0;
        lpxh = mtod(m, struct lpxhdr *);
		bzero(lpxh, sizeof(*lpxh));
		
		lpxh->pu.pktsize = htons(LPX_PKT_LEN + len);
		lpxh->pu.p.type = LPX_TYPE_STREAM;
		lpxh->source_port = lpxp->lpxp_laddr.x_port;
		lpxh->dest_port = cb->s_dport;
		lpxh->u.s.tag = cb->s_tag;
		lpxh->u.s.lsctl = htons(LSCTL_DATA | LSCTL_ACK);

		// There'll be converted to network order later.
		lpxh->u.s.sequence = cb->s_seq;
		lpxh->u.s.ackseq = cb->s_ack;

		// Packet Continued Bit.
		if (datalen == mtu) {
			lpxh->u.r.option |= LPX_OPTION_PACKETS_CONTINUE_BIT;
		}
		
        len += sizeof(*lpxh);
        m->m_pkthdr.len = ((len - 1) | 1) + 1 + paddingLen;
		
        /*
         * queue stuff up for output
         */
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: queue stuff up for output\n"));
		
        sbappendrecord(sb, m);
        cb->s_seq++;
    }

again:
    sendalot = 1; len = 1; // win = 1;

    /*
     * If in persist timeout with window of 0, send a probe.
     * Otherwise, if window is small but nonzero
     * and timer expired, send what we can and go into
     * transmit state.
     */
 /*   if (cb->s_force == 1 + SMPT_PERSIST) {
        if (win != 0) {
            cb->s_timer[SMPT_PERSIST] = 0;
            cb->s_rxtshift = 0;
        }
    }
	*/
    if (cb->s_flags & SF_ACKNOW)
        goto send;
    if (cb->s_state < LPX_STREAM_ESTABLISHED)
        goto send;
    /*
     * Silly window can't happen in smp.
     * Code from tcp deleted.
     */
    if (len)
        goto send;

	/*
     * Many comments from tcp_output.c are appropriate here
     * including . . .
     * If send window is too small, there is data to transmit, and no
     * retransmit or persist is pending, then go to persist state.
     * If nothing happens soon, send when timer expires:
     * if window is nonzero, transmit what we can,
     * otherwise send a probe.
     */
/*    if (so && so->so_snd.sb_cc && cb->s_timer[SMPT_REXMT] == 0 &&
        cb->s_timer[SMPT_PERSIST] == 0) {
            cb->s_rxtshift = 0;
            lpx_stream_setpersist(cb);
    }
*/	
	/*
     * No reason to send a packet, just return.
     */
	DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: No reason to send a packet.\n"));
	
    cb->s_outx = 1;
    return (0);

send:
	/*
	 * Find requested packet.
	 */
	lpxh = NULL;
    if (len > 0) {
        cb->s_want = cb->s_snxt;
        for (m = sb->sb_mb; m != NULL; m = m->m_act) {
						
            lpxh = mtod(m, struct lpxhdr *);
            if (SSEQ_LEQ(cb->s_snxt, lpxh->u.s.sequence)) {
                break;
			}
        }
		
        if (lpxh != NULL) {
            if (lpxh->u.s.sequence == cb->s_snxt)
                cb->s_snxt++;
            else
                lpx_stream_stat.smps_sndvoid++, lpxh = 0;
        }
    }
	
    if (lpxh != NULL) {
		/*
		 * must make a copy of this packet for
		 * lpx_USER_output to monkey with
		 */
		m = m_copy(dtom(lpxh), 0, (int)M_COPYALL);
		if (m == NULL) {
			return (ENOBUFS);
		}
		lpxh = mtod(m, struct lpxhdr *);
		lpxh->u.s.ackseq = cb->s_ack;
		
		if (SSEQ_LT(lpxh->u.s.sequence, cb->s_smax))
			lpx_stream_stat.smps_sndrexmitpack++;
		else
			lpx_stream_stat.smps_sndpack++;
    } else if (cb->s_force || cb->s_flags & SF_ACKNOW) {
        /*
         * Must send an acknowledgement or a probe
         */
        if (cb->s_force)
            lpx_stream_stat.smps_sndprobe++;
        if (cb->s_flags & SF_ACKNOW)
            lpx_stream_stat.smps_sndacks++;
        m = m_gethdr(M_DONTWAIT, MT_HEADER);
        if (m == NULL)
            return (ENOBUFS);
        /*
         * Fill in mbuf with extended SP header
         * and addresses and length put into network format.
         */
        MH_ALIGN(m, sizeof(struct lpxhdr));
        m->m_len = sizeof(struct lpxhdr);
        m->m_pkthdr.len = sizeof(struct lpxhdr);
        lpxh = mtod(m, struct lpxhdr *);
		bzero(lpxh, sizeof(*lpxh));

        lpxh->pu.pktsize = htons(LPX_PKT_LEN);
        lpxh->pu.p.type = LPX_TYPE_STREAM;
        lpxh->source_port = lpxp->lpxp_laddr.x_port;
        lpxh->dest_port = cb->s_dport;
        lpxh->u.s.ackseq = cb->s_ack;
		lpxh->u.s.tag = cb->s_tag;
		
        switch (cb->s_state) {
			case LPX_STREAM_SYN_SENT:
			{
				lpxh->u.s.lsctl = htons(LSCTL_CONNREQ | LSCTL_ACK);
				lpxh->u.s.sequence = cb->s_seq;
				lpxh->u.s.tag = 0;  // Init 0;
				break;
			}
				
			case LPX_STREAM_FIN_WAIT_1:
			case LPX_STREAM_LAST_ACK:
			case LPX_STREAM_CLOSING:
			{
				DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: Send DISCONN. state = %d\n", cb->s_state));
				lpxh->u.s.lsctl = htons(LSCTL_DISCONNREQ | LSCTL_ACK);
                lpxh->u.s.sequence = cb->s_fseq;
				
				break;
			}
				
			default:
			{
				lpxh->u.s.lsctl = LSCTL_ACK;
				if (cb->s_flags & SF_ACKREQ) {
					DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_output: ACKREQ\n"));

					lpxh->u.s.lsctl |= LSCTL_ACKREQ;
				}
				lpxh->u.s.lsctl = htons(lpxh->u.s.lsctl);
				lpxh->u.s.sequence = cb->s_smax + 1;
			}
				break;
        }
        
    } else {
        cb->s_outx = 3;
		
        return (0);
    }
    /*
     * Stuff checksum and output datagram.
     */
    if (ntohs(lpxh->u.s.lsctl) & LSCTL_DATA) {
        		
		//		if (cb->s_force != (1 + SMPT_PERSIST) ||
		//            cb->s_timer[SMPT_PERSIST] == 0) {
		/*
		 * If this is a new packet and we are not currently 
		 * timing anything, time this one.
		 */
		if (SSEQ_LT(cb->s_smax, lpxh->u.s.sequence)) {
			cb->s_smax = lpxh->u.s.sequence;
		}
		/*
		 * Set rexmt timer if not currently set,
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
		if (cb->s_timer[SMPT_REXMT] == 0 &&
			cb->s_snxt != cb->s_rack) {
			cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
			//                if (cb->s_timer[SMPT_PERSIST]) {
			//                    cb->s_timer[SMPT_PERSIST] = 0;
			//                    cb->s_rxtshift = 0;
			//                }
			//    }
		} else {
			if (SSEQ_LT(cb->s_smax, lpxh->u.s.sequence)) {
				cb->s_smax = lpxh->u.s.sequence;
			}
		}
    } else if (cb->s_state < LPX_STREAM_ESTABLISHED) {
        if (cb->s_timer[SMPT_REXMT] == 0) {
            cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
		}
        if (ntohs(lpxh->u.s.lsctl) & LSCTL_CONNREQ) {
            cb->s_smax = lpxh->u.s.sequence;
		}
    }
	
    {
        /*
         * Do not request acks when we ack their data packets or
         * when we do a gratuitous window update.
         */		
        DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("cb = %p, cb->s_state = %x, sequence = %x, ackseq = %x\n", cb, cb->s_state, lpxh->u.s.sequence, lpxh->u.s.ackseq));
        DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));
		
		lpxh->u.s.sequence = htons(lpxh->u.s.sequence);
        lpxh->u.s.ackseq = htons(lpxh->u.s.ackseq);
		
        if (so && so->so_options & SO_DONTROUTE) {
            error = lpx_output((mbuf_t)m, LPX_ROUTETOIF, lpxp);
            if (error) {
				DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_output: error(%d) in Lpx_OUT_outputfl m@%p, lpxp@%p\n",error,m,lpxp));
            }
        } else {
            error = lpx_output((mbuf_t)m, 0, lpxp);
            if (error) {
				DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_output: error(%d) in Lpx_OUT_outputfl m@%p\n",error,m));
            }
        }
    }
	
    if (error) {
        return (error);
    }
    
	lpx_stream_stat.smps_sndtotal++;
    /*
     * Data sent (as far as we can tell). 
     * If this advertises a larger window than any other segment,
     * then remember the size of the advertized window.
     * Any pending ACK has now been sent.
     */
    cb->s_force = 0;
    cb->s_flags &= ~(SF_ACKNOW|SF_DELACK);
	
    if (sendalot)
        goto again;
    cb->s_outx = 5;
	
    return (0);
}
/*
static void lpx_stream_setpersist(register struct stream_pcb *cb)
{
    register int t = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;

    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_setpersist: Entered.\n"));

    if (cb->s_timer[SMPT_REXMT] && lpx_stream_do_persist_panics)
        panic("lpx_stream_output REXMT");
    //
	// Start/restart persistance timer.
	//
    SMPT_RANGESET(cb->s_timer[SMPT_PERSIST],
        t*lpx_stream_backoff[cb->s_rxtshift],
        SMPTV_PERSMIN, SMPTV_PERSMAX);
    if (cb->s_rxtshift < SMP_MAXRXTSHIFT)
        cb->s_rxtshift++;
}
*/

/*
 * SMP timer processing.
 */
struct stream_pcb *lpx_stream_timers(register struct stream_pcb *cb, int timer)
{
    long rexmt;
    int win;
	
    DEBUG_PRINT(DEBUG_MASK_TIMER_TRACE, ("lpx_stream_timers: Entered.\n"));
	
	if (!cb) {
		DEBUG_PRINT(DEBUG_MASK_TIMER_ERROR, ("lpx_stream_timers: No cb!!!\n"));
		
		return NULL;
	}
	
	if (cb->s_state != LPX_STREAM_ESTABLISHED)
        DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers, timer = %d\n", timer));
	
    cb->s_force = 1 + timer;
    switch (timer) {
		
		/*
		 * 2 MSL timeout in shutdown went off.  TCP deletes connection
		 * control block.
		 */
		case SMPT_2MSL:
		{
			struct lpxpcb *lpxp = NULL;
			struct socket *so = NULL;
			
			DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: SMPT_2MSL went off for no reason flag 0x%x, cb = %p\n", cb->s_flags_control, cb));
			
			lpxp = cb->s_lpxpcb;
			if (!lpxp) {
				break;
			}
			
			so = lpxp->lpxp_socket;
			if (!so) {
				break;
			}
						
			cb->s_timer[timer] = 0;
			
			if ( cb->s_flags_control & SF_NEEDRELEASE) {
//				 && so->so_usecount == 1) {
				
				DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: SMPT_2MSL Release.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"));
				
				cb->s_flags_control &= ~SF_NEEDRELEASE;
				cb->s_lpxpcb->lpxp_socket = NULL;
				
				// Unlock. sock_release will lock. 
				lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
				lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
				
				// It will call sbappend and sorwakeup.
				sock_release((socket_t)so);
				
				lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
				lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
				
			} else {
				cb->s_timer[timer] = 1;
			}
			
			cb->s_flags_control |= SF_NEEDFREE;
			cb->s_flags &= ~SF_DELACK;		
		}
			break;
			
			/*
			 * Retransmission timer went off.  Message has not
			 * been acked within retransmit interval.  Back off
			 * to a longer retransmit interval and retransmit one packet.
			 */
		case SMPT_REXMT:
		{
			
			DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: SMPT_REXMT went off for no reason, cb->s_rxtshift = %d, SMP_MAXRXTSHIFT = %d, cb = %p\n", cb->s_rxtshift, SMP_MAXRXTSHIFT, cb));
			
			int maxRTshift;
			
			if (cb->s_state == LPX_STREAM_SYN_SENT) {
				maxRTshift = SMP_MAXRXTSHIFTOFCONNECT;
			} else {
				maxRTshift = SMP_MAXRXTSHIFT;
			}
			
			if (++cb->s_rxtshift > maxRTshift) {
				cb->s_rxtshift = maxRTshift;
				lpx_stream_stat.smps_timeoutdrop++;
				
				if (maxRTshift == SMP_MAXRXTSHIFT) {
					DEBUG_PRINT(DEBUG_MASK_TIMER_ERROR, ("lpx_stream_timer: End Retransmit. cb %p rxtshift %d\n", cb, cb->s_rxtshift));
				}
				
				goto dropit;
			}
			
			lpx_stream_stat.smps_rexmttimeo++;
			rexmt = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;
			rexmt *= lpx_stream_backoff[cb->s_rxtshift];
			SMPT_RANGESET(cb->s_rxtcur, rexmt, SMPTV_MIN, SMPTV_REXMTMAX);
			cb->s_rxtcur = SMPTV_MIN;
			DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: cb->s_rxtcur = %d\n", cb->s_rxtcur));
			cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
			/*
			 * If we have backed off fairly far, our srtt
			 * estimate is probably bogus.  Clobber it
			 * so we'll take the next rtt measurement as our srtt;
			 * move the current srtt into rttvar to keep the current
			 * retransmit times until then.
			 */
			if (cb->s_rxtshift > SMP_MAXRXTSHIFT / 4 ) {
				cb->s_rttvar += (cb->s_srtt >> 2);
				cb->s_srtt = 0;
			}
			cb->s_snxt = cb->s_rack;
			/*
			 * If timing a packet, stop the timer.
			 */
			cb->s_rtt = 0;
			/*
			 * See very long discussion in tcp_timer.c about congestion
			 * window and sstrhesh
			 */
			win = min(cb->s_swnd, (cb->s_cwnd/CUNIT)) / 2;
			if (win < 2) {
				win = 2;
			}
			
			cb->s_cwnd = CUNIT;
			cb->s_ssthresh = win * CUNIT;
			lpx_stream_output(cb, (struct mbuf *)NULL);
		}
			break;
			
			/*
			 * Persistance timer into zero window.
			 * Force a probe to be sent.
			 */
			/*   case SMPT_PERSIST:
			
			lpx_stream_stat.smps_persisttimeo++;
			lpx_stream_setpersist(cb);
			lpx_stream_output(cb, (struct mbuf *)NULL);
			
			break;
			*/
			/*
			 * Keep-alive timer went off; send something
			 * or drop connection if idle for too long.
			 */
		case SMPT_KEEP:
		{
			DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: cb->s_state = %d, LPX_STREAM_ESTABLISHED  = %d\n", cb->s_state, LPX_STREAM_ESTABLISHED));
			lpx_stream_stat.smps_keeptimeo++;
			if (cb->s_state < LPX_STREAM_ESTABLISHED) {
				
				DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("lpx_stream_timers: Keep alive. Not connected. cb 0x%p, status %d\n", cb, cb->s_state));

				goto dropit;
			}
			
			//DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("time = %x %x so_options = %d\n", time.tv_sec, time.tv_usec, cb->s_lpxpcb->lpxp_socket->so_options & SO_KEEPALIVE));
			
			if (cb->s_lpxpcb->lpxp_socket && cb->s_lpxpcb->lpxp_socket->so_options & SO_KEEPALIVE) {
				
				DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("cb->s_idle = %d, SMPTV_MAXIDLE = %d\n", cb->s_idle, SMPTV_MAXIDLE));
				
				if (cb->s_idle >= SMPTV_MAXIDLE) {
					DEBUG_PRINT(DEBUG_MASK_TIMER_ERROR, ("lpx_stream_timers: End Keep alive. cb->s_idle = %d, SMPTV_MAXIDLE = %d, cb = %p\n", cb->s_idle, SMPTV_MAXIDLE, cb));

					goto dropit;
				}
				
				lpx_stream_stat.smps_keepprobe++;
				cb->s_flags |= (SF_ACKNOW|SF_ACKREQ);
				
				lpx_stream_output(cb, (struct mbuf *)NULL);
				
				cb->s_flags &= ~(SF_ACKNOW|SF_ACKREQ);
				
				DEBUG_PRINT(DEBUG_MASK_TIMER_INFO, ("Sent Kepp Alive packet.\n"));
				
			} else {
				cb->s_idle = 0;
			}
			cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
		}
			break;
			
		case SMPT_DISCON:
		{
			DEBUG_PRINT(DEBUG_MASK_TIMER_ERROR, ("No reactions from sent DISCON packet.\n"));
			
			goto dropit;
		}
			break;
			
		dropit:
			lpx_stream_stat.smps_keepdrops++;
			cb = lpx_stream_drop(cb, ETIMEDOUT);
			break;
    }
	
    return (cb);
}

static int lpx_stream_reass(register struct stream_pcb *cb, register struct lpxhdr *lpxh)
{
	register struct lpx_stream_q *q;
	register struct mbuf *m;
	register struct socket *so = cb->s_lpxpcb->lpxp_socket;
//	char wakeup = 0;

	DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_reass: Entered.\n"));
	
	if (lpxh == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_reass: No lpxh.\n"));
		
		return -1;
	}

	if (so == NULL) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_reass: No Socket.\n"));
		
		return -1;
	}
	
	/*
	 * Update our news from them.
	 */
	if (ntohs(lpxh->u.s.lsctl) & LSCTL_DATA) {
		cb->s_flags |= (SF_ACKNOW);
	}
	
	cb->s_dupacks = 0;
	/*
	 * If our correspondent acknowledges data we haven't sent
	 * TCP would drop the packet after acking.  We'll be a little
	 * more permissive
	 */
	lpx_stream_stat.smps_rcvackpack++;

	/*
	 * If all outstanding data is acked, stop retransmit
	 * timer and remember to restart (more output or persist).
	 * If there is more data to be acked, restart retransmit
	 * timer, using current (possibly backed-off) value;
	 */
	//  if (si->si_ack == cb->s_smax + 1) {
	if (lpxh->u.s.ackseq == cb->s_smax + 1) {
		cb->s_timer[SMPT_REXMT] = 0;
		cb->s_flags |= SF_RXT;
	}/* else if (cb->s_timer[SMPT_PERSIST] == 0)
		cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
*/
	/*
	 * Trim Acked data from output queue.
	 */
	while ((m = so->so_snd.sb_mb) != NULL) {
		//          if (SSEQ_LT((mtod(m, struct smp *))->si_seq, si->si_ack))
		/*
				Add more condition codes to check the wrap around of sequence. 
		*/
		__u16	sequenceOfSentPacket = (mtod(m, struct lpxhdr *))->u.s.sequence;

		if (SSEQ_LT(sequenceOfSentPacket, lpxh->u.s.ackseq)
			&& ((lpxh->u.s.ackseq - sequenceOfSentPacket) < INT16_MAX) ) {
			cb->s_rxtshift = 0;
			sbdroprecord(&so->so_snd);
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: call drop record\n"));
			
		} else if (SSEQ_GT(sequenceOfSentPacket, lpxh->u.s.ackseq)
				   && ((sequenceOfSentPacket - lpxh->u.s.ackseq) > INT16_MAX) ) {
			cb->s_rxtshift = 0;
			sbdroprecord(&so->so_snd);
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: call drop record\n"));
		} else {
			break;
		}
	}
	
	sowwakeup(so);

	cb->s_rack = lpxh->u.s.ackseq;
	//update_window:
	if (SSEQ_LT(cb->s_snxt, cb->s_rack)) {
		cb->s_snxt = cb->s_rack;
	}

	//
	//If this is a system packet, we don't need to
	//queue it up, and won't update acknowledge #
	//
	if (ntohs(lpxh->u.s.lsctl) & LSCTL_DATA) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: With DATA flag\n"));
	} else {
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: No Data\n"));
		
		return (1);		
	}
	
	/* data packet from here */
	
	/*
	 * We have already seen this packet, so drop.
	 */
	if (SSEQ_LT(lpxh->u.s.sequence, cb->s_ack)) {
		lpx_stream_istat.bdreas++;
		lpx_stream_stat.smps_rcvduppack++;
		if (lpxh->u.s.sequence == cb->s_ack - 1)
			lpx_stream_istat.lstdup++;
		
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: Already received packet. packet seq %d, Expected seq %d\n", lpxh->u.s.sequence, cb->s_ack));

		return (1);
	}

	DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: Good sequence number.\n"));

	/*
	 * Loop through all packets queued up to insert in
	 * appropriate sequence.
	 */
	for (q = cb->s_q.si_next; q != &cb->s_q; q = q->si_next) {
		if (lpxh->u.s.sequence == ((struct lpxhdr *)q)->u.s.sequence) {
			lpx_stream_stat.smps_rcvduppack++;
			
			DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: Already received packet. packet seq %d, queued packet seq %d\n", lpxh->u.s.sequence, ((struct lpxhdr *)q)->u.s.sequence));

			return (1);
		}
		if (SSEQ_LT(lpxh->u.s.sequence, ((struct lpxhdr *)q)->u.s.sequence)) {
			lpx_stream_stat.smps_rcvoopack++;
			break;
		}
	}

	DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: sequence = %d, seq = %d\n",lpxh->u.s.sequence, cb->s_ack));
		
	// Insert Queue.
	insque(lpxh, q->si_prev);
	
present:
	/*
	 * Loop through all packets queued up to update acknowledge
	 * number, and present all acknowledged data to user;
	 * If in packet interface mode, show packet headers.
	 */
	for (q = cb->s_q.si_next; q != &cb->s_q; q = q->si_next) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: From queue. sequence = %x, seq = %x\n",((struct lpxhdr *)q)->u.s.sequence, cb->s_ack));
		
		if (((struct lpxhdr *)q)->u.s.sequence != cb->s_ack)
			continue;

		errno_t	error;
		
		cb->s_ack++;
		m = dtom(q);
		
		q = q->si_prev;
		remque(q->si_next);
		//			wakeup = 1;
		lpx_stream_stat.smps_rcvpack++;
		
		// Remove Lpx Header. 
		m_adj(m, sizeof(struct lpxhdr));
		
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: here 3 m->m_len = %d\n", (int)m->m_len));
		
		// Unlock. sock_inject_data_in will lock. 
		so->so_usecount++;
		
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
		
		// It will call sbappend and sorwakeup.
		error = sock_inject_data_in((socket_t)so, NULL, (mbuf_t)m, NULL, 0);
		
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
		
		so->so_usecount--;
		
		DEBUG_PRINT(DEBUG_MASK_STREAM_INFO, ("lpx_stream_reass: sock_inject_data_in return %d\n", error));			
	}
	
	return (0);
}

static struct stream_pcb *lpx_stream_close(struct stream_pcb *cb)
{
    struct lpxpcb *lpxp = NULL;
    struct socket *so = NULL;
 
    DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_close: Entered.\n"));

	if (!cb) {
		return NULL;
	}
	
	lpxp = cb->s_lpxpcb;
	if (!lpxp) {
		return NULL;
	}
		
	so = lpxp->lpxp_socket;
	if (!so) {
		return NULL;
	}
	
	if (cb->s_flags_control & SF_SOCKETFREED) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_close: Socket was freed already.\n"));
		return NULL;
	} else {
		cb->s_flags_control |= SF_SOCKETFREED;
	}
	
//	Lpx_PCB_disconnect(cb->s_lpxpcb);

	Lpx_PCB_detach(lpxp);
	
//	sofree(so);
			
//	so->so_flags |= SOF_PCBCLEARING;
	
	lpx_stream_stat.smps_closed++;
    	
	return ((struct stream_pcb *)NULL);
}


struct stream_pcb *lpx_stream_drop(struct stream_pcb *cb, int errno)
{
	struct lpxpcb *lpxp = NULL;
    struct socket *so = NULL;

	DEBUG_PRINT(DEBUG_MASK_STREAM_TRACE, ("lpx_stream_drop: Entered.\n"));

	if (!cb) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_drop: No cb\n"));

		return NULL;
	}
	
	lpxp = cb->s_lpxpcb;
	if (!lpxp) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_drop: No lpxp.\n"));

		return NULL;
	}
	
	so = lpxp->lpxp_socket;
	if (!so) {
		DEBUG_PRINT(DEBUG_MASK_STREAM_ERROR, ("lpx_stream_drop: No socket.\n"));

		return NULL;
	}
			 
   if (SMPS_HAVERCVDSYN(cb->s_state)) {
        cb->s_state = LPX_STREAM_CLOSED;
        (void) lpx_stream_output(cb, (struct mbuf *)NULL);
        lpx_stream_stat.smps_drops++;
   } else {
        lpx_stream_stat.smps_conndrops++;
   }
	
    if (errno == ETIMEDOUT && cb->s_softerror)
        errno = cb->s_softerror;
    so->so_error = errno;

	soisdisconnected(so);
	lpx_stream_close(cb);

	if ( cb->s_flags_control & SF_NEEDRELEASE) {
		cb->s_timer[SMPT_2MSL] = 1;
	} else { 
		cb->s_flags_control |= SF_NEEDFREE;
		cb->s_flags &= ~SF_DELACK;
	}
	
    return NULL;
}

int
lpx_stream_lock(struct socket *so, int refcount, int lr) {
	
	if (so->so_pcb) {
		lck_mtx_lock(((struct lpxpcb *)so->so_pcb)->lpxp_mtx);
	} else  {
		panic("lpx_stream_lock: so=%p NO PCB!\n", so);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
	}
	
	if (so->so_usecount < 0)
		panic("lpx_stream_lock: so=%p so_pcb=%p ref=%x\n",
			  so, so->so_pcb, so->so_usecount);
	
	if (refcount)
		so->so_usecount++;

	return (0);
}

int
lpx_stream_unlock(struct socket *so, int refcount, int lr) {
	
	if (refcount)
		so->so_usecount--;
	
	if (so->so_usecount < 0)
		panic("lpx_stream_unlock: so=%p usecount=%x\n", so, so->so_usecount);	
	if (so->so_pcb == NULL) {
		panic("lpx_stream_unlock: so=%p NO PCB usecount=%x\n", so, so->so_usecount);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);	
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
	}
	else {
		lck_mtx_assert(((struct lpxpcb *)so->so_pcb)->lpxp_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(((struct lpxpcb *)so->so_pcb)->lpxp_mtx);
	}
	
	return (0);
}

lck_mtx_t *
lpx_stream_getlock(struct socket *so, int locktype) {
	
	struct lpxpcb *lpxp = sotolpxpcb(so);
	
	if (so->so_pcb)  {
		if (so->so_usecount < 0)
			panic("lpx_stream_getlock: so=%p usecount=%x\n", so, so->so_usecount);	
		return(lpxp->lpxp_mtx);
	}
	else {
		panic("lpx_stream_getlock: so=%p NULL so_pcb\n", so);
		return (so->so_proto->pr_domain->dom_mtx);
	}
}