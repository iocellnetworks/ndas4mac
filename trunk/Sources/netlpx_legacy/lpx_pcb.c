/***************************************************************************
 *
 *  lpx_pcb.c
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
 *  @(#)lpx_pcb.c
 *
 * $FreeBSD: src/sys/netlpx/lpx_pcb.c,v 1.25 2002/05/31 11:52:33 tanimura Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/random.h>



#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_pcb.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control.h>

#include <machine/spl.h>

struct   lpx_addr zerolpx_addr;

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

int Lpx_PCB_alloc( struct socket *so,
                  struct lpxpcb *head,
                  struct proc *td )
{
    register struct lpxpcb *lpxp;

    DEBUG_CALL(4, ("Lpx_PCB_alloc\n"));
    
    MALLOC(lpxp, struct lpxpcb *, sizeof *lpxp, M_PCB, M_WAITOK);
    if (lpxp == NULL) {
        DEBUG_CALL(0, ("Lpx_PCB_alloc:==> Failed\n"));
        return (ENOBUFS);
    }
    bzero(lpxp, sizeof(*lpxp));
    
    lpxp->lpxp_socket = so;
    if (lpxcksum)
        lpxp->lpxp_flags |= LPXP_CHECKSUM;

    read_random(&lpxp->lpxp_messageid, sizeof(lpxp->lpxp_messageid));

    insque(lpxp, head);
    so->so_pcb = (caddr_t)lpxp;
    so->so_options |= SO_DONTROUTE;

    return (0);
}

    
int Lpx_PCB_bind( register struct lpxpcb *lpxp,
                  struct sockaddr *nam,		/* local address */
                  struct proc *td )
{
    register struct sockaddr_lpx *slpx = NULL;
    u_short lport = 0;
	struct lpx_addr				laddr;

	DEBUG_CALL(4, ("Lpx_PCB_bind: Entered.\n"));
	
    if (lpxp->lpxp_lport || !lpx_nullhost(lpxp->lpxp_laddr)) {
      DEBUG_CALL(0,("!!!Lpx_PCB_bind: already have local port or address info\n"));
      return (EINVAL);
    }

    if (nam) {
      slpx = (struct sockaddr_lpx *)nam;
      if (!lpx_nullhost(slpx->sipx_addr)) {
        int tport = slpx->slpx_port;

        slpx->slpx_port = 0;        /* yech... */
        if (ifa_ifwithaddr((struct sockaddr *)slpx) == 0) {
            DEBUG_CALL(0,("!!Lpx_PCB_bind: Address not available\n"));
            return (EADDRNOTAVAIL);
        }
        slpx->slpx_port = tport;
      }
      lport = slpx->slpx_port;
      if (lport) {
#if 0
        u_short aport = ntohs(lport);
        int error;
#endif /* if 0 */

#if 0
        if (aport < LPXPORT_RESERVED &&
          td != NULL && (error = suser(td->p_ucred, &td->p_acflag)) != 0) {
          return (error);
        }
#endif /* if 0 */
#if 0
        if (Lpx_PCB_lookup(&zerolpx_addr, lport, 0)) {
          DEBUG_CALL(0,("!!Lpx_PCB_bind: local port number in use\n"));
          return (EADDRINUSE);
        }
#else /* if 0 */
		
		DEBUG_CALL(4, ("Lpx_PCB_bind: 1.\n"));
		
		laddr.x_port = lport;
		
        if (Lpx_PCB_lookup(&slpx->sipx_addr, &laddr, 0)) {
          DEBUG_CALL(0,("!!Lpx_PCB_bind: local port number in use\n"));
          return (EADDRINUSE);
        }
#endif /* if 0 else */
      }
      lpxp->lpxp_laddr = slpx->sipx_addr;
    } 	/* if (nam) */

    if (lport == 0) {
	
		DEBUG_CALL(4, ("Lpx_PCB_bind: 2.\n"));
		
#if 1		
        do {
            lpxpcb.lpxp_lport++;
            if ((lpxpcb.lpxp_lport < LPXPORT_RESERVED) ||
                (lpxpcb.lpxp_lport >= LPXPORT_WELLKNOWN))
                lpxpcb.lpxp_lport = LPXPORT_RESERVED;
            lport = htons(lpxpcb.lpxp_lport);
			
			laddr.x_port = lport;

        } while (Lpx_PCB_lookup(&zerolpx_addr, &laddr, 0));
#else /* if 0 */
        do {
            lpxpcb.lpxp_lport++;
            if ((lpxpcb.lpxp_lport < LPXPORT_RESERVED) ||
                (lpxpcb.lpxp_lport >= LPXPORT_WELLKNOWN))
                lpxpcb.lpxp_lport = LPXPORT_RESERVED;
            lport = htons(lpxpcb.lpxp_lport);
        } while (Lpx_PCB_lookup(&slpx->sipx_addr, lport, 0));
#endif /* if 0 else */
    }	/* if (lport == 0) */

