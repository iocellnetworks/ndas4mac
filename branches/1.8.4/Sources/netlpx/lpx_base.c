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
//#include <net/dlil.h>

#include <mach/mach_types.h>

#include "Utilities.h"
#include "lpx.h"
#include "lpx_var.h"
#include "lpx_pcb.h"
#include "lpx_if.h"
#include "lpx_stream.h"
#include "lpx_datagram.h"

mbuf_tag_id_t	lpxTagId;

int lpxcksum = 0;
union   lpx_net lpx_zeronet;

struct  lpxstat lpxstat;

// PCB.
struct  lpxpcb lpx_datagram_pcb;
struct  lpxpcb lpx_stream_pcb;

long lpx_pexseq;

struct lpx_rtentry g_RtTbl[MAX_LPX_ENTRY];
int g_iRtLastIndex = 0;

extern struct domain lpxdomain;

static  u_short allones[] = {-1, -1, -1};
 union lpx_host lpx_broadhost;

// List Lock.
extern lck_rw_t *stream_mtx;
extern lck_rw_t *datagram_mtx;

void lpx_domain_init()
{
	lpx_broadhost = *(union lpx_host *)allones;
	
	return;
}

void lpx_init()
{
	errno_t	error;
	
	DEBUG_PRINT(DEBUG_MASK_LPX_TRACE, ("lpx_init: Entered.\n"));
	
	// Find mbuf tag id
	error = mbuf_tag_id_find("com.ximeta.nke.lpx", &lpxTagId);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_init: Can't find tag id Error : %d\n", error));
	}
	
	// Init PCB.
	bzero(&lpx_datagram_pcb, sizeof(lpx_datagram_pcb));
    lpx_datagram_pcb.lpxp_next = lpx_datagram_pcb.lpxp_prev = &lpx_datagram_pcb;
	lpx_datagram_pcb.lpxp_lport = LPXPORT_RESERVED;
	
	bzero(&lpx_stream_pcb, sizeof(lpx_stream_pcb));
    lpx_stream_pcb.lpxp_next = lpx_stream_pcb.lpxp_prev = &lpx_stream_pcb;
	lpx_stream_pcb.lpxp_lport = LPXPORT_RESERVED;
	
	// Initialize LPX routing table
    bzero((void *)g_RtTbl, sizeof(struct lpx_rtentry) * MAX_LPX_ENTRY);
		
	return;
}

void lpx_input(mbuf_t packet, int len)
{
	struct lpxhdr		*lpxHeader;
	struct lpx_addr sna_addr;
	struct lpx_addr	dna_addr;
	errno_t error;
	size_t	tagLength;
	struct ether_header *etherHeader;
	struct lpxpcb		*lpxp;
	
	DEBUG_PRINT(DEBUG_MASK_LPX_TRACE, ("lpx_input: Entered.\n"));
		
	lpxHeader = (struct lpxhdr *)mbuf_data(packet);

	// Copy Address.
	error = mbuf_tag_find(
						  packet,
						  lpxTagId,
						  LPX_TAG_TYPE_ETHERNET_ADDR,
						  &tagLength,
						  (void **)&etherHeader
						  ); 
	
	if (error != 0) {
		DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_input: Can't get tag error: %d\n", error));
		
		// Free mbuf.
		mbuf_freem(packet);
		return;
	}
	
	// free Tag.
	mbuf_tag_free(
				  packet,
				  lpxTagId,
				  LPX_TAG_TYPE_ETHERNET_ADDR
				  );
	
	bzero(&sna_addr, sizeof(sna_addr));
	bzero(&dna_addr, sizeof(dna_addr));
	
	bcopy(etherHeader->ether_shost, sna_addr.x_node, LPX_NODE_LEN);
	sna_addr.x_port = lpxHeader->source_port;
	
	bcopy(etherHeader->ether_dhost, dna_addr.x_node, LPX_NODE_LEN);
	dna_addr.x_port = lpxHeader->dest_port;

#if NDAS_MACOSXVERSION == 5
	/* xnu-1228 does not lock for non-reenterable protocol */
	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
	lck_mtx_lock(lpxdomain.dom_mtx);
#endif

	switch (lpxHeader->pu.p.type) {
		case LPX_TYPE_DATAGRAM:
		{
			DEBUG_PRINT(DEBUG_MASK_LPX_INFO, ("lpx_input: Datagram packet\n"));			   
			
			lpxp = Lpx_PCB_lookup(&sna_addr, &dna_addr, LPX_WILDCARD, &lpx_datagram_pcb);
			if (lpxp == NULL) {
				DEBUG_PRINT(DEBUG_MASK_LPX_INFO, ("lpx_input: No PCB. Drop.\n"));
				
				// Free mbuf.
				mbuf_freem(packet);
				break;
			}
			
			lpx_datagram_input(packet, lpxp, etherHeader);
									 
		}
			break;
		case LPX_TYPE_STREAM:
		{
			DEBUG_PRINT(DEBUG_MASK_LPX_INFO, ("lpx_input: Stream packet\n"));

			lpxp = Lpx_PCB_lookup(&sna_addr, &dna_addr, LPX_WILDCARD, &lpx_stream_pcb);
			if (lpxp == NULL) {
				DEBUG_PRINT(DEBUG_MASK_LPX_INFO, ("lpx_input: No PCB. Drop.\n"));
				
				// Free mbuf.
				mbuf_freem(packet);
				break;
			}
						
			lpx_stream_input((struct mbuf *)packet, lpxp, etherHeader);
		}
			break;
		case LPX_TYPE_RAW:
		default:
			DEBUG_PRINT(DEBUG_MASK_LPX_INFO, ("lpx_input: Unsupported packet %d\n", lpxHeader->pu.p.type));
			
			// Free mbuf.
			mbuf_freem(packet);
			break;
	}
		
#if NDAS_MACOSXVERSION == 5
	/* xnu-1228 does not lock for non-reenterable protocol */
	lck_mtx_assert(lpxdomain.dom_mtx, LCK_MTX_ASSERT_OWNED);
	lck_mtx_unlock(lpxdomain.dom_mtx);
#endif
	return;
}

