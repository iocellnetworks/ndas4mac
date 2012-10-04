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
#include <net/route.h>
#include <net/dlil.h>
#include <net/ethernet.h>
#include <net/kext_net.h>
#include <netinet/if_ether.h>

#include <mach/mach_types.h>

#include "lpx.h"
#include "lpx_pcb.h"
#include "lpx_base.h"
#include "lpx_stream.h"
#include "lpx_datagram.h"
#include "Utilities.h"
#include "lpx_if.h"

// Init Flag.
boolean_t lpx_initted = FALSE;

extern  struct protosw  lpxsw[];
extern  struct domain   lpxdomain;
extern	int lpx_proto_count;

lck_attr_t			*domain_mtx_attr;      // mutex attributes
lck_grp_t           *domain_mtx_grp;       // mutex group definition 
lck_grp_attr_t		*domain_mtx_grp_attr;  // mutex group attributes
lck_mtx_t            *domain_mtx;           // global mutex for the pcblist

kern_return_t lpx_start(kmod_info_t * ki, void * d) {
	
	kern_return_t   retval = KERN_SUCCESS;
	int				i, j;
    struct protosw  *pr;
		
	DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("lpx_start: Entered.\n"));
	
	// Check lpx address structure size.
    if (sizeof(struct sockaddr_lpx) != sizeof(struct sockaddr))
        DEBUG_PRINT(DEBUG_MASK_NKE_ERROR, ("sizeof struct sockaddr_lpx = %u, sizeof struct sockaddr = %u\n",
										   (int)sizeof(struct sockaddr_lpx), (int)sizeof(struct sockaddr)));
	
	// Add Domain.
    net_add_domain(&lpxdomain);
	
	// Add Protocols.
    for(i = 0, pr = &lpxsw[0]; i < lpx_proto_count; i++, pr++) {
        if ((retval = net_add_proto(pr, &lpxdomain)) == KERN_SUCCESS) {
            lpx_initted = TRUE;
        } else {
            DEBUG_PRINT(DEBUG_MASK_NKE_ERROR, ("retval = %d, i=%d\n", retval, i));
            lpx_initted = FALSE;
            break;
        }
    }
    
	// Delete Protocls and domain if failed.
    if (lpx_initted == FALSE) {
        for(j=0, pr=&lpxsw[0]; j<i; j++, pr++) {
            net_del_proto(pr->pr_type, pr->pr_protocol, &lpxdomain);
        }
        
        net_del_domain(&lpxdomain);
        lpx_initted = FALSE;
    }
	
	min_mtu = 1500;
	
	lpx_ifnet_attach_protocol();
	
    return retval;
}


kern_return_t lpx_stop(kmod_info_t * ki, void * d) {
    
	int				i;
	int				errno;
    struct protosw  *pr;
	
	DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("lpx_stop: Entered.\n"));
	
	if (lpx_initted == FALSE) {
		DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("%d\n", __LINE__)); debugLevel = 0x33333333;
        return KERN_SUCCESS;
	}
	
    if (lpx_datagram_pcb.lpxp_next != &lpx_datagram_pcb) {
		DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("%d\n", __LINE__)); debugLevel = 0x33333333;
		return EBUSY;
	}
	
    if (lpx_datagram_pcb.lpxp_prev != &lpx_datagram_pcb)
        panic("lpx is unstable\n");
	
	if (lpx_stream_pcb.lpxp_next != &lpx_stream_pcb) {
		DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("%d\n", __LINE__)); debugLevel = 0x33333333;
		return EBUSY;
	}
	
	if (lpx_stream_pcb.lpxp_prev != &lpx_stream_pcb)
		panic("lpx is unstable\n");
	
	lpx_stream_free();
	
	lpx_datagram_free();
	
	lpx_ifnet_detach_protocol();
	
    for		(i=0, pr=&lpxsw[0]; i<lpx_proto_count; i++, pr++) {
        net_del_proto(pr->pr_type, pr->pr_protocol, &lpxdomain);
    }
    
	errno = net_del_domain(&lpxdomain);
	
	if (EBUSY == errno && 0 != lpxdomain.dom_refs) {
		DEBUG_PRINT(DEBUG_MASK_NKE_ERROR, ("ERROR!!! : lpxdomain.dom_refs(%d). retry with dom_refs = 0\n", lpxdomain.dom_refs));
		lpxdomain.dom_refs = 0;
		errno = net_del_domain(&lpxdomain);
	}
	
	if (0 != errno) {
		DEBUG_PRINT(DEBUG_MASK_NKE_ERROR, ("ERROR!!! : lpxdomain.dom_refs. Fails to stop\n"));
		return errno;
	}
	
	DEBUG_PRINT(DEBUG_MASK_NKE_TRACE, ("%d\n", __LINE__)); debugLevel = 0x33333333;
	return KERN_SUCCESS;
}
