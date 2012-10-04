/***************************************************************************
 *
 *  smp_usrreq.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
//#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
//#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
//#include <sys/sx.h>
#include <sys/systm.h>

#include <net/route.h>
#include <netinet/tcp_fsm.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_pcb.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>
#include <netlpx/lpx_outputfl.h>
#include <netlpx/lpx_usrreq.h>
#include <netlpx/smp.h>
#include <netlpx/smp_usrreq_int.h>
#include <netlpx/smp_usrreq.h>
#include <netlpx/smp_debug.h>
#include <netlpx/smp_timer.h>
#include <netlpx/smp_var.h>
#include <netlpx/lpx_cksum.h>

#include <sys/proc.h>

#include <machine/spl.h>
#include <net/ethernet.h>
#include <string.h>


static struct   smp_istat smp_istat;
static u_short  smp_iss;
static int  traceallsmps = 0;
static struct   smp     smp_savesi;
static int  smp_use_delack = 0;

static u_short  smp_newchecks[50];
static int smp_backoff[SMP_MAXRXTSHIFT+1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };
static int smp_do_persist_panics = 0;


#define SMPDEBUG0
#define SMPDEBUG1()
#define SMPDEBUG2(req)

#define COMMON_START()  SMPDEBUG0; \
            do { \
                     if (lpxp == 0) { \
                         splx(s); \
                         return EINVAL; \
                     } \
                     cb = lpxtosmppcb(lpxp); \
                     SMPDEBUG1(); \
             } while(0)
                 
#define COMMON_END(req) out: SMPDEBUG2(req); splx(s); return error; goto out

#define SMPS_HAVERCVDSYN(s) ((s) >= TCPS_SYN_RECEIVED)

#define SPINC sizeof(struct smphdr)

/* Following was struct smpstat smpstat; */
#ifndef smpstat 
#define smpstat smp_istat.newstats
#endif  

struct  pr_usrreqs smp_usrreqs = {
    smp_usr_abort, smp_usr_accept, smp_usr_attach, smp_usr_bind,
    smp_usr_connect, pru_connect2_notsupp, Lpx_CTL_control, smp_usr_detach,
    smp_usr_disconnect, smp_usr_listen, Lpx_USER_peeraddr, smp_usr_rcvd,
    smp_usr_rcvoob, smp_usr_send, pru_sense_null, smp_usr_shutdown,
    Lpx_USER_sockaddr, sosend, soreceive, sopoll
};


/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

void Smp_init()
{
    DEBUG_CALL(4, ("Smp_init\n"));

    smp_iss = 1; /* WRONG !! should fish it out of TODR */
}


