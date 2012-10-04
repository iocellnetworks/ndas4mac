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
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/random.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

//#include "ExcludedByApple.h"

#include "Utilities.h"
#include "lpx_var.h"
#include "lpx_pcb.h"
#include "lpx.h"
#include "lpx_if.h"
#include "lpx_base.h"
#include "lpx_datagram.h"
#include "lpx_stream_var.h"
#include "lpx_stream.h"

struct   lpx_addr zerolpx_addr;

// List Lock.
extern lck_rw_t *stream_mtx;
extern lck_rw_t *datagram_mtx;

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
	
    DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_alloc\n"));
    	
    MALLOC(lpxp, struct lpxpcb *, sizeof *lpxp, M_PCB, M_WAITOK);
    if (lpxp == NULL) {
        DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_alloc:==> Failed\n"));
        return (ENOBUFS);
    }
    bzero(lpxp, sizeof(*lpxp));
    
    lpxp->lpxp_socket = so;
    if (lpxcksum)
        lpxp->lpxp_flags |= LPXP_CHECKSUM;
	
    read_random(&lpxp->lpxp_messageid, sizeof(lpxp->lpxp_messageid));
	
	lck_rw_lock_exclusive(head->lpxp_list_rw);	
    insque(lpxp, head);
	lck_rw_unlock_exclusive(head->lpxp_list_rw);
	
	lpxp->lpxp_head = head;
	
    so->so_pcb = (caddr_t)lpxp;
    //so->so_options |= SO_DONTROUTE;
	
	if (so->so_proto->pr_flags & PR_PCBLOCK) {
		
		if (head == &lpx_stream_pcb) {
			lpxp->lpxp_mtx = lck_mtx_alloc_init(stream_mtx_grp, stream_mtx_attr);
			lpxp->lpxp_mtx_grp = stream_mtx_grp;
		} else {
			lpxp->lpxp_mtx = lck_mtx_alloc_init(datagram_mtx_grp, datagram_mtx_attr);
			lpxp->lpxp_mtx_grp = datagram_mtx_grp;
		}
		
		if (lpxp->lpxp_mtx == NULL) {
			DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_alloc: can't alloc mutex! so=%p\n", so));
			
			FREE(lpxp, M_PCB);

			return(ENOMEM);
		}
	}
	
    return (0);
}


int Lpx_PCB_bind( register struct lpxpcb *lpxp,
                  struct sockaddr *nam,		// local address
                  struct proc *td)
{
    struct sockaddr_lpx *slpx = NULL;
    u_short lport = 0;
	struct lpx_addr				laddr;
	
	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_bind: Entered.\n"));
	
    if (lpxp->lpxp_lport || !lpx_nullhost(lpxp->lpxp_laddr)) {
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("!!!Lpx_PCB_bind: already have local port or address info\n"));
		return (EINVAL);
    }
	
    if (nam) {
		slpx = (struct sockaddr_lpx *)nam;
		if (!lpx_nullhost(slpx->sipx_addr)) {
			int tport = slpx->slpx_port;
			
			slpx->slpx_port = 0;        /* yech... */
			if (lpx_find_ifnet_by_addr(&satolpx_addr(*slpx)) == 0) {
				DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("!!Lpx_PCB_bind: Address not available\n"));
				return (EADDRNOTAVAIL);
			}
			slpx->slpx_port = tport;
		}
		lport = slpx->slpx_port;
		if (lport) {
			
			DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_bind: 1.\n"));
			
			laddr.x_port = lport;
			
			if (Lpx_PCB_lookup(&slpx->sipx_addr, &laddr, 0, lpxp->lpxp_head)) {
				DEBUG_PRINT(0,("!!Lpx_PCB_bind: local port number in use\n"));
				return (EADDRINUSE);
			}
		}
		lpxp->lpxp_laddr = slpx->sipx_addr;
    } 	/* if (nam) */

    if (lport == 0) {
		
		DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_bind: 2.\n"));

		if ((lpxp->lpxp_head->lpxp_lport < LPXPORT_RESERVED) ||
			(lpxp->lpxp_head->lpxp_lport >= LPXPORT_WELLKNOWN))
			lpxp->lpxp_head->lpxp_lport = LPXPORT_RESERVED;
		
		u_short startPort = lpxp->lpxp_head->lpxp_lport;
		
        do {

			lpxp->lpxp_head->lpxp_lport++;
			
			if ((lpxp->lpxp_head->lpxp_lport < LPXPORT_RESERVED) ||
				(lpxp->lpxp_head->lpxp_lport >= LPXPORT_WELLKNOWN))
				lpxp->lpxp_head->lpxp_lport = LPXPORT_RESERVED;

			if (lport == startPort) {
				return (EADDRINUSE); 
			} 

            lport = htons(lpxp->lpxp_head->lpxp_lport);

			laddr.x_port = lport;
			
        } while (Lpx_PCB_lookup(&zerolpx_addr, &laddr, 0, lpxp->lpxp_head));
    }	/* if (lport == 0) */

    lpxp->lpxp_lport = lport;

	if (nam) {
		((struct sockaddr_lpx *)nam)->slpx_port = lport;
    }

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
    struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)nam;