    lpxp->lpxp_lport = lport;
#if 1
    ((struct sockaddr_lpx *)nam)->slpx_port = lport;
#endif /* if 1 */
    return (0);
}


/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument slpx.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int Lpx_PCB_connect( struct lpxpcb *lpxp,
                    struct sockaddr *nam,
                    struct proc *td )
{
    struct lpx_ifaddr *ia = NULL;
    register struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;
    register struct lpx_addr *dst;
    register struct route *ro;
    struct ifnet *ifp;
	struct lpx_addr laddr;

    DEBUG_CALL(4, ("Lpx_PCB_connect\n"));

    if (nam == NULL) {
      return(EINVAL);
    }

    DEBUG_CALL(2, ("slpx->sipx_addr.x_net = %x\n", slpx->sipx_addr.x_net));

    if (slpx->slpx_family != AF_LPX)
        return (EAFNOSUPPORT);
    if (slpx->slpx_port == 0 || lpx_nullhost(slpx->sipx_addr))
        return (EADDRNOTAVAIL);
    /*
     * If we haven't bound which network number to use as ours,
     * we will use the number of the outgoing interface.
     * This depends on having done a routing lookup, which
     * we will probably have to do anyway, so we might
     * as well do it now.  On the other hand if we are
     * sending to multiple destinations we may have already
     * done the lookup, so see if we can use the route
     * from before.  In any case, we only
     * chose a port number once, even if sending to multiple
     * destinations.
     */
    ro = &lpxp->lpxp_route;
    dst = &satolpx_addr(ro->ro_dst);
    if (lpxp->lpxp_socket->so_options & SO_DONTROUTE)
        goto flush;
    if (!lpx_neteq(lpxp->lpxp_lastdst, slpx->sipx_addr))
        goto flush;
    if (!lpx_hosteq(lpxp->lpxp_lastdst, slpx->sipx_addr)) {
        if (ro->ro_rt != NULL && !(ro->ro_rt->rt_flags & RTF_HOST)) {
            /* can patch route to avoid rtalloc */
            *dst = slpx->sipx_addr;
        } else {
    flush:
        if (ro->ro_rt != NULL)
                RTFREE(ro->ro_rt);
            ro->ro_rt = NULL;
        }
    }/* else cached route is ok; do nothing */
    lpxp->lpxp_lastdst = slpx->sipx_addr;
    if ((lpxp->lpxp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
        (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL)) {
            /* No route yet, so try to acquire one */
            ro->ro_dst.sa_family = AF_LPX;
            ro->ro_dst.sa_len = sizeof(ro->ro_dst);
            *dst = slpx->sipx_addr;
            dst->x_port = 0;
            rtalloc(ro);
			
			DEBUG_CALL(4, ("Lpx_PCB_connect RO 1\n"));
        }
    if (lpx_neteqnn(lpxp->lpxp_laddr.x_net, lpx_zeronet)) {
        
		DEBUG_CALL(4, ("Lpx_PCB_connect RO 2\n"));
		
		/* 
         * If route is known or can be allocated now,
         * our src addr is taken from the i/f, else punt.
         */

        /*
         * If we found a route, use the address
         * corresponding to the outgoing interface
         */
        ia = NULL;
        if (ro->ro_rt != NULL && (ifp = ro->ro_rt->rt_ifp) != NULL)
            for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
                if (ia->ia_ifp == ifp)
                    break;
        if (ia == NULL) {
           // u_short fport = slpx->sipx_addr.x_port;
            //slpx->sipx_addr.x_port = 0;
            ia = (struct lpx_ifaddr *)
                ifa_ifwithaddr((struct sockaddr *)&lpxp->lpxp_laddr);
            //slpx->sipx_addr.x_port = fport;

			// BUG BUG BUG!!! 
            if (ia == NULL)
                ia = Lpx_CTL_iaonnetof(&slpx->sipx_addr);
            if (ia == NULL)
                ia = lpx_ifaddr;
            if (ia == NULL)
                return (EADDRNOTAVAIL);
        }
        lpxp->lpxp_laddr.x_net = satolpx_addr(ia->ia_addr).x_net;
    }
    if (lpx_nullhost(lpxp->lpxp_laddr)) {
		
		DEBUG_CALL(4, ("Lpx_PCB_connect RO 3\n"));
		
        /* 
         * If route is known or can be allocated now,
         * our src addr is taken from the i/f, else punt.
         */

        /*
         * If we found a route, use the address
         * corresponding to the outgoing interface
         */
        ia = NULL;
        if (ro->ro_rt != NULL && (ifp = ro->ro_rt->rt_ifp) != NULL)
            for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
                if (ia->ia_ifp == ifp)
                    break;
        if (ia == NULL) {
            u_short fport = slpx->sipx_addr.x_port;
            slpx->sipx_addr.x_port = 0;
            ia = (struct lpx_ifaddr *)
                ifa_ifwithdstaddr((struct sockaddr *)slpx);
            slpx->sipx_addr.x_port = fport;
			// BUG BUG BUG!!!
			
            if (ia == NULL)
                ia = Lpx_CTL_iaonnetof(&slpx->sipx_addr);
            if (ia == NULL)
                ia = lpx_ifaddr;
            if (ia == NULL)
                return (EADDRNOTAVAIL);
        }
        lpxp->lpxp_laddr.x_host = satolpx_addr(ia->ia_addr).x_host;
    }

	DEBUG_CALL(4, ("Lpx_PCB_connect: 4.\n"));

	laddr.x_port = lpxp->lpxp_lport;

    if (Lpx_PCB_lookup(&slpx->sipx_addr, &laddr, 0))
        return (EADDRINUSE);
    if (lpxp->lpxp_lport == 0)
        Lpx_PCB_bind(lpxp, (struct sockaddr *)NULL, td);

    /* XXX just leave it zero if we can't find a route */

    lpxp->lpxp_faddr = slpx->sipx_addr;
    /* Includes lpxp->lpxp_fport = slpx->slpx_port; */
    return (0);
}


void Lpx_PCB_disconnect( struct lpxpcb *lpxp )
{

    lpxp->lpxp_faddr = zerolpx_addr;
    if (lpxp->lpxp_socket->so_state & SS_NOFDREF)
        Lpx_PCB_detach(lpxp);
}


void Lpx_PCB_detach(struct lpxpcb *lpxp )
{
    struct socket *so = lpxp->lpxp_socket;

    so->so_pcb = 0;
    sofree(so);
        
    if (lpxp->lpxp_route.ro_rt != NULL)
        rtfree(lpxp->lpxp_route.ro_rt);
    remque(lpxp);
    FREE(lpxp, M_PCB);
}


struct lpxpcb * Lpx_PCB_lookup( struct lpx_addr *faddr,	/* peer addr:port */
								struct lpx_addr *laddr,	/* local addr:port */
                               int wildp )
{
    register struct lpxpcb *lpxp, *match = NULL;
    int matchwild = 3, wildcard = 0;	/* matching cost */
    u_short fport;
	u_short lport;
	
    /* Check arguments */
    if (NULL==faddr) {
		return (NULL);
    }

	fport = faddr->x_port;
	lport = laddr->x_port;

    DEBUG_CALL(4, ("PCBLU>> lpxpcb@(%lx):lpx_next@(%lx)  %02x:%02x:%02x:%02x:%02x:%02x faddr->xport = %d, lport = %d wildp = %d\n",&lpxpcb, lpxpcb.lpxp_next, faddr->x_node[0],faddr->x_node[1],faddr->x_node[2],faddr->x_node[3],faddr->x_node[4],faddr->x_node[5], faddr->x_port, lport, wildp));
	
    for (lpxp = (&lpxpcb)->lpxp_next; lpxp != (&lpxpcb); lpxp = lpxp->lpxp_next) {
		
        DEBUG_CALL(4, ("PCBLU>> lpxp@(%lx) %02x:%02x:%02x:%02x:%02x:%02x lpxp_lport = %d, lpxp_fport = %d \n", lpxp, lpxp->lpxp_faddr.x_node[0], lpxp->lpxp_faddr.x_node[1], lpxp->lpxp_faddr.x_node[2], lpxp->lpxp_faddr.x_node[3], lpxp->lpxp_faddr.x_node[4], lpxp->lpxp_faddr.x_node[5], lpxp->lpxp_lport, lpxp->lpxp_fport));
		
		if (lpxp->lpxp_lport == lport) {	/* at least local port number should match */
			wildcard = 0;
			if (lpx_nullhost(lpxp->lpxp_faddr)) {	/* no peer address registered : INADDR_ANY */
				if (!lpx_nullhost(*faddr)) {
					if(lpx_hosteq(lpxp->lpxp_laddr, *laddr)) {
						DEBUG_CALL(4, ("Maybe broadcast packet.PCBLU>> lpxp@(%lx) %02x:%02x:%02x:%02x:%02x:%02x lpxp_lport = %d, lpxp_fport = %d \n", lpxp, lpxp->lpxp_laddr.x_node[0], lpxp->lpxp_laddr.x_node[1], lpxp->lpxp_laddr.x_node[2], lpxp->lpxp_laddr.x_node[3], lpxp->lpxp_laddr.x_node[4], lpxp->lpxp_laddr.x_node[5], lpxp->lpxp_lport, lpxp->lpxp_fport));
					} else {
						wildcard++;
					}
				}
			} else {				/* peer address is in the DB */
				if (lpx_nullhost(*faddr)) {
					wildcard++;
				} else {
					if (!lpx_hosteq(lpxp->lpxp_faddr, *faddr)) {/* peer address don't match */
						continue;
					}
					if (lpxp->lpxp_fport != fport) {
						if (lpxp->lpxp_fport != 0) {		/* peer port number don't match */
							continue;
						} else {
							wildcard++;
						}
					}
				}
			}

			DEBUG_CALL(2, ("PCBLU>> matchwild = %d, wildcard = %d\n",matchwild, wildcard));

			if (wildcard && (wildp == 0)) {	/* if wildcard match is allowed */
				continue;
			}
			if (wildcard < matchwild) {
				match = lpxp;
				matchwild = wildcard;
				
				if (wildcard == 0) {		/* exact match : all done */
					break;
				}
			}
		} /* if (lpxp->lpxp_lport == lport) */
    }	/* for (lpxp = ...) ... */
	DEBUG_CALL(6, ("PCBLU>> match@(%lx) matchwild = %d, wildcard = %d\n", match, matchwild, wildcard));
	if (match) {
		DEBUG_CALL(4, ("PCBLU>> match@(%lx) matchwild = %d, wildcard = %d\n", match, matchwild, wildcard));
	}
	return (match);
}


void Lpx_PCB_setsockaddr( register struct lpxpcb *lpxp,
                      struct sockaddr **nam )
{
    struct sockaddr_lpx *slpx, sslpx;
    