int Smp_ctloutput( struct socket *so, struct sockopt *sopt )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
    register struct smppcb *cb;
    int mask, error;
    short soptval;
    u_short usoptval;
    int optval;

    DEBUG_CALL(4, ("Smp_ctloutput\n"));

    error = 0;

    if (sopt->sopt_level != LPXPROTO_SMP) {
        /* This will have to be changed when we do more general
           stacking of protocols */
        return (Lpx_USER_ctloutput(so, sopt));
    }
    if (lpxp == NULL)
        return (EINVAL);
    else
        cb = lpxtosmppcb(lpxp);

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
                smp_newchecks[5]++;
            } else {
                cb->s_flags2 &= ~SF_NEWCALL;
                smp_newchecks[6]++;
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
                cb->s_dt = sp.smp_dt;
                cb->s_cc = sp.smp_cc & SMP_EM;
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

    DEBUG_CALL(4, ("smp_ctlinput\n"));

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


void Smp_fasttimo()
{
    register struct lpxpcb *lpxp;
    register struct smppcb *cb;
    int s = splnet();

    DEBUG_CALL(6, ("smp_fasttimo\n"));

    lpxp = lpxpcb.lpxp_next;
    if (lpxp != NULL) {
      for (; lpxp != &lpxpcb; lpxp = lpxp->lpxp_next) {
        if ((cb = (struct smppcb *)lpxp->lpxp_pcb) != NULL &&
            (cb->s_flags & SF_DELACK)) {
            cb->s_flags &= ~SF_DELACK;
            cb->s_flags |= SF_ACKNOW;
            smpstat.smps_delack++;
            smp_output(cb, (struct mbuf *)NULL);
        }
      }
    }
    splx(s);
}

/*
 * smp protocol timeout routine called every 500 ms.
 * Updates the timers in all active pcb's and
 * causes finite state machine actions if timers expire.
 */
void Smp_slowtimo()
{
    register struct lpxpcb *lpxp, *lpxpnxt;
    register struct smppcb *cb;
    int s = splnet();
    register int i;

    DEBUG_CALL(6, ("smp_slowtimo\n"));

    /*
     * Search through tcb's and update active timers.
     */
    lpxp = lpxpcb.lpxp_next;
    if (lpxp == NULL) {
        splx(s);
        return;
    }
    while (lpxp != &lpxpcb) {
        cb = lpxtosmppcb(lpxp);
        lpxpnxt = lpxp->lpxp_next;
        if (cb == NULL)
            goto tpgone;
        for (i = 0; i < SMPT_NTIMERS; i++) {
//            if(i == SMPT_KEEP)
//                DEBUG_CALL(2, ("cb = %p time = %d\n", cb, cb->s_timer[SMPT_KEEP]));
            if (cb->s_timer[i] && --cb->s_timer[i] == 0) {
                smp_timers(cb, i);
                if (lpxpnxt->lpxp_prev != lpxp)
                    goto tpgone;
            }
        }
        cb->s_idle++;
        if (cb->s_rtt)
            cb->s_rtt++;
tpgone:
        lpxp = lpxpnxt;
    }
    smp_iss += SMP_ISSINCR/PR_SLOWHZ;       /* increment iss */
    splx(s);
}


void Smp_input( register struct mbuf *m, 
                register struct lpxpcb *lpxp,
                void *frame_header)
{
    register struct smppcb *cb;
//  register struct smp *si = mtod(m, struct smp *);
    register struct lpxhdr *lpxh = mtod(m, struct lpxhdr *);
    register struct socket *so;
    int dropsocket = 0;
    short ostate = 0;
    struct ether_header *eh;

    DEBUG_CALL(5, ("smp_input\n"));
    
    smpstat.smps_rcvtotal++;
    if (lpxp == NULL) {
        panic("No lpxpcb in smp_input\n");
        return;
    }

    DEBUG_CALL(5, ("smp_input 2\n"));
    cb = lpxtosmppcb(lpxp);
    if (cb == NULL)
        goto bad;

    DEBUG_CALL(4, ("smp_input, cb->s_state = %x\n", cb->s_state));
    DEBUG_CALL(4, ("lpxp->lpxp_laddr.x_port = %x\n", lpxp->lpxp_laddr.x_port));
    DEBUG_CALL(4, ("lpxh->dest_port = %x, lpxh->u.s.sequence = %x, lpxh->u.s.ackseq = %x, cb->s_rack = %x, cb->s_seq = %x, cb->s_ack = %x\n", lpxh->dest_port, lpxh->u.s.sequence, lpxh->u.s.ackseq, cb->s_rack, cb->s_seq, cb->s_ack));
    DEBUG_CALL(4, ("ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));
    
#if 0
    eh = (struct ether_header *) m->m_pktdat;
#else /* if 0 */
    eh = (struct ether_header *)frame_header;
#endif /* if 0 else */

#if 0        
    if (m->m_len < sizeof(*si)) {
        if ((m = m_pullup(m, sizeof(*si))) == NULL) {
            smpstat.smps_rcvshort++;
            return;
        }
        si = mtod(m, struct smp *);
    }
    si->si_seq = ntohs(si->si_seq);
    si->si_ack = ntohs(si->si_ack);
    si->si_alo = ntohs(si->si_alo);
#endif
    if ((unsigned long)m->m_len < sizeof(*lpxh)) {
        if ((m = m_pullup(m, sizeof(*lpxh))) == NULL) {
            smpstat.smps_rcvshort++;
            return;
        }
        lpxh = mtod(m, struct lpxhdr *);
    }
    
    lpxh->u.s.sequence = ntohs(lpxh->u.s.sequence);
    lpxh->u.s.ackseq = ntohs(lpxh->u.s.ackseq);

    so = lpxp->lpxp_socket;
    DEBUG_CALL(2, ("Smp_input to so@%lx\n",so));
    DEBUG_CALL(2, ("smp_input, cb->s_state = %x\n", cb->s_state));
    DEBUG_CALL(2, ("lpxp->lpxp_laddr.x_port = %x\n", lpxp->lpxp_laddr.x_port));
    DEBUG_CALL(2, ("lpxh->dest_port = %x, lpxh->u.s.sequence = %x, lpxh->u.s.ackseq = %x, cb->s_rack = %x, cb->s_seq = %x, cb->s_ack = %x\n", lpxh->dest_port, lpxh->u.s.sequence, lpxh->u.s.ackseq, cb->s_rack, cb->s_seq, cb->s_ack));
    DEBUG_CALL(2, ("ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));

#if 0
    if (so->so_options & SO_DEBUG || traceallsmps) {
        ostate = cb->s_state;
        smp_savesi = *si;
    }
#endif        
    if (so->so_options & SO_ACCEPTCONN) {
        struct smppcb *ocb = cb;

        so = sonewconn(so, 0);
        if (so == NULL) {
            goto drop;
        }
      
        /*
         * This is ugly, but ....
         *
         * Mark socket as temporary until we're
         * committed to keeping it.  The code at
         * ``drop'' and ``dropwithreset'' check the
         * flag dropsocket to see if the temporary
         * socket created here should be discarded.
         * We mark the socket as discardable until
         * we're committed to it below in TCPS_LISTEN.
         */
        
        dropsocket++;
        lpxp = (struct lpxpcb *)so->so_pcb;
//      lpxp->lpxp_laddr = si->si_dna;
        bcopy(eh->ether_dhost, lpxp->lpxp_laddr.x_node, LPX_NODE_LEN);
        lpxp->lpxp_laddr.x_port = ntohs(lpxh->dest_port);
                
        cb = lpxtosmppcb(lpxp);
        cb->s_mtu = ocb->s_mtu;     /* preserve sockopts */
        cb->s_flags = ocb->s_flags; /* preserve sockopts */
        cb->s_flags2 = ocb->s_flags2;   /* preserve sockopts */
        cb->s_state = TCPS_LISTEN;
    }

    /*
     * Packet received on connection.
     * reset idle time and keep-alive timer;
     */
    cb->s_idle = 0;
    cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;

    switch (cb->s_state) {

    case TCPS_LISTEN:{
        struct sockaddr_lpx *slpx, sslpx;
        struct lpx_addr laddr;
        struct proc *proc0 = current_proc();

        /*
         * If somebody here was carying on a conversation
         * and went away, and his pen pal thinks he can
         * still talk, we get the misdirected packet.
         */
#if 0
        if (smp_hardnosed && (si->si_did != 0 || si->si_seq != 0)) {
            smp_istat.gonawy++;
            goto dropwithreset;
        }
#endif
        slpx = &sslpx;
        bzero(slpx, sizeof *slpx);
        slpx->slpx_len = sizeof(*slpx);
        slpx->slpx_family = AF_LPX;
//      slpx->sipx_addr = si->si_sna;
        bcopy(eh->ether_shost, slpx->slpx_node, LPX_NODE_LEN);
        slpx->slpx_port = ntohs(lpxh->source_port);
        laddr = lpxp->lpxp_laddr;
        if (lpx_nullhost(laddr)) {
//          lpxp->lpxp_laddr = si->si_dna;
            bcopy(eh->ether_dhost, lpxp->lpxp_laddr.x_node, LPX_NODE_LEN);
            lpxp->lpxp_laddr.x_port = ntohs(lpxh->dest_port);
        }
                
        if (Lpx_PCB_connect(lpxp, (struct sockaddr *)slpx, proc0)) {
            lpxp->lpxp_laddr = laddr;
            smp_istat.noconn++;
            goto drop;
        }
               
        smp_template(cb);
        dropsocket = 0;     /* committed to socket */
//      cb->s_did = si->si_sid;
//      cb->s_rack = si->si_ack;
//      cb->s_ralo = si->si_alo;
                cb->s_rack = lpxh->u.s.ackseq;
#define THREEWAYSHAKE
#ifdef THREEWAYSHAKE
        cb->s_state = TCPS_SYN_RECEIVED;
        cb->s_force = 1 + SMPT_KEEP;
        smpstat.smps_accepts++;
        cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
        }
        break;
    /*
     * This state means that we have heard a response
     * to our acceptance of their connection
     * It is probably logically unnecessary in this
     * implementation.
     */
     case TCPS_SYN_RECEIVED: {
//      if (si->si_did != cb->s_sid) {
//          smp_istat.wrncon++;
//          goto drop;
//      }
#endif
//      lpxp->lpxp_fport =  si->si_sport;
			// Tag
		 cb->s_tag = lpxh->u.s.tag;
             lpxp->lpxp_fport = lpxh->source_port; 
             cb->s_timer[SMPT_REXMT] = 0;
             cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
             soisconnected(so);
             cb->s_state = TCPS_ESTABLISHED;
             smpstat.smps_accepts++;
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
    case TCPS_SYN_SENT: {
//  if (si->si_did != cb->s_sid) {
//          smp_istat.notme++;
//          goto drop;
//      }
        smpstat.smps_connects++;
//      cb->s_did = si->si_sid;
//      cb->s_rack = si->si_ack;
//      cb->s_ralo = si->si_alo;
//      cb->s_dport = lpxp->lpxp_fport =  si->si_sport;
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
        cb->s_state = TCPS_ESTABLISHED;
        cb->s_lpxpcb->lpxp_socket->so_options |= SO_KEEPALIVE;
        cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
        DEBUG_CALL(4, ("cb = %p time = %d\n", cb, cb->s_timer[SMPT_KEEP]));
#if 0
        /* Use roundtrip time of connection request for initial rtt */
        if (cb->s_rtt) {
            cb->s_srtt = cb->s_rtt << 3;
            cb->s_rttvar = cb->s_rtt << 1;
            SMPT_RANGESET(cb->s_rxtcur,
                        ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
                        SMPTV_MIN, SMPTV_REXMTMAX);
            cb->s_rtt = 0;
        }
#endif
        break;
    }
    
    case TCPS_ESTABLISHED: {
        switch (ntohs(lpxh->u.s.lsctl)) {
         case LSCTL_DISCONNREQ:
         case LSCTL_DISCONNREQ | LSCTL_ACK:
            cb->s_ack ++;
            cb->s_flags |= SF_ACKNOW;
            cb->s_state = TCPS_CLOSE_WAIT;
            break;
        case LSCTL_ACKREQ:
        case LSCTL_ACKREQ | LSCTL_ACK:
           cb->s_flags |= SF_ACKNOW;
        }
        
        break;
    }
    
    case TCPS_LAST_ACK:

        DEBUG_CALL(4, ("TCPS_LAST_ACK\n"));
        
        if(ntohs(lpxh->u.s.lsctl) == LSCTL_ACK) {
            cb->s_state = TCPS_TIME_WAIT; 
            cb->s_timer[SMPT_2MSL] = 1;
        }
        
        break;
    
    case TCPS_FIN_WAIT_1: {

        DEBUG_CALL(4, ("TCPS_FIN_WAIT_1, ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));

        switch(ntohs(lpxh->u.s.lsctl)) {
    
        case LSCTL_DATA:
        case LSCTL_DATA|LSCTL_ACK:
    
        if(!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK))
            break;
        
        case LSCTL_ACK:
            if(lpxh->u.s.ackseq == (cb->s_fseq+1)) {
                cb->s_seq++;          
                cb->s_state = TCPS_FIN_WAIT_2;
            }
            
            break;

        case LSCTL_DISCONNREQ:
        case LSCTL_DISCONNREQ|LSCTL_ACK:

            cb->s_ack ++;
            cb->s_flags |= SF_ACKNOW;
           
            cb->s_state = TCPS_CLOSING;
            
            if(!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK))
                break;
           
            
            if(lpxh->u.s.ackseq == (cb->s_seq+1)) {
                cb->s_seq++;
                cb->s_state = TCPS_TIME_WAIT; 
                cb->s_timer[SMPT_2MSL] = 1;
            }
        }
        break;
    }
    
    case TCPS_FIN_WAIT_2: {
        DEBUG_CALL(4, ("TCPS_FIN_WAIT_2\n"));

        switch(ntohs(lpxh->u.s.lsctl)) 
        {
        case LSCTL_DATA:
        case LSCTL_DATA|LSCTL_ACK:
    
            break;
        
        case LSCTL_DISCONNREQ:
        case LSCTL_DISCONNREQ|LSCTL_ACK:
        
            cb->s_ack ++;
            cb->s_flags |= SF_ACKNOW;
            
            if(!(ntohs(lpxh->u.s.lsctl) & LSCTL_ACK))
                break;
           
//            if(lpxh->u.s.ackseq == (cb->s_fseq+1)) {
                cb->s_state = TCPS_TIME_WAIT; 
                cb->s_timer[SMPT_2MSL] = 1;
//            }
            break;
        }
    }

    }
    
    
    if (so->so_options & SO_DEBUG || traceallsmps)
        Smp_trace(SA_INPUT, (u_char)ostate, cb, &smp_savesi, 0);

//  m->m_len -= sizeof(struct lpx);
//  m->m_pkthdr.len -= sizeof(struct lpx);
//  m->m_data += sizeof(struct lpx);

    if(ntohs(lpxh->u.s.lsctl) & LSCTL_ACKREQ)
        cb->s_flags |= (smp_use_delack ? SF_DELACK : SF_ACKNOW);

//      if (smp_reass(cb, si)) {
    if (smp_reass(cb, lpxh)) {
        m_freem(m);
    }
    
    if (cb->s_force || (cb->s_flags & (SF_ACKNOW|SF_WIN|SF_RXT)))
        smp_output(cb, (struct mbuf *)NULL);
    cb->s_flags &= ~(SF_WIN|SF_RXT);
    return;

//dropwithreset:
    if (dropsocket)
        soabort(so);
//  si->si_seq = ntohs(si->si_seq);
//  si->si_ack = ntohs(si->si_ack);
//  si->si_alo = ntohs(si->si_alo);
        lpxh->u.s.sequence = ntohs(lpxh->u.s.sequence);
        lpxh->u.s.ackseq = ntohs(lpxh->u.s.ackseq);
//      m_freem(dtom(si));
    m_freem(dtom(lpxh));
    if (cb->s_lpxpcb->lpxp_socket->so_options & SO_DEBUG || traceallsmps)
        Smp_trace(SA_DROP, (u_char)ostate, cb, &smp_savesi, 0);
    return;

drop:
bad:
    if (cb == 0 || cb->s_lpxpcb->lpxp_socket->so_options & SO_DEBUG ||
            traceallsmps)
        Smp_trace(SA_DROP, (u_char)ostate, cb, &smp_savesi, 0);
    m_freem(m);
}


/***************************************************************************
 *
 *  Internal functions
 *
 **************************************************************************/

static int smp_usr_attach(struct socket *so, int proto, struct proc *p)
{
    int s = splnet();
    int error;
    struct lpxpcb *lpxp = sotolpxpcb(so);

    DEBUG_CALL(2, ("smp_usr_attach with so@%lx: lpxp@%lx\n",so,lpxp));

    if (lpxp) {
        error = EISCONN;
        goto out;
    }

    error = smp_attach(so, p);
    if (error)
        goto out;

out:
    splx(s);
    return error;
}


static int smp_usr_detach(struct socket *so)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_detach\n"));

    if (lpxp == 0) {
        splx(s);
        return EINVAL;  /* XXX */
    }
    cb = lpxtosmppcb(lpxp);
    /* In case we got disconnected from the peer */
    if (cb == 0) 
        goto out;

    cb = smp_disconnect(cb);
out:
    splx(s);
    return error;
}



static int smp_usr_bind( struct socket *so,
                         struct sockaddr *nam,
                         struct proc *p )
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;

    if(slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
    
    DEBUG_CALL(4, ("smp_usr_bind\n"));

    COMMON_START();

    error = Lpx_PCB_bind(lpxp, nam, p);
    if (error) {
        DEBUG_CALL(0,("!!!smp_usr_bind: Bind failure!!!\n"));
        goto out;
    }
    COMMON_END(PRU_BIND);

}


static int smp_usr_listen(struct socket *so, struct proc *p)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_listen\n"));

    COMMON_START();
    if (lpxp->lpxp_lport == 0)
        error = Lpx_PCB_bind(lpxp, (struct sockaddr *)0, p);
    if (error == 0) {
        cb->s_state = TCPS_LISTEN;
    } else {
        DEBUG_CALL(0,("!!!smp_usr_listen: Bind failure!!!\n"));
    }
    COMMON_END(PRU_LISTEN);
}


static int smp_usr_connect( struct socket *so,
                            struct sockaddr *nam,
                            struct proc *td )
{
    int error;
    int s;
    struct lpxpcb *lpxp;
    struct smppcb *cb;
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;

    if(slpx)
        bzero(&slpx->sipx_addr.x_net, sizeof(slpx->sipx_addr.x_net));
                  
    lpxp = sotolpxpcb(so);
    cb = lpxtosmppcb(lpxp);

    DEBUG_CALL(4, ("smp_usr_connect\n"));

    s = splnet();
    if (lpxp->lpxp_lport == 0) {
#if 0
        error = Lpx_PCB_bind(lpxp, (struct sockaddr *)NULL, td);
#else /* if 0 */
        error = Lpx_PCB_bind(lpxp, (struct sockaddr *)nam, td);
#endif /* if 0 else */
        if (error) {
            DEBUG_CALL(0,("!!!smp_usr_connect: Bind failure!!!\n"));
            goto smp_connect_end;
        }
    }

    error = Lpx_PCB_connect(lpxp, nam, td);

    if (error)
        goto smp_connect_end;

    
    soisconnecting(so);
    smpstat.smps_connattempt++;
    cb->s_state = TCPS_SYN_SENT;
    cb->s_did = 0;
    smp_template(cb);
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
    DEBUG_CALL(2, ("smp_usr_connect for lpxp@%lx, lpxp->lpxp_laddr.x_port = %x\n",
           lpxp, lpxp->lpxp_laddr.x_port));
    error = smp_output(cb, (struct mbuf *)NULL);
smp_connect_end:
    splx(s);
    return (error);
}


static int smp_usr_disconnect(struct socket *so)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_disconnect\n"));

    COMMON_START();
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    cb = smp_disconnect(cb);
    COMMON_END(PRU_DISCONNECT);
}


static int smp_usr_accept(struct socket *so, struct sockaddr **nam)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb = NULL;
    struct sockaddr_lpx *slpx, sslpx;


    DEBUG_CALL(4, ("smp_usr_accept\n"));

    if (so->so_state & SS_ISDISCONNECTED) {
        error = ECONNABORTED;
        goto out;
    }
    if (lpxp == 0) {
        splx(s);
        return (EINVAL);
    }
    cb = lpxtosmppcb(lpxp);

    slpx = &sslpx;
    bzero(slpx, sizeof *slpx);
    slpx->slpx_len = sizeof *slpx;
    slpx->slpx_family = AF_LPX;
    slpx->sipx_addr = lpxp->lpxp_faddr;
    *nam = dup_sockaddr((struct sockaddr *)slpx, 0);

    COMMON_END(PRU_ACCEPT);
}


static int smp_usr_shutdown(struct socket *so)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_shutdown\n"));

    COMMON_START();
    socantsendmore(so);
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    cb = smp_usrclosed(cb);
    if (cb)
        error = smp_output(cb, (struct mbuf *)NULL);

    COMMON_END(PRU_SHUTDOWN);
}


static int smp_usr_rcvd(struct socket *so, int flags)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_rcvd\n"));

    COMMON_START();
        /* In case we got disconnected from the peer */
        if (cb == 0)
            goto out;
    smp_output(cb, (struct mbuf *)NULL);
    COMMON_END(PRU_RCVD);
}


