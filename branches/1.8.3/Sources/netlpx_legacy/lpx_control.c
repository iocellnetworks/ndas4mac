/***************************************************************************
 *
 *  lpx_control.c
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
 *  @(#)lpx.c
 *
 * $FreeBSD: src/sys/netlpx/lpx.c,v 1.25 2003/02/25 15:10:23 tjr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sockio.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netlpx/lpx.h>
#include <netlpx/lpx_if.h>
#include <netlpx/lpx_var.h>
#include <netlpx/lpx_control_int.h>
#include <netlpx/lpx_control.h>
#include <netlpx/lpx_ether_attach.h>

#include <machine/spl.h>
#include <net/if_types.h>
#include <vm/vm_kern.h>

#include <string.h>


struct lpx_ifaddr *lpx_ifaddr = NULL;
u_int32_t	min_mtu;

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

/*
 * Generic internet control operations (ioctl's).
 */
int Lpx_CTL_control( struct socket *so,
                     u_long cmd,
                     caddr_t data,
                     register struct ifnet *ifp,
                     struct proc *td)
{
    register struct ifreq *ifr = (struct ifreq *)data;
    register struct lpx_aliasreq *ifra = (struct lpx_aliasreq *)data;
    register struct lpx_ifaddr *ia;
    struct ifaddr *ifa;
    struct lpx_ifaddr *oia = NULL;
    int dstIsNew, hostIsNew;
    int error = 0;
	u_long  dl_tag;
	
    /*
     * Find address for this interface, if it exists.
     */
    if (ifp == NULL)
        return (EADDRNOTAVAIL);
	
    for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next)
        if (ia->ia_ifp == ifp)
            break;
	
    switch (cmd) {
		
		case SIOCGIFADDR:
			if (ia == NULL)
				return (EADDRNOTAVAIL);
			*(struct sockaddr_lpx *)&ifr->ifr_addr = ia->ia_addr;
			DEBUG_CALL(4, ("Get local IfAddr\n"));
			return (0);
			
		case SIOCGIFBRDADDR:
			if (ia == NULL)
				return (EADDRNOTAVAIL);
			if ((ifp->if_flags & IFF_BROADCAST) == 0)
				return (EINVAL);
				*(struct sockaddr_lpx *)&ifr->ifr_dstaddr = ia->ia_broadaddr;
			DEBUG_CALL(4, ("Get Broadcast IfAddr\n"));
			return (0);
			
		case SIOCGIFDSTADDR:
			if (ia == NULL)
				return (EADDRNOTAVAIL);
			if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
				return (EINVAL);
				*(struct sockaddr_lpx *)&ifr->ifr_dstaddr = ia->ia_dstaddr;
			DEBUG_CALL(4, ("Get Dest IfAddr\n"));
			return (0);
    }
	
    if (td && (error = suser(td->p_ucred, &td->p_acflag)) != 0)
        return (error);
	
    switch (cmd) {
		case SIOCAIFADDR:
		case SIOCDIFADDR:
			if (ifra->ifra_addr.slpx_family == AF_LPX) {
				for (oia = ia; ia != NULL; ia = ia->ia_next) {
					if (ia->ia_ifp == ifp  &&
						lpx_neteq(ia->ia_addr.sipx_addr,
								  ifra->ifra_addr.sipx_addr)) {
						break;
					}
				}
				if (cmd == SIOCDIFADDR && ia == NULL) {
					return (EADDRNOTAVAIL);
				}
			/* FALLTHROUGH */
			}
		case SIOCSIFADDR:
		case SIOCSIFDSTADDR:
			if (ia == NULL) {
#if 1
				oia = (struct lpx_ifaddr *)
				_MALLOC(sizeof(struct lpx_ifaddr), M_IFADDR, M_WAITOK);
				if (oia == NULL) {
					return (ENOBUFS);
				}
				bzero(oia, sizeof(struct lpx_ifaddr));
				oia->ia_next = NULL;
#else /* if 0 */
				kmem_alloc(kernel_map, (vm_offset_t *)oia, sizeof(struct lpx_ifaddr));
				if (oia == NULL) {
					return (ENOBUFS);
				}
				bzero(oia, sizeof(struct lpx_ifaddr));
#endif /* if 0 else */
				
				if ((ia = lpx_ifaddr) != NULL) {
					for ( ; ia->ia_next != NULL; ia = ia->ia_next)
						;
					ia->ia_next = oia;
				} else {
					lpx_ifaddr = oia;
				}
				
				ia = oia;
#if 0
				ifa = (struct ifaddr *)ia;
#else /* if 0 */
				ifa = (struct ifaddr *)(&(ia->ia_ifa));
#endif /* if 0 else */
#if CHECK == 1
				IFA_LOCK_INIT(ifa);
#endif                        
				ifa->ifa_refcnt = 1;
				TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);
				
				if(ifp->if_type == IFT_ETHER)
					dl_tag = Lpx_EA_ether_attach(ifp);
				else
					return EINVAL;
				
				ia->ia_ifp = ifp;
				ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
				
				ifa->ifa_dlt = dl_tag;
				
				ifa->ifa_netmask = (struct sockaddr *)&lpx_netmask;
				
				ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
				if (ifp->if_flags & IFF_BROADCAST) {
					ia->ia_broadaddr.slpx_family = AF_LPX;
					ia->ia_broadaddr.slpx_len = sizeof(ia->ia_addr);
					ia->ia_broadaddr.sipx_addr.x_host = lpx_broadhost;
				}
			}
	}
	
    switch (cmd) {
		
		case SIOCSIFDSTADDR:
			if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
				return (EINVAL);
			if (ia->ia_flags & IFA_ROUTE) {
				rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
				ia->ia_flags &= ~IFA_ROUTE;
			}
				if (ifp->if_ioctl) {
					error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR, (void *)ia);
					if (error)
						return (error);
				}
				*(struct sockaddr *)&ia->ia_dstaddr = ifr->ifr_dstaddr;
			return (0);
			
		case SIOCSIFADDR:
			return (lpx_CTL_ifinit(ifp, ia,
								   (struct sockaddr_lpx *)&ifr->ifr_addr, 1));
			
		case SIOCDIFADDR:
#if CHECK == 1
			lpx_CTL_ifscrub(ifp, ia);
#endif
			ifa = (struct ifaddr *)ia;
			TAILQ_REMOVE(&ifp->if_addrhead, ifa, ifa_link);
			oia = ia;
			if (oia == (ia = lpx_ifaddr)) {
				lpx_ifaddr = ia->ia_next;
			} else {
				while (ia->ia_next && (ia->ia_next != oia)) {
					ia = ia->ia_next;
				}
				if (ia->ia_next) {
					ia->ia_next = oia->ia_next;
				} else {
					DEBUG_CALL(2, ("Didn't unlink lpxifadr from list\n"));
				}
			}
				
				/* <<<<<<<<<<<<< Removed : no need to be freed cause it's not a pointer <<<<<<<<<<
				IFAFREE((&oia->ia_ifa));
				<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
				
                if(ifp->if_type == IFT_ETHER)
                    return Lpx_EA_ether_detach(ifp);
				
                return (0);
			
		case SIOCAIFADDR:
			dstIsNew = 0;
			hostIsNew = 1;
			if (ia->ia_addr.slpx_family == AF_LPX) {
				if (ifra->ifra_addr.slpx_len == 0) {
					ifra->ifra_addr = ia->ia_addr;
					hostIsNew = 0;
				} else if (lpx_neteq(ifra->ifra_addr.sipx_addr,
									 ia->ia_addr.sipx_addr))
					hostIsNew = 0;
			}
				if ((ifp->if_flags & IFF_POINTOPOINT) &&
					(ifra->ifra_dstaddr.slpx_family == AF_LPX)) {
					if (hostIsNew == 0)
						lpx_CTL_ifscrub(ifp, ia);
					ia->ia_dstaddr = ifra->ifra_dstaddr;
					dstIsNew  = 1;
				}
				if (ifra->ifra_addr.slpx_family == AF_LPX &&
					(hostIsNew || dstIsNew))
					error = lpx_CTL_ifinit(ifp, ia, &ifra->ifra_addr, 0);
				return (error);
			
		default:
			if (ifp->if_ioctl == NULL)
				return (EOPNOTSUPP);
			return ((*ifp->if_ioctl)(ifp, cmd, data));
    }
}


/*
 * Return address info for specified internet network.
 */
struct lpx_ifaddr * Lpx_CTL_iaonnetof( register struct lpx_addr *dst )
{
    register struct lpx_ifaddr *ia;
    register struct lpx_addr *compare;
    register struct ifnet *ifp;
    struct lpx_ifaddr *ia_maybe = NULL;
    union lpx_net net = dst->x_net;

    DEBUG_CALL(2, ("dst->x_net = %x\n", dst->x_net));
    