//	struct lpx_addr laddr = { 0 };
	
    DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_connect: Entered.\n"));
		
    if (nam == NULL) {
		return(EINVAL);
    }

//    DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("slpx->sipx_addr.x_net = %x\n", slpx->sipx_addr.x_net));
	
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
	
    lpxp->lpxp_lastdst = slpx->sipx_addr;
	
	DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_connect RO 1\n"));

    if (lpx_nullhost(lpxp->lpxp_laddr)) {
		
		DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_connect RO 3\n"));
		
        /*
         * If we found a route, use the address
         * corresponding to the outgoing interface
         */
        ia = NULL;
				
		if (ia == NULL)
			ia = lpx_iaonnetof(&slpx->sipx_addr);
		if (ia == NULL)
			ia = lpx_ifaddrs;	// Use first interface
		if (ia == NULL)
			return (EADDRNOTAVAIL);
		
		lpxp->lpxp_laddr.x_host = satolpx_addr(ia->ia_addr).x_host;
    }
		
	DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_connect: 4.\n"));

	if (lpxp->lpxp_laddr.x_port == 0) {
        Lpx_PCB_bind(lpxp, (struct sockaddr *)NULL, td);
	}
	/*
    if (Lpx_PCB_lookup(&slpx->sipx_addr, &laddr, 0, lpxp->lpxp_head)) {	

		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_connect: slpx %02x:%02x:%02x:%02x:%02x:%02x[%d]\n", 
										  slpx->sipx_addr.x_node[0],
										  slpx->sipx_addr.x_node[1],
										  slpx->sipx_addr.x_node[2],
										  slpx->sipx_addr.x_node[3],
										  slpx->sipx_addr.x_node[4],
										  slpx->sipx_addr.x_node[5],
										   slpx->sipx_addr.x_port));
		
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_connect: laddr %02x:%02x:%02x:%02x:%02x:%02x[%d]\n", 
										  lpxp->lpxp_laddr.x_node[0],
										  lpxp->lpxp_laddr.x_node[1],
										  lpxp->lpxp_laddr.x_node[2],
										  lpxp->lpxp_laddr.x_node[3],
										  lpxp->lpxp_laddr.x_node[4],
										  lpxp->lpxp_laddr.x_node[5],
										   lpxp->lpxp_laddr.x_port));
		
				
        return (EADDRINUSE);
	}
	*/
	
    /* XXX just leave it zero if we can't find a route */
	
    //lpxp->lpxp_faddr = slpx->sipx_addr;
	bcopy(slpx->slpx_node, lpxp->lpxp_faddr.x_node, 6);
	lpxp->lpxp_faddr.x_port = slpx->slpx_port;
	
    /* Includes lpxp->lpxp_fport = slpx->slpx_port; */

	DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_connect: Source %02x:%02x:%02x:%02x:%02x:%02x\n", 
									  lpxp->lpxp_laddr.x_node[0],
									  lpxp->lpxp_laddr.x_node[1],
									  lpxp->lpxp_laddr.x_node[2],
									  lpxp->lpxp_laddr.x_node[3],
									  lpxp->lpxp_laddr.x_node[4],
									  lpxp->lpxp_laddr.x_node[5]));
	
	DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Lpx_PCB_connect: Dest %02x:%02x:%02x:%02x:%02x:%02x\n", 
									  lpxp->lpxp_faddr.x_node[0],
									  lpxp->lpxp_faddr.x_node[1],
									  lpxp->lpxp_faddr.x_node[2],
									  lpxp->lpxp_faddr.x_node[3],
									  lpxp->lpxp_faddr.x_node[4],
									  lpxp->lpxp_faddr.x_node[5]));

	return (0);
}