static int smp_usr_send( struct socket *so,
                         int flags,
                         struct mbuf *m,
                         struct sockaddr *addr,
                         struct mbuf *controlp,
                         struct proc *td)
{
    int error;
    int s;
    struct lpxpcb *lpxp;
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_send\n"));

    error = 0;
    lpxp = sotolpxpcb(so);
    cb = lpxtosmppcb(lpxp);

    s = splnet();
    if (flags & PRUS_OOB) {
        if (sbspace(&so->so_snd) < -512) {
            error = ENOBUFS;
            DEBUG_CALL(0,("smp_usr_send: error(%d) in sbspace so@%lx, so_snd @ %lx\n",error,so, so->so_snd));
            goto smp_send_end;
        }
        cb->s_oobflags |= SF_SOOB;
    }
    if (controlp != NULL) {
        u_short *p = mtod(controlp, u_short *);
        smp_newchecks[2]++;
        if ((p[0] == 5) && (p[1] == 1)) { /* XXXX, for testing */
            cb->s_shdr.smp_dt = *(u_char *)(&p[2]);
            smp_newchecks[3]++;
        }
        m_freem(controlp);
    }
    controlp = NULL;
    error = smp_output(cb, m);
    if (error) {
        DEBUG_CALL(0,("smp_usr_send: error(%d) in smp_output so@%lx, cb@%lx, m@%lx\n",error,so,cb,m));
//      debug_level=4;
    }
    m = NULL;
smp_send_end:
    if (controlp != NULL)
        m_freem(controlp);
    if (m != NULL)
        m_freem(m);
    splx(s);
    return (error);
}