    slpx = &sslpx;
    bzero((caddr_t)slpx, sizeof(*slpx));
    slpx->slpx_len = sizeof(*slpx);
    slpx->slpx_family = AF_LPX;
    slpx->sipx_addr = lpxp->lpxp_laddr;
    *nam = dup_sockaddr((struct sockaddr *)slpx, 0);
}


void Lpx_PCB_setpeeraddr(register struct lpxpcb *lpxp,
                     struct sockaddr **nam )
{
    struct sockaddr_lpx *slpx, sslpx;
    
    slpx = &sslpx;
    bzero((caddr_t)slpx, sizeof(*slpx));
    slpx->slpx_len = sizeof(*slpx);
    slpx->slpx_family = AF_LPX;
    slpx->sipx_addr = lpxp->lpxp_faddr;
    *nam = dup_sockaddr((struct sockaddr *)slpx, 0);
}


#if 0
/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  Call the
 * protocol specific routine to handle each connection.
 * Also pass an extra paramter via the lpxpcb. (which may in fact
 * be a parameter list!)
 */
void Lpx_PCB_notify( register struct lpx_addr *dst,
                    int errno,
                    void (*notify)(struct lpxpcb *),
                    long param )
{
    register struct lpxpcb *lpxp, *oinp;
    int s = splimp();

    for (lpxp = (&lpxpcb)->lpxp_next; lpxp != (&lpxpcb);) {
        if (!lpx_hosteq(*dst,lpxp->lpxp_faddr)) {
    next:
            lpxp = lpxp->lpxp_next;
            continue;
        }
        if (lpxp->lpxp_socket == 0)
            goto next;
        if (errno) 
            lpxp->lpxp_socket->so_error = errno;
        oinp = lpxp;
        lpxp = lpxp->lpxp_next;
        oinp->lpxp_notify_param = param;
        (*notify)(oinp);
    }
    splx(s);
}

#ifdef notdef
/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
lpx_rtchange( struct lpxpcb *lpxp )
{
    if (lpxp->lpxp_route.ro_rt != NULL) {
        rtfree(lpxp->lpxp_route.ro_rt);
        lpxp->lpxp_route.ro_rt = NULL;
        /*
         * A new route can be allocated the next time
         * output is attempted.
         */
    }
    /* SHOULD NOTIFY HIGHER-LEVEL PROTOCOLS */
}
#endif

#endif /* if 0 */