void Lpx_PCB_disconnect( struct lpxpcb *lpxp )
{
	
	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_disconnect\n"));
	
	if (!lpxp->lpxp_socket) {
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_disconnect: No Socket!\n"));
		return;
	}
	
    lpxp->lpxp_faddr = zerolpx_addr;
    if (lpxp->lpxp_socket->so_state & SS_NOFDREF)
	{
        Lpx_PCB_detach(lpxp);
	}
}


void Lpx_PCB_detach(struct lpxpcb *lpxp )
{
    struct socket *so = lpxp->lpxp_socket;
	
	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_detach\n"));
	
	if (!so) {
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("No Socket\n"));

		return;
	}
	
	if ((so->so_flags & SOF_PCBCLEARING) == 0) {
		
		DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_detach: usecount %d state 0x%x refCount %d\n", so->so_usecount, so->so_state, so->so_retaincnt));
		
		if (so->so_state & SS_ISDISCONNECTED) {
			so->so_flags |= SOF_PCBCLEARING; /* makes sure we're not called twice from so_close */
			
			so->so_pcb = 0;	
			//		lpxp->lpxp_socket = NULL;		
			
			sofree(so);			
		}
	}
}

void Lpx_PCB_dispense(struct lpxpcb *lpxp )
{	
	struct stream_pcb *cb = NULL;

	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_dispense: Entered.\n"));

	if (lpxp == 0) {
		return;
	}
	
	cb = (struct stream_pcb *)lpxp->lpxp_pcb;
	
	if (cb != 0) {
		
		register struct lpx_stream_q *q;
				
		for (q = cb->s_q.si_next; q != &cb->s_q; q = q->si_next) {
			q = q->si_prev;
			remque(q->si_next);
		}
		
		m_freem(dtom(cb->s_lpx));
		FREE(cb, M_PCB);
		lpxp->lpxp_pcb = 0;
	}	
	
    // Free Lock.
	if (lpxp->lpxp_mtx != NULL) {
		lck_mtx_free(lpxp->lpxp_mtx, lpxp->lpxp_mtx_grp);  
	}
				
	lck_rw_lock_exclusive(lpxp->lpxp_head->lpxp_list_rw);
	remque(lpxp);
	lck_rw_unlock_exclusive(lpxp->lpxp_head->lpxp_list_rw);
		
	FREE(lpxp, M_PCB);		
}