static int smp_usr_abort(struct socket *so)
{
    int s = splnet();
    int error = 0;
    struct lpxpcb *lpxp = sotolpxpcb(so);
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_abort\n"));

    COMMON_START();
        /* In case we got disconnected from the peer */
    if (cb == 0)
        goto out;
    cb = smp_drop(cb, ECONNABORTED);
    COMMON_END(PRU_ABORT);
}


static int smp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
    struct lpxpcb *lpxp;
    struct smppcb *cb;

    DEBUG_CALL(4, ("smp_usr_rcvoob\n"));

    lpxp = sotolpxpcb(so);
    cb = lpxtosmppcb(lpxp);

    if ((cb->s_oobflags & SF_IOOB) || so->so_oobmark ||
        (so->so_state & SS_RCVATMARK)) {
        m->m_len = 1;
        *mtod(m, caddr_t) = cb->s_iobc;
        return (0);
    }
    return (EINVAL);
}


static int smp_attach( struct socket *so, struct proc *td )
{
    int error;
    int s;
    struct lpxpcb *lpxp = NULL;
    struct smppcb *cb;
    struct mbuf *mm;
    struct sockbuf *sb;

    DEBUG_CALL(4, ("smp_attach\n"));

    lpxp = sotolpxpcb(so);
//    cb = lpxtosmppcb(lpxp);

    if (lpxp != NULL)
        return (EISCONN);

    s = splnet();

    error = Lpx_PCB_alloc(so, &lpxpcb, td);

    if (error) {
        goto smp_attach_end;
    }

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
        int smpsends, smprecvs;
                            
        smpsends = 256 * 1024;
        smprecvs = 256 * 1024;

        while(smpsends > 0 && smprecvs > 0) {
            error = soreserve(so, smpsends, smprecvs);
            if(error == 0)
                break;
            
            smpsends -= (1024*2);
            smprecvs -= (1024*2);
        }

        DEBUG_CALL(4, ("smpsends = %d\n", smpsends/1024));
    
        // error = soreserve(so, (u_long) 3072, (u_long) 3072);
        if (error)
            goto smp_attach_end;
    }
        
    lpxp = sotolpxpcb(so);


    MALLOC(cb, struct smppcb *, sizeof *cb, M_PCB, M_WAITOK);

    if (cb == NULL) {
        error = ENOBUFS;
        goto smp_attach_end;
    }

    bzero(cb, sizeof(*cb));
      
    sb = &so->so_snd;

    mm = m_getclr(M_DONTWAIT, MT_HEADER);
    if (mm == NULL) {
        FREE(cb, M_PCB);
        error = ENOBUFS;
        goto smp_attach_end;
    }
    cb->s_lpx = mtod(mm, struct lpx *);
//    cb->s_state = TCPS_LISTEN;
    cb->s_state = TCPS_CLOSED;
    cb->s_smax = -1;
    cb->s_swl1 = -1;
    cb->s_q.si_next = cb->s_q.si_prev = &cb->s_q;
    cb->s_lpxpcb = lpxp;
    cb->s_mtu = min_mtu - sizeof(struct smp);
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

    DEBUG_CALL(4, ("cb->s_rxtcur = %d\n", cb->s_rxtcur));
    cb->s_rxtcur = 1;
    
    lpxp->lpxp_pcb = (caddr_t)cb; 
    DEBUG_CALL(2, ("smp_attach with so@%lx: ==> lpxp@%lx : pcb@%lx\n",so,lpxp,lpxp->lpxp_pcb));
smp_attach_end:
    splx(s);

    return (error);
}


static struct smppcb *smp_disconnect( register struct smppcb *cb )
{
    struct socket *so = cb->s_lpxpcb->lpxp_socket;

    DEBUG_CALL(4, ("smp_disconnect\n"));

    if (cb->s_state < TCPS_ESTABLISHED)
        cb = smp_close(cb);
    else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
        cb = smp_drop(cb, 0);
    else {
        soisdisconnecting(so);
        sbflush(&so->so_rcv);
        cb = smp_usrclosed(cb);
        if (cb) {
            (void) smp_output(cb, (struct mbuf *)NULL);
        }
    }
    return (cb);
}


static struct smppcb *smp_usrclosed(register struct smppcb *cb)
{

    DEBUG_CALL(4, ("smp_usrclosed\n"));

    switch (cb->s_state) {

    case TCPS_CLOSED:
    case TCPS_LISTEN:
        cb->s_state = TCPS_CLOSED;
        cb = smp_close(cb);
        break;

    case TCPS_SYN_SENT:
    case TCPS_SYN_RECEIVED:
        cb->s_flags |= SF_NEEDFIN;
        break;

    case TCPS_ESTABLISHED:
        cb->s_state = TCPS_FIN_WAIT_1;
        cb->s_fseq = cb->s_seq;
        break;

    case TCPS_CLOSE_WAIT: 
        cb->s_state = TCPS_LAST_ACK;
        cb->s_fseq = cb->s_seq;
        break;
    }
    if (cb && cb->s_state >= TCPS_FIN_WAIT_2) {
        soisdisconnected(cb->s_lpxpcb->lpxp_socket);
        /* To prevent the connection hanging in FIN_WAIT_2 forever. */
        if (cb->s_state == TCPS_FIN_WAIT_2)
            cb->s_timer[SMPT_2MSL] = SMPTV_MAXIDLE;
    }
    return (cb);
}


static void smp_template(register struct smppcb *cb)
{
    register struct lpxpcb *lpxp = cb->s_lpxpcb;
    register struct lpx *lpx = cb->s_lpx;
    register struct sockbuf *sb = &(lpxp->lpxp_socket->so_snd);

    DEBUG_CALL(4, ("smp_template\n"));

    lpx->lpx_pt = LPXPROTO_SMP;
    lpx->lpx_sna = lpxp->lpxp_laddr;
    lpx->lpx_dna = lpxp->lpxp_faddr;
    cb->s_sid = htons(smp_iss);
    smp_iss += SMP_ISSINCR/2;
    cb->s_alo = 1;
    cb->s_cwnd = (sbspace(sb) * CUNIT) / cb->s_mtu;
    cb->s_ssthresh = cb->s_cwnd; /* Try to expand fast to full complement
                    of large packets */
    cb->s_cwmx = (sbspace(sb) * CUNIT) / (2 * sizeof(struct smp));
    cb->s_cwmx = max(cb->s_cwmx, cb->s_cwnd);
        /* But allow for lots of little packets as well */
}