int	lpx_output( mbuf_t m0,
				int flags,
				struct lpxpcb *lpxp)
{

	register struct lpxhdr *lpxh = (struct lpxhdr *)mbuf_data(m0);
    int error = 0;
	ifnet_t	interface;
	
    DEBUG_PRINT(DEBUG_MASK_LPX_TRACE, ("lpx_output: Entered. size %d, mbuf size %zu\n", ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK), mbuf_pkthdr_len(m0)));
		
    if (lpxp == NULL) {
        DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_output: lpxp is NULL\n"));
        return EINVAL;
    }

	// Find interface.	
	interface = lpx_find_ifnet_by_addr(&(lpxp->lpxp_laddr));
	if (interface == 0) {
		DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_output: Can't Find interface.\n"));
		
		return EINVAL;			
	}

	// Check packet Size.
    if (ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK) <= ifnet_mtu(interface)) {

		struct sockaddr_lpx dest_addr;
		        
        lpxstat.lpxs_localout++;
		
        dest_addr.slpx_family = AF_LPX;
        dest_addr.slpx_len = sizeof(dest_addr);
        dest_addr.sipx_addr = lpxp->lpxp_faddr;
		
		error = lpx_ifnet_output(
						 interface,
						 (mbuf_t)m0,
						 (struct sockaddr *)&dest_addr
						 );
		if (error != 0) {
			DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_output: lpx_ifnet_output error.\n"));
			
			return error;
		}
		
        goto done;
    } else {
		DEBUG_PRINT(DEBUG_MASK_LPX_ERROR, ("lpx_output: Too small packet %d, mtu %d\n", ntohs(lpxh->pu.pktsize & ~LPX_TYPE_MASK), ifnet_mtu(interface)));
		
        lpxstat.lpxs_mtutoosmall++;
        error = EMSGSIZE;
    }
bad:
	mbuf_freem(m0);

done:
	
    return (error);	
}

/*
 * Return address info for specified internet network.
 */
struct lpx_ifaddr * lpx_iaonnetof( register struct lpx_addr *dst )
{
    register struct lpx_ifaddr *ia;
    register ifnet_t ifp;
    struct lpx_ifaddr *ia_maybe = NULL;
 	
    DEBUG_PRINT(DEBUG_MASK_LPX_TRACE, ("lpx_iaonnetof: Entered.\n")); // dst->x_net = %x\n", dst->x_net));
    		
    for (ia = lpx_ifaddrs; ia != NULL; ia = ia->ia_next) {
        if ((ifp = (ifnet_t)ia->ia_ifp) != NULL) {
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
        }
    }
    return (ia_maybe);
}

int lpx_peeraddr( struct socket *so,
                       struct sockaddr **nam )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
    Lpx_PCB_setpeeraddr(lpxp, nam); /* XXX what if alloc fails? */
    return (0);
}


int lpx_sockaddr( struct socket *so,
                       struct sockaddr **nam )
{
    struct lpxpcb *lpxp = sotolpxpcb(so);
	
    Lpx_PCB_setsockaddr(lpxp, nam); /* XXX what if alloc fails? */
    return (0);
}