struct lpxpcb * Lpx_PCB_lookup( struct lpx_addr *faddr,	// peer addr:port 
								struct lpx_addr *laddr,	// local addr:port 
								int wildp,
								struct lpxpcb	*head
								)
{
    register struct lpxpcb *lpxp, *match = NULL;
    int matchwild = 3, wildcard = 0;	/* matching cost */
    u_short fport;
	u_short lport;

//	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_lookup\n"));
	
    /* Check arguments */
    if (NULL == faddr
		|| NULL == head) {
		return (NULL);
    }
	
	fport = faddr->x_port;
	lport = laddr->x_port;
	
//    DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("PCBLU>> lpxpcb@(%lx):lpx_next@(%lx)  %02x:%02x:%02x:%02x:%02x:%02x faddr->xport = %d, lport = %d wildp = %d\n",&lpxpcb, lpxpcb.lpxp_next, faddr->x_node[0],faddr->x_node[1],faddr->x_node[2],faddr->x_node[3],faddr->x_node[4],faddr->x_node[5], faddr->x_port, lport, wildp));
	
	lck_rw_lock_shared(head->lpxp_list_rw);
	
    for (lpxp = (head)->lpxp_next; lpxp != (head); lpxp = lpxp->lpxp_next) {
		
 //       DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("PCBLU>> lpxp@(%lx) %02x:%02x:%02x:%02x:%02x:%02x lpxp_lport = %d, lpxp_fport = %d \n", lpxp, lpxp->lpxp_faddr.x_node[0], lpxp->lpxp_faddr.x_node[1], lpxp->lpxp_faddr.x_node[2], lpxp->lpxp_faddr.x_node[3], lpxp->lpxp_faddr.x_node[4], lpxp->lpxp_faddr.x_node[5], lpxp->lpxp_lport, lpxp->lpxp_fport));
		
		if (lpxp->lpxp_lport == lport) {	/* at least local port number should match */
			wildcard = 0;
			
			if (lpx_nullhost(lpxp->lpxp_faddr)) {	/* no peer address registered : INADDR_ANY */
				if (!lpx_nullhost(*faddr)) {
					if (lpx_hosteq(lpxp->lpxp_laddr, *laddr)) {
//						DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("Maybe broadcast packet.PCBLU>> lpxp@(%lx) %02x:%02x:%02x:%02x:%02x:%02x lpxp_lport = %d, lpxp_fport = %d \n", lpxp, lpxp->lpxp_laddr.x_node[0], lpxp->lpxp_laddr.x_node[1], lpxp->lpxp_laddr.x_node[2], lpxp->lpxp_laddr.x_node[3], lpxp->lpxp_laddr.x_node[4], lpxp->lpxp_laddr.x_node[5], lpxp->lpxp_lport, lpxp->lpxp_fport));
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

	//		DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("PCBLU>> matchwild = %d, wildcard = %d\n",matchwild, wildcard));

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
	
	lck_rw_unlock_shared(head->lpxp_list_rw);

	//DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("PCBLU>> match@(%lx) matchwild = %d, wildcard = %d\n", match, matchwild, wildcard));
	
	if (match) {
//		DEBUG_PRINT(DEBUG_MASK_PCB_INFO, ("PCBLU>> match@(%lx) matchwild = %d, wildcard = %d\n", match, matchwild, wildcard));
	}
	
	return (match);
}


int Lpx_PCB_setsockaddr( register struct lpxpcb *lpxp,
						 struct sockaddr **nam )
{
    struct sockaddr_lpx *slpx;
    
	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_setsockaddr\n"));
	
	MALLOC(slpx, struct sockaddr_lpx *, sizeof *slpx, M_SONAME, M_WAITOK);
	if (slpx == NULL) {
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_setsockaddr: Can't alloc addr.\n"));

		return ENOBUFS;
	}
	
	bzero(slpx, sizeof *slpx);
	slpx->slpx_family = AF_LPX;
	slpx->slpx_len = sizeof(*slpx);
	
	slpx->sipx_addr = lpxp->lpxp_laddr;
	
	*nam = (struct sockaddr *)slpx;
	
	return 0;
}


int Lpx_PCB_setpeeraddr(register struct lpxpcb *lpxp,
						struct sockaddr **nam )
{	
	struct sockaddr_lpx *slpx;
    
	DEBUG_PRINT(DEBUG_MASK_PCB_TRACE, ("Lpx_PCB_setpeeraddr\n"));
	
	MALLOC(slpx, struct sockaddr_lpx *, sizeof *slpx, M_SONAME, M_WAITOK);
	if (slpx == NULL) {
		DEBUG_PRINT(DEBUG_MASK_PCB_ERROR, ("Lpx_PCB_setpeeraddr: Can't alloc addr.\n"));

		return ENOBUFS;
	}
	bzero(slpx, sizeof *slpx);
	slpx->slpx_family = AF_LPX;
	slpx->slpx_len = sizeof(*slpx);
	
	slpx->sipx_addr = lpxp->lpxp_faddr;
	
	*nam = (struct sockaddr *)slpx;
	
	return 0;
}