    for (ia = lpx_ifaddr; ia != NULL; ia = ia->ia_next) {
        if ((ifp = ia->ia_ifp) != NULL) {
            if (ifp->if_flags & IFF_POINTOPOINT) {
                compare = &satolpx_addr(ia->ia_dstaddr);
                if (lpx_hosteq(*dst, *compare))
                    return (ia);
                if (lpx_neteqnn(net, ia->ia_addr.sipx_addr.x_net))
                    ia_maybe = ia;
            } else {
#if 0
                if (lpx_neteqnn(net, ia->ia_addr.sipx_addr.x_net))
                    return (ia);
#else /* if 0 */
                /* TODO: ugly code <<<< NEED MODIFICATION !!!!!! */
                int index =0;
                for (index = 0; index < MAX_LPX_ENTRY; index++) {
                  if (!lpx_nullhost(g_RtTbl[index].ia_faddr)) {
                    if (lpx_hosteq(*dst, g_RtTbl[index].ia_faddr)) {
                      if (ifp == g_RtTbl[index].rt_ifp) {
                        return (ia);
                      } else {
                        /* Address is not for this if addr */
                        break;
                      }
                    }
                  }
                }
#endif /* if 0 else */
            }
        }
    }
    return (ia_maybe);
}


void Lpx_CTL_printhost( register struct lpx_addr *addr )
{
    u_short port;
    struct lpx_addr work = *addr;
    register char *p; register u_char *q;
    register char *net = "", *host = "";
    char cport[10], chost[15], cnet[15];

    port = ntohs(work.x_port);

    if (lpx_nullnet(work) && lpx_nullhost(work)) {

        if (port) {
            DEBUG_CALL(2, ("*.%x", port));
        } else {
            DEBUG_CALL(2, ("*.*"));
        }

        return;
    }

    if (lpx_wildnet(work))
        net = "any";
    else if (lpx_nullnet(work))
        net = "*";
    else {
        q = work.x_net.c_net;
        snprintf(cnet, sizeof(cnet), "%x%x%x%x",
            q[0], q[1], q[2], q[3]);
        for (p = cnet; *p == '0' && p < cnet + 8; p++)
            continue;
        net = p;
    }

    if (lpx_wildhost(work))
        host = "any";
    else if (lpx_nullhost(work))
        host = "*";
    else {
        q = work.x_host.c_host;
        snprintf(chost, sizeof(chost), "%x%x%x%x%x%x",
            q[0], q[1], q[2], q[3], q[4], q[5]);
        for (p = chost; *p == '0' && p < chost + 12; p++)
            continue;
        host = p;
    }

    if (port) {
        if (strcmp(host, "*") == 0) {
            host = "";
            snprintf(cport, sizeof(cport), "%x", port);
        } else
            snprintf(cport, sizeof(cport), ".%x", port);
    } else
        *cport = 0;

    DEBUG_CALL(2, ("%s.%s%s", net, host, cport));
}


/***************************************************************************
 *
 *  Internal helper functions
 *
 **************************************************************************/

/*
 * Delete any previous route for an old address.
 */
static void lpx_CTL_ifscrub( register struct ifnet *ifp,
                             register struct lpx_ifaddr *ia )
{
    if (ia->ia_flags & IFA_ROUTE) {
        if (ifp->if_flags & IFF_POINTOPOINT) {
            rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
        } else
            rtinit(&(ia->ia_ifa), (int)RTM_DELETE, 0);
        ia->ia_flags &= ~IFA_ROUTE;
    }
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int lpx_CTL_ifinit( register struct ifnet *ifp,
                           register struct lpx_ifaddr *ia,
                           register struct sockaddr_lpx *slpx,
                           int scrub )
{
    struct sockaddr_lpx oldaddr;
    int s = splimp(), error;

    /*
     * Set up new addresses.
     */
    oldaddr = ia->ia_addr;
    ia->ia_addr = *slpx;

    /*
     * The convention we shall adopt for naming is that
     * a supplied address of zero means that "we don't care".
     * Use the MAC address of the interface. If it is an
     * interface without a MAC address, like a serial line, the
     * address must be supplied.
     *
     * Give the interface a chance to initialize
     * if this is its first address,
     * and to validate the address if necessary.
     */
    if (ifp->if_ioctl != NULL &&
        (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (void *)ia))) {
        ia->ia_addr = oldaddr;
        splx(s);
        return (error);
    }
    splx(s);
    ia->ia_ifa.ifa_metric = ifp->if_metric;
    /*
     * Add route for the network.
     */
    if (scrub) {
        ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
        lpx_CTL_ifscrub(ifp, ia);
        ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
    }
    if (ifp->if_flags & IFF_POINTOPOINT)
        rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
    else {
        ia->ia_broadaddr.sipx_addr.x_net = ia->ia_addr.sipx_addr.x_net;
        rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_UP);
    }
    ia->ia_flags |= IFA_ROUTE;
    return (0);
}