static int smp_output(register struct smppcb *cb, struct mbuf *m0)
{
    struct socket *so = cb->s_lpxpcb->lpxp_socket;
    register struct mbuf *m;
//  register struct smp *si = (struct smp *)NULL;
    register struct lpxhdr *lpxh = (struct lpxhdr *)NULL;
    register struct sockbuf *sb = &so->so_snd;
    int len = 0, win/*, rcv_win*/;
    short /*span, off, */recordp = 0;
//  u_short alo;
    int error = 0, sendalot;
    struct lpxpcb *lpxp = smptolpxpcb(cb);


#ifdef notdef
    int idle;
#endif
    struct mbuf *mprev;

    DEBUG_CALL(2, ("smp_output to so@%lx pcb@%lx\n",so, cb));

    if (m0 != NULL) {
        int mtu = cb->s_mtu;
        int datalen;
        /*
         * Make sure that packet isn't too big.
         */
        for (m = m0; m != NULL; m = m->m_next) {
            mprev = m;
            len += m->m_len;
            if (m->m_flags & M_EOR)
                recordp = 1;
        }
        datalen = (cb->s_flags & SF_HO) ?
                len - (int)(sizeof(struct smphdr)) : len;
        if (datalen > mtu) {
            if (cb->s_flags & SF_PI) {
                m_freem(m0);
                return (EMSGSIZE);
            } else {
                int oldEM = cb->s_cc & SMP_EM;

                cb->s_cc &= ~SMP_EM;
                while (len > mtu) {
                    /*
                     * Here we are only being called
                     * from usrreq(), so it is OK to
                     * block.
                     */
                    m = m_copym(m0, 0, mtu, M_WAITOK);

                    if (cb->s_flags & SF_NEWCALL) {
                        struct mbuf *mm = m;
                        smp_newchecks[7]++;
                        while (mm != NULL) {
                        mm->m_flags &= ~M_EOR;
                        mm = mm->m_next;
                        }
                    }
                    error = smp_output(cb, m);
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
            if (M_TRAILINGSPACE(m) >= 1)
                m->m_len++;
            else {
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
			
			DEBUG_CALL(4, ("lpx_stream_output: Adding padding to support NDAS chip 2.0 len = %d\n", len));
			
			m = m0;
			
			while(m->m_next) {
				m = m->m_next;
			}
			
			if (!(m->m_flags & M_EXT)
				&& M_TRAILINGSPACE(m) >= paddingLen) {
				m->m_len += paddingLen;
				
				padding = mtod(m, caddr_t) + m->m_len;
				
				m0->m_pkthdr.len += paddingLen;
				
			} else {
				DEBUG_CALL(4, ("lpx_stream_output: M_TRAILINGSPACE len = %d\n", M_TRAILINGSPACE(m0)));		
				
				struct mbuf *m1 = m_get(M_DONTWAIT, MT_DATA);
				
				if (m1 == NULL) {
					// BUG BUG BUG
					// Send not padded packet.
				} else {
					
					padding = mtod(m1, caddr_t);
					
					m1->m_len = paddingLen;
					m1->m_next = NULL;
					m->m_next = m1;
					m = m1;	
					m0->m_pkthdr.len += paddingLen;
				}
			}
			
			if (padding) {
				bzero(padding, paddingLen);
				
				if (m0 = m_pullup(m0, len)) {
					bcopy(mtod(m0, caddr_t) + len - 4, padding + paddingLen - 4, 4);	
				} else {
					// It should not failed....
					panic("lpx_stream_output: Can't pullup.\n");		
				}
			}
		}
		
        m = m_gethdr(M_DONTWAIT, MT_HEADER);
        if (m == NULL) {
            m_freem(m0);
            return (ENOBUFS);
        }
        /*
         * Fill in mbuf with extended SP header
         * and addresses and length put into network format.
         */
//                MH_ALIGN(m, sizeof(struct smp));
//                m->m_len = sizeof(struct smp);
//                m->m_next = m0;
//                si = mtod(m, struct smp *);
//                si->si_i = *cb->s_lpx;
//                si->si_s = cb->s_shdr;

                MH_ALIGN(m, sizeof(struct lpxhdr));
        m->m_len = sizeof(struct lpxhdr);
        m->m_next = m0;
        lpxh = mtod(m, struct lpxhdr *);
                bzero(lpxh, sizeof(*lpxh));
                
                lpxh->pu.pktsize = htons(LPX_PKT_LEN + len);
                lpxh->pu.p.type = LPX_TYPE_STREAM;
                lpxh->source_port = lpxp->lpxp_laddr.x_port;
                lpxh->dest_port = cb->s_dport;
                lpxh->u.s.sequence = cb->s_seq;
                lpxh->u.s.ackseq = cb->s_ack;
				lpxh->u.s.tag = cb->s_tag;
                lpxh->u.s.lsctl = htons(LSCTL_DATA | LSCTL_ACK);
//              len += sizeof(*si);
                len += sizeof(*lpxh);
//              si->si_len = htons((u_short)len);
        m->m_pkthdr.len = ((len - 1) | 1) + 1 + paddingLen;
        /*
         * queue stuff up for output
         */
        sbappendrecord(sb, m);
        cb->s_seq++;
    }
#ifdef notdef
    idle = (cb->s_smax == (cb->s_rack - 1));
#endif
again:
    sendalot = 1; win = 1; len = 1;
//  sendalot = 0;
//  off = cb->s_snxt - cb->s_rack;
//  win = min(cb->s_swnd, (cb->s_cwnd / CUNIT));

    /*
     * If in persist timeout with window of 0, send a probe.
     * Otherwise, if window is small but nonzero
     * and timer expired, send what we can and go into
     * transmit state.
     */
    if (cb->s_force == 1 + SMPT_PERSIST) {
        if (win != 0) {
            cb->s_timer[SMPT_PERSIST] = 0;
            cb->s_rxtshift = 0;
        }
    }

    if (cb->s_flags & SF_ACKNOW)
        goto send;
    if (cb->s_state < TCPS_ESTABLISHED)
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
    if (so->so_snd.sb_cc && cb->s_timer[SMPT_REXMT] == 0 &&
        cb->s_timer[SMPT_PERSIST] == 0) {
            cb->s_rxtshift = 0;
            smp_setpersist(cb);
    }
    /*
     * No reason to send a packet, just return.
     */
    cb->s_outx = 1;
    return (0);

send:
    /*
     * Find requested packet.
     */
    lpxh = 0;
    if (len > 0) {
        cb->s_want = cb->s_snxt;
        for (m = sb->sb_mb; m != NULL; m = m->m_act) {
//          si = mtod(m, struct smp *);
            lpxh = mtod(m, struct lpxhdr *);
//          if (SSEQ_LEQ(cb->s_snxt, si->si_seq))
            if (SSEQ_LEQ(cb->s_snxt, lpxh->u.s.sequence))
                break;
        }
//  found:
        if (lpxh != NULL) {
            if (lpxh->u.s.sequence == cb->s_snxt)
                cb->s_snxt++;
            else
//              smpstat.smps_sndvoid++, si = 0;
                smpstat.smps_sndvoid++, lpxh = 0;
        }
    }

//  if (si != NULL) {
    if (lpxh != NULL) {
    /*
     * must make a copy of this packet for
     * lpx_USER_output to monkey with
     */
//  m = m_copy(dtom(si), 0, (int)M_COPYALL);
    m = m_copy(dtom(lpxh), 0, (int)M_COPYALL);
    if (m == NULL) {
        return (ENOBUFS);
    }
//  si = mtod(m, struct smp *);
    lpxh = mtod(m, struct lpxhdr *);
//  if (SSEQ_LT(si->si_seq, cb->s_smax))
    if (SSEQ_LT(lpxh->u.s.sequence, cb->s_smax))
        smpstat.smps_sndrexmitpack++;
    else
        smpstat.smps_sndpack++;
    } else if (cb->s_force || cb->s_flags & SF_ACKNOW) {
        /*
         * Must send an acknowledgement or a probe
         */
        if (cb->s_force)
            smpstat.smps_sndprobe++;
        if (cb->s_flags & SF_ACKNOW)
            smpstat.smps_sndacks++;
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
        lpxh->pu.pktsize = htons(LPX_PKT_LEN);
        lpxh->pu.p.type = LPX_TYPE_STREAM;
        lpxh->source_port = lpxp->lpxp_laddr.x_port;
        lpxh->dest_port = cb->s_dport;
        lpxh->u.s.ackseq = cb->s_ack;
		lpxh->u.s.tag = cb->s_tag;

        switch(cb->s_state) {
        case TCPS_SYN_SENT:
        {
            lpxh->u.s.lsctl = htons(LSCTL_CONNREQ | LSCTL_ACK);
            lpxh->u.s.sequence = cb->s_seq;
			lpxh->u.s.tag = 0;  // Init 0;
            break;
        }
        
        case TCPS_FIN_WAIT_1:
        case TCPS_LAST_ACK:
       {
           DEBUG_CALL(4, ("TCPS_FIN_WAIT_1 send\n"));
            lpxh->u.s.lsctl = htons(LSCTL_DISCONNREQ | LSCTL_ACK);
//            if(!cb->AlreadySendFin) {
                lpxh->u.s.sequence = cb->s_fseq;
  //              cb->s_seq++;
    //            cb->AlreadySendFin = 1;
      //      } else
        //        lpxh->u.s.sequence = cb->s_seq-1; /* Retransmission */
            
            break;
        }
        
        default:
            lpxh->u.s.lsctl = htons(LSCTL_ACK);
            if(cb->s_flags & SF_ACKREQ)
                lpxh->u.s.lsctl |= htons(LSCTL_ACKREQ);
            lpxh->u.s.sequence = cb->s_smax + 1;
            break;
        }
        
    } else {
        cb->s_outx = 3;

        return (0);
    }
    /*
     * Stuff checksum and output datagram.
     */
//  if ((si->si_cc & SMP_SP) == 0) {
    if (ntohs(lpxh->u.s.lsctl) & LSCTL_DATA) {
        if (cb->s_force != (1 + SMPT_PERSIST) ||
            cb->s_timer[SMPT_PERSIST] == 0) {
            /*
             * If this is a new packet and we are not currently 
             * timing anything, time this one.
             */
//  if (SSEQ_LT(cb->s_smax, si->si_seq)) {
            if (SSEQ_LT(cb->s_smax, lpxh->u.s.sequence)) {
//      cb->s_smax = si->si_seq;
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
                if (cb->s_timer[SMPT_PERSIST]) {
                    cb->s_timer[SMPT_PERSIST] = 0;
                    cb->s_rxtshift = 0;
                }
            }
//      } else if (SSEQ_LT(cb->s_smax, si->si_seq)) {
//          cb->s_smax = si->si_seq;
//      }
        } else if (SSEQ_LT(cb->s_smax, lpxh->u.s.sequence)) {
            cb->s_smax = lpxh->u.s.sequence;
        }
    } else if (cb->s_state < TCPS_ESTABLISHED) {
        if (cb->s_timer[SMPT_REXMT] == 0)
            cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;

        if (ntohs(lpxh->u.s.lsctl) & LSCTL_CONNREQ) 
            cb->s_smax = lpxh->u.s.sequence;
    }
    {
        /*
         * Do not request acks when we ack their data packets or
         * when we do a gratuitous window update.
         */
        lpxh->u.s.sequence = htons(lpxh->u.s.sequence);
        lpxh->u.s.ackseq = htons(lpxh->u.s.ackseq);

//        DEBUG_CALL(4, ("cb->s_state = %x, sequence = %x, ackseq = %x\n", cb->s_state, lpxh->u.s.sequence, lpxh->u.s.ackseq));
//        DEBUG_CALL(4, ("ntohs(lpxh->u.s.lsctl) = %x\n", ntohs(lpxh->u.s.lsctl)));
        if (so->so_options & SO_DONTROUTE) {
            error = Lpx_OUT_outputfl(m, (struct route *)NULL, LPX_ROUTETOIF, lpxp);
            if (error) {
              DEBUG_CALL(0,("smp_output: error(%d) in Lpx_OUT_outputfl m@%lx, lpxp@%lx\n",error,m,lpxp));
            }
        } else {
            error = Lpx_OUT_outputfl(m, &cb->s_lpxpcb->lpxp_route, 0, NULL);
            if (error) {
              DEBUG_CALL(0,("smp_output: error(%d) in Lpx_OUT_outputfl m@%lx, cb->s_lpxpcb->lpxp_route@%lx\n",error,m,&cb->s_lpxpcb->lpxp_route));
            }
        }
    }
    if (error) {
        return (error);
    }
    smpstat.smps_sndtotal++;
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


static void smp_setpersist(register struct smppcb *cb)
{
    register int t = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;

    DEBUG_CALL(4, ("smp_setpersist\n"));

    if (cb->s_timer[SMPT_REXMT] && smp_do_persist_panics)
        panic("smp_output REXMT");
    /*
     * Start/restart persistance timer.
     */
    SMPT_RANGESET(cb->s_timer[SMPT_PERSIST],
        t*smp_backoff[cb->s_rxtshift],
        SMPTV_PERSMIN, SMPTV_PERSMAX);
    if (cb->s_rxtshift < SMP_MAXRXTSHIFT)
        cb->s_rxtshift++;
}


/*
 * SMP timer processing.
 */
static struct smppcb *smp_timers(register struct smppcb *cb, int timer)
{
    long rexmt;
    int win;

    DEBUG_CALL(4, ("smp_timers\n"));
    if(cb->s_state != TCPS_ESTABLISHED)
        DEBUG_CALL(4, ("smp_timers, timer = %d\n", timer));

    cb->s_force = 1 + timer;
    switch (timer) {

    /*
     * 2 MSL timeout in shutdown went off.  TCP deletes connection
     * control block.
     */
    case SMPT_2MSL:
        DEBUG_CALL(4, ("smp: SMPT_2MSL went off for no reason\n"));
        cb->s_timer[timer] = 0;
        cb = smp_close(cb);
        break;

    /*
     * Retransmission timer went off.  Message has not
     * been acked within retransmit interval.  Back off
     * to a longer retransmit interval and retransmit one packet.
     */
    case SMPT_REXMT:
        DEBUG_CALL(4, ("smp: SMPT_REXMT went off for no reason, cb->s_rxtshift = %d, SMP_MAXRXTSHIFT = %d\n", cb->s_rxtshift, SMP_MAXRXTSHIFT));
        if (++cb->s_rxtshift > SMP_MAXRXTSHIFT) {
            cb->s_rxtshift = SMP_MAXRXTSHIFT;
            smpstat.smps_timeoutdrop++;
            cb = smp_drop(cb, ETIMEDOUT);
            break;
        }
        smpstat.smps_rexmttimeo++;
        rexmt = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;
        rexmt *= smp_backoff[cb->s_rxtshift];
        SMPT_RANGESET(cb->s_rxtcur, rexmt, SMPTV_MIN, SMPTV_REXMTMAX);
        DEBUG_CALL(4, ("cb->s_rxtcur = %d\n", cb->s_rxtcur));
        cb->s_rxtcur = 1;
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
        if (win < 2)
            win = 2;
        cb->s_cwnd = CUNIT;
        cb->s_ssthresh = win * CUNIT;
        smp_output(cb, (struct mbuf *)NULL);
        break;

    /*
     * Persistance timer into zero window.
     * Force a probe to be sent.
     */
    case SMPT_PERSIST:
        smpstat.smps_persisttimeo++;
        smp_setpersist(cb);
    smp_output(cb, (struct mbuf *)NULL);
        break;

    /*
     * Keep-alive timer went off; send something
     * or drop connection if idle for too long.
     */
    case SMPT_KEEP:
        DEBUG_CALL(4, ("cb->s_state = %d, TCPS_ESTABLISHED  = %d\n", cb->s_state, TCPS_ESTABLISHED));
        smpstat.smps_keeptimeo++;
        if (cb->s_state < TCPS_ESTABLISHED)
            goto dropit;

//        DEBUG_CALL(4, ("time = %x %x so_options = %d\n", time.tv_sec, time.tv_usec, cb->s_lpxpcb->lpxp_socket->so_options & SO_KEEPALIVE));
        if (cb->s_lpxpcb->lpxp_socket->so_options & SO_KEEPALIVE) {
 //           DEBUG_CALL(4, ("cb->s_idle = %d, SMPTV_MAXIDLE = %d\n", cb->s_idle, SMPTV_MAXIDLE));
            if (cb->s_idle >= SMPTV_MAXIDLE)
                goto dropit;
            smpstat.smps_keepprobe++;
            cb->s_flags |= (SF_ACKNOW|SF_ACKREQ);
            smp_output(cb, (struct mbuf *)NULL);
            cb->s_flags &= ~(SF_ACKNOW|SF_ACKREQ);
        } else
            cb->s_idle = 0;
        cb->s_timer[SMPT_KEEP] = SMPTV_KEEP;
        break;
    dropit:
        smpstat.smps_keepdrops++;
        cb = smp_drop(cb, ETIMEDOUT);
        break;
    }
    return (cb);
}


//static int
//smp_reass(cb, si)
//register struct smppcb *cb;
//register struct smp *si;
static int smp_reass(register struct smppcb *cb, register struct lpxhdr *lpxh)
{
    register struct smp_q *q;
    register struct mbuf *m;
    register struct socket *so = cb->s_lpxpcb->lpxp_socket;
    char packetp = cb->s_flags & SF_HI;
//  int incr;
    char wakeup = 0;

        DEBUG_CALL(4, ("smp_reass\n"));
        
//  if (si == SI(0))
        if(lpxh == NULL)
            goto present;


        /*
     * Update our news from them.
     */
//  if (si->si_cc & SMP_SA)
        if(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA)
        cb->s_flags |= (smp_use_delack ? SF_DELACK : SF_ACKNOW);
#if 0
        if (SSEQ_GT(si->si_alo, cb->s_ralo))
        cb->s_flags |= SF_WIN;
    if (SSEQ_LEQ(si->si_ack, cb->s_rack)) {
        if ((si->si_cc & SMP_SP) && cb->s_rack != (cb->s_smax + 1)) {
            smpstat.smps_rcvdupack++;
            /*
             * If this is a completely duplicate ack
             * and other conditions hold, we assume
             * a packet has been dropped and retransmit
             * it exactly as in tcp_input().
             */
            if (si->si_ack != cb->s_rack ||
                si->si_alo != cb->s_ralo)
                cb->s_dupacks = 0;
            else if (++cb->s_dupacks == smprexmtthresh) {
                u_short onxt = cb->s_snxt;
                int cwnd = cb->s_cwnd;

                cb->s_snxt = si->si_ack;
                cb->s_cwnd = CUNIT;
                cb->s_force = 1 + SMPT_REXMT;
                smp_output(cb, (struct mbuf *)NULL);
                cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
                cb->s_rtt = 0;
                if (cwnd >= 4 * CUNIT)
                    cb->s_cwnd = cwnd / 2;
                if (SSEQ_GT(onxt, cb->s_snxt))
                    cb->s_snxt = onxt;
                return (1);
            }
        } else
            cb->s_dupacks = 0;
        goto update_window;
    }
#endif
        
        cb->s_dupacks = 0;
    /*
     * If our correspondent acknowledges data we haven't sent
     * TCP would drop the packet after acking.  We'll be a little
     * more permissive
     */
#if 0
        if (SSEQ_GT(si->si_ack, (cb->s_smax + 1))) {
        smpstat.smps_rcvacktoomuch++;
        si->si_ack = cb->s_smax + 1;
    }
#endif        
        smpstat.smps_rcvackpack++;
    /*
     * If transmit timer is running and timed sequence
     * number was acked, update smoothed round trip time.
     * See discussion of algorithm in tcp_input.c
     */
#if 0
        if (cb->s_rtt && SSEQ_GT(si->si_ack, cb->s_rtseq)) {
        smpstat.smps_rttupdated++;
        if (cb->s_srtt != 0) {
            register short delta;
            delta = cb->s_rtt - (cb->s_srtt >> 3);
            if ((cb->s_srtt += delta) <= 0)
                cb->s_srtt = 1;
            if (delta < 0)
                delta = -delta;
            delta -= (cb->s_rttvar >> 2);
            if ((cb->s_rttvar += delta) <= 0)
                cb->s_rttvar = 1;
        } else {
            /*
             * No rtt measurement yet
             */
            cb->s_srtt = cb->s_rtt << 3;
            cb->s_rttvar = cb->s_rtt << 1;
        }
        cb->s_rtt = 0;
        cb->s_rxtshift = 0;
        SMPT_RANGESET(cb->s_rxtcur,
            ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
            SMPTV_MIN, SMPTV_REXMTMAX);
    }
#endif
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
    } else if (cb->s_timer[SMPT_PERSIST] == 0)
        cb->s_timer[SMPT_REXMT] = cb->s_rxtcur;
    /*
     * When new data is acked, open the congestion window.
     * If the window gives us less than ssthresh packets
     * in flight, open exponentially (maxseg at a time).
     * Otherwise open linearly (maxseg^2 / cwnd at a time).
     */
#if 0
        incr = CUNIT;
    if (cb->s_cwnd > cb->s_ssthresh)
        incr = max(incr * incr / cb->s_cwnd, 1);
    cb->s_cwnd = min(cb->s_cwnd + incr, cb->s_cwmx);
#endif
        /*
     * Trim Acked data from output queue.
     */
    while ((m = so->so_snd.sb_mb) != NULL) {
//          if (SSEQ_LT((mtod(m, struct smp *))->si_seq, si->si_ack))
		/*
		 Add more condition codes to check the wrap around of sequence. 
		 */
		__u16	sequenceOfBeSentedPacket = (mtod(m, struct lpxhdr *))->u.s.sequence;
		
		if (SSEQ_LT(sequenceOfBeSentedPacket, lpxh->u.s.ackseq)
			&& ((lpxh->u.s.ackseq - sequenceOfBeSentedPacket) < INT16_MAX) ) {
			cb->s_rxtshift = 0;
			sbdroprecord(&so->so_snd);
		} else if (SSEQ_GT(sequenceOfBeSentedPacket, lpxh->u.s.ackseq)
				   && ((sequenceOfBeSentedPacket - lpxh->u.s.ackseq) > INT16_MAX) ) {
			cb->s_rxtshift = 0;
			sbdroprecord(&so->so_snd);			
		} else {
			break;
		}
	}
	
    sowwakeup(so);
//  cb->s_rack = si->si_ack;
        cb->s_rack = lpxh->u.s.ackseq;
//update_window:
    if (SSEQ_LT(cb->s_snxt, cb->s_rack))
        cb->s_snxt = cb->s_rack;
#if 0
        if (SSEQ_LT(cb->s_swl1, si->si_seq) || ((cb->s_swl1 == si->si_seq &&
        (SSEQ_LT(cb->s_swl2, si->si_ack))) ||
         (cb->s_swl2 == si->si_ack && SSEQ_LT(cb->s_ralo, si->si_alo)))) {
        /* keep track of pure window updates */
        if ((si->si_cc & SMP_SP) && cb->s_swl2 == si->si_ack
            && SSEQ_LT(cb->s_ralo, si->si_alo)) {
            smpstat.smps_rcvwinupd++;
            smpstat.smps_rcvdupack--;
        }
        cb->s_ralo = si->si_alo;
        cb->s_swl1 = si->si_seq;
        cb->s_swl2 = si->si_ack;
        cb->s_swnd = (1 + si->si_alo - si->si_ack);
        if (cb->s_swnd > cb->s_smxw)
            cb->s_smxw = cb->s_swnd;
        cb->s_flags |= SF_WIN;
    }
        
        /*
     * If this packet number is higher than that which
     * we have allocated refuse it, unless urgent
     */
    if (SSEQ_GT(si->si_seq, cb->s_alo)) {
        if (si->si_cc & SMP_SP) {
            smpstat.smps_rcvwinprobe++;
            return (1);
        } else
            smpstat.smps_rcvpackafterwin++;
        if (si->si_cc & SMP_OB) {
            if (SSEQ_GT(si->si_seq, cb->s_alo + 60)) {
                m_freem(dtom(si));
                return (0);
            } /* else queue this packet; */
        } else {
            /*register struct socket *so = cb->s_lpxpcb->lpxp_socket;
            if (so->so_state && SS_NOFDREF) {
                smp_close(cb);
            } else
                       would crash system*/
            smp_istat.notyet++;
            m_freem(dtom(si));
            return (0);
        }
    }
#endif
    /*
     * If this is a system packet, we don't need to
     * queue it up, and won't update acknowledge #
     */
//      if (si->si_cc & SMP_SP) {
        if(!(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA)) {
        return (1);
    }

        if(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA)
            DEBUG_CALL(4, ("here 0\n"));
        /*
     * We have already seen this packet, so drop.
     */

//      if (SSEQ_LT(si->si_seq, cb->s_ack)) {
        if (SSEQ_LT(lpxh->u.s.sequence, cb->s_ack)) {
        smp_istat.bdreas++;
        smpstat.smps_rcvduppack++;
//              if (si->si_seq == cb->s_ack - 1)
        if (lpxh->u.s.sequence == cb->s_ack - 1)
            smp_istat.lstdup++;
        return (1);
    }

    if(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA)
        DEBUG_CALL(4, ("here 1\n"));
        /*
     * Loop through all packets queued up to insert in
     * appropriate sequence.
     */
    for (q = cb->s_q.si_next; q != &cb->s_q; q = q->si_next) {
//              if (si->si_seq == SI(q)->si_seq) {
        if (lpxh->u.s.sequence == ((struct lpxhdr *)q)->u.s.sequence) {
            smpstat.smps_rcvduppack++;
            return (1);
        }
//              if (SSEQ_LT(si->si_seq, SI(q)->si_seq)) {
        if (SSEQ_LT(lpxh->u.s.sequence, ((struct lpxhdr *)q)->u.s.sequence)) {
            smpstat.smps_rcvoopack++;
            break;
        }
    }
//  insque(si, q->si_prev);
        if(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA) {
            DEBUG_CALL(4, ("here 2\n"));
            DEBUG_CALL(4, ("sequence = %d, seq = %d\n",lpxh->u.s.sequence, cb->s_ack));
    }

        insque(lpxh, q->si_prev);
    /*
     * If this packet is urgent, inform process
     */
#if 0
        if (si->si_cc & SMP_OB) {
        cb->s_iobc = ((char *)si)[1 + sizeof(*si)];
        sohasoutofband(so);
        cb->s_oobflags |= SF_IOOB;
    }
#endif
        
present:
    /*
     * Loop through all packets queued up to update acknowledge
     * number, and present all acknowledged data to user;
     * If in packet interface mode, show packet headers.
     */
    for (q = cb->s_q.si_next; q != &cb->s_q; q = q->si_next) {
		DEBUG_CALL(4, ("sequence = %x, seq = %x\n",((struct lpxhdr *)q)->u.s.sequence, cb->s_ack));
		if (((struct lpxhdr *)q)->u.s.sequence == cb->s_ack) {
			cb->s_ack++;
			m = dtom(q);
#if 0
			if (SI(q)->si_cc & SMP_OB) {
                cb->s_oobflags &= ~SF_IOOB;
                if (so->so_rcv.sb_cc)
                    so->so_oobmark = so->so_rcv.sb_cc;
                else
                    so->so_state |= SS_RCVATMARK;
            }
#endif                        
			q = q->si_prev;
            remque(q->si_next);
            wakeup = 1;
            smpstat.smps_rcvpack++;
#ifdef SF_NEWCALL
            if (cb->s_flags2 & SF_NEWCALL) {
                struct smphdr *sp = mtod(m, struct smphdr *);
                u_char dt = sp->smp_dt;
                smp_newchecks[4]++;
                if (dt != cb->s_rhdr.smp_dt) {
                    struct mbuf *mm =
					m_getclr(M_DONTWAIT, MT_CONTROL);
                    smp_newchecks[0]++;
                    if (mm != NULL) {
                        u_short *s =
						mtod(mm, u_short *);
                        cb->s_rhdr.smp_dt = dt;
                        mm->m_len = 5; /*XXX*/
                        s[0] = 5;
                        s[1] = 1;
                        *(u_char *)(&s[2]) = dt;
                        sbappend(&so->so_rcv, mm);
                    }
                }
                if (sp->smp_cc & SMP_OB) {
                    MCHTYPE(m, MT_OOBDATA);
                    smp_newchecks[1]++;
                    so->so_oobmark = 0;
                    so->so_state &= ~SS_RCVATMARK;
                }
                if (packetp == 0) {
                    m->m_data += SPINC;
                    m->m_len -= SPINC;
                    m->m_pkthdr.len -= SPINC;
                }
                if ((sp->smp_cc & SMP_EM) || packetp) {
                    sbappendrecord(&so->so_rcv, m);
                    smp_newchecks[9]++;
                } else
                    sbappend(&so->so_rcv, m);
            } else
#endif
				if (packetp) {
					sbappendrecord(&so->so_rcv, m);
				} else {
					//                            cb->s_rhdr = *mtod(m, struct smphdr *);
					//                            m->m_data += SPINC;
					//                            m->m_len -= SPINC;
					//                            m->m_pkthdr.len -= SPINC;
					m->m_data += sizeof(struct lpxhdr);
					m->m_len -= sizeof(struct lpxhdr);
					m->m_pkthdr.len -= sizeof(struct lpxhdr);
					//                            if(ntohs(lpxh->u.s.lsctl) & LSCTL_DATA)
					DEBUG_CALL(4, ("here 3 m->m_len = %d\n", (int)m->m_len));
					sbappend(&so->so_rcv, m);
				}
		} else
            break;
    }
    if (wakeup)
        sorwakeup(so);
    return (0);
}


static struct smppcb *smp_close(struct smppcb *cb)
{
    register struct smp_q *s;
    struct lpxpcb *lpxp = cb->s_lpxpcb;
    struct socket *so = lpxp->lpxp_socket;
    register struct mbuf *m;

    DEBUG_CALL(4, ("smp_close\n"));

    s = cb->s_q.si_next;
    while (s != &(cb->s_q)) {
        s = s->si_next;
        m = dtom(s->si_prev);
        remque(s->si_prev);
        m_freem(m);
    }
    m_free(dtom(cb->s_lpx));
    FREE(cb, M_PCB);
    lpxp->lpxp_pcb = 0;
    soisdisconnected(so);
    Lpx_PCB_detach(lpxp);
    smpstat.smps_closed++;
    return ((struct smppcb *)NULL);
}


struct smppcb *smp_drop(struct smppcb *cb, int errno)
{
    struct socket *so = cb->s_lpxpcb->lpxp_socket;
  
     DEBUG_CALL(4, ("smp_drop\n"));

   if (SMPS_HAVERCVDSYN(cb->s_state)) {
        cb->s_state = TCPS_CLOSED;
        (void) smp_output(cb, (struct mbuf *)NULL);
        smpstat.smps_drops++;
    } else
        smpstat.smps_conndrops++;
    if (errno == ETIMEDOUT && cb->s_softerror)
        errno = cb->s_softerror;
    so->so_error = errno;
    return (smp_close(cb));
}

