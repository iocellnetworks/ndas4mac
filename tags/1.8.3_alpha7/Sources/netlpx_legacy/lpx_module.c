/***************************************************************************
 *
 *  lpx_module.c
 *
 *    LPX module wrapper : Starts and stops lpx module
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/random.h>
#include <sys/protosw.h>
#include <sys/domain.h>

#include <machine/spl.h>
#include <string.h>

#include <net/if.h>
#include <net/route.h>
#include <net/dlil.h>
#include <net/ethernet.h>
#include <net/kext_net.h>
#include <netinet/if_ether.h>

#include "lpx.h"
#include "lpx_if.h"
#include "lpx_var.h"
#include "lpx_pcb.h"
#include "lpx_input.h"
#include "lpx_module_int.h"
#include "lpx_control.h"
#include "lpx_pcb.h"

static  u_short allones[] = {-1, -1, -1};
static int lpxqmaxlen = IFQ_MAXLEN;

extern  struct protosw  lpxsw[];
extern  struct domain   lpxdomain;
extern  struct lpx_addr zerolpx_addr;

int lpx_proto_count = 3;
boolean_t lpx_initted = FALSE;
int debug_level;
struct lpx_rtentry g_RtTbl[MAX_LPX_ENTRY];
int g_iRtLastIndex = 0;

__u8 LPX_BROADCAST_NODE[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

kern_return_t netlpx_start (kmod_info_t * ki, void * d) 
{
    kern_return_t   retval = KERN_SUCCESS;
    int         funnel_state;
    int         i, j;
    struct protosw  *pr;
    
    
    debug_level = DEBUG_LEVEL;
    
    DEBUG_CALL(2, ("netlpx_start\n"));

    if(sizeof(struct sockaddr_lpx) != sizeof(struct sockaddr))
        DEBUG_CALL(2, ("sizeof struct sockaddr_lpx = %u, sizeof struct sockaddr = %u\n",
               (int)sizeof(struct sockaddr_lpx), (int)sizeof(struct sockaddr)));

        
    funnel_state = thread_funnel_set(network_flock, TRUE);

    net_add_domain(&lpxdomain);

    for(i=0, pr=&lpxsw[0]; i<lpx_proto_count; i++, pr++) {
        if((retval = net_add_proto(pr, &lpxdomain)) == KERN_SUCCESS) {
            lpx_initted = TRUE;
        } else {
            DEBUG_CALL(0, ("retval = %d, i=%d\n", retval, i));
            lpx_initted = FALSE;
            break;
        }
    }
    
    if(lpx_initted == FALSE) {
        for(j=0, pr=&lpxsw[0]; j<i; j++, pr++) {
            net_del_proto(pr->pr_type, pr->pr_protocol, &lpxdomain);
        }
        
        net_del_domain(&lpxdomain);
        lpx_initted = FALSE;
    }

    /* Initialize LPX routing table */
    bzero((void *)g_RtTbl, sizeof(struct lpx_rtentry)*MAX_LPX_ENTRY);
    
    thread_funnel_set(network_flock, funnel_state);
    
    return retval;
}

kern_return_t netlpx_stop (kmod_info_t * ki, void *data) 
{
    int         funnel_state;
    int         i;
    struct protosw  *pr;
    int         stat;
    
    DEBUG_CALL(2, ("netlpx_stop, lpx_initted = %d\n", lpx_initted));

    if(lpx_initted == FALSE)
        return KERN_SUCCESS;


    if(lpxpcb.lpxp_next != &lpxpcb)
        return EBUSY;

    if(lpxpcb.lpxp_prev != &lpxpcb)
        panic("lpx is unstable\n");

   if(lpxrawpcb.lpxp_next != &lpxrawpcb)
       return EBUSY;
   
   if(lpxrawpcb.lpxp_prev != &lpxrawpcb)
       panic("lpx is unstable\n");

    /* TODO: need to do the following properly */
    stat = lpx_MODULE_ifnet_detach("en0");
    stat = lpx_MODULE_ifnet_detach("en1");
	stat = lpx_MODULE_ifnet_detach("en2"); 
	stat = lpx_MODULE_ifnet_detach("en3");
        
    if(stat) {
        DEBUG_CALL(2, ("stat = %d\n", stat));
        return EBUSY;
    }
        
    funnel_state = thread_funnel_set(network_flock, TRUE);


    for(i=0, pr=&lpxsw[0]; i<lpx_proto_count; i++, pr++) {
        net_del_proto(pr->pr_type, pr->pr_protocol, &lpxdomain);
    }
    
    net_del_domain(&lpxdomain);

    thread_funnel_set(network_flock, funnel_state);
    
    return KERN_SUCCESS;
}


/*
 * LPX initialization.
 */
void Lpx_MODULE_init()
{
    lpx_broadnet = *(union lpx_net *)allones;
    lpx_broadhost = *(union lpx_host *)allones;
    read_random(&lpx_pexseq, sizeof lpx_pexseq);

    bzero(&zerolpx_addr, sizeof(zerolpx_addr));
    bzero(&lpxpcb, sizeof(lpxpcb));
    bzero(&lpxrawpcb, sizeof(lpxrawpcb));
    lpxpcb.lpxp_next = lpxpcb.lpxp_prev = &lpxpcb;
    lpxrawpcb.lpxp_next = lpxrawpcb.lpxp_prev = &lpxrawpcb;

    lpx_netmask.slpx_len = 6;
    lpx_netmask.sipx_addr.x_net = lpx_broadnet;

    lpx_hostmask.slpx_len = 12;
    lpx_hostmask.sipx_addr.x_net = lpx_broadnet;
    lpx_hostmask.sipx_addr.x_host = lpx_broadhost;

    bzero(&lpxintrq, sizeof(lpxintrq));
    lpxintrq.ifq_maxlen = lpxqmaxlen;
//  mtx_init(&lpxintrq.ifq_mtx, "lpx_inq", NULL, MTX_DEF);
//    netisr_register(NETISR_LPX, Lpx_IN_intr, &lpxintrq);

    lpx_ifaddr = NULL;
    
    DEBUG_CALL(2, ("Lpx_MODULE_init: lpxpcb = %lx, lpx_next = %lx, lpx_prev = %lx\n", &lpxpcb, lpxpcb.lpxp_next, lpxpcb.lpxp_prev));

	min_mtu = 1500;

    /* TODO: need to do the following properly */
    lpx_MODULE_ifnet_attach("en0");
    lpx_MODULE_ifnet_attach("en1");
    lpx_MODULE_ifnet_attach("en2");
    lpx_MODULE_ifnet_attach("en3");
}


/***************************************************************************
 *
 *  Internal helper functions
 *
 **************************************************************************/

void lpx_MODULE_ifnet_attach(const char *name)
{
    struct ifnet    *ifp;
    struct lpx_aliasreq ifreqa;

    DEBUG_CALL(2, ("lpx_MODULE_ifnet_attach\n"));

    ifp = ifunit(name);
    if(ifp == NULL)
        return;

    if(sizeof(((struct arpcom *)ifp)->ac_enaddr) == ETHER_ADDR_LEN)
    {
        int i;

        for(i=0; i<ETHER_ADDR_LEN; i++)
            DEBUG_CALL(2, ("ether_addr[%u] = 0x%02d\n", i, ((struct arpcom *)ifp)->ac_enaddr[i]));
    } else {
        DEBUG_CALL(2, ("size = %d\n", (int)(sizeof(((struct arpcom *)ifp)->ac_enaddr))));
        return;
    }

    bzero(&ifreqa, sizeof(ifreqa));
    bcopy(name, ifreqa.ifra_name, strlen(name)+1);

    ifreqa.ifra_addr.slpx_len = sizeof(struct sockaddr_lpx);
    ifreqa.ifra_addr.slpx_family = PF_LPX;
    bcopy(((struct arpcom *)ifp)->ac_enaddr, ifreqa.ifra_addr.slpx_node, LPX_NODE_LEN);
    ifreqa.ifra_broadaddr.slpx_len = sizeof(struct sockaddr_lpx);
    ifreqa.ifra_broadaddr.slpx_family = PF_LPX;
    bcopy(LPX_BROADCAST_NODE, ifreqa.ifra_broadaddr.slpx_node, LPX_NODE_LEN);

	// Check MTU.
	if(min_mtu > ifp->if_data.ifi_mtu) {
		min_mtu = ifp->if_data.ifi_mtu;
	}
	
    Lpx_CTL_control(NULL, SIOCSIFADDR, (caddr_t)&ifreqa, ifp, NULL);

    return;
}


int lpx_MODULE_ifnet_detach(const char *name)
{
    struct ifnet    *ifp;
    struct lpx_aliasreq ifreqa;
    int         stat;

    DEBUG_CALL(2, ("lpx_MODULE_ifnet_detach\n"));

    ifp = ifunit(name);
    if(ifp == NULL)
        return EADDRNOTAVAIL;

    if(sizeof(((struct arpcom *)ifp)->ac_enaddr) == ETHER_ADDR_LEN)
    {
        int i;

        for(i=0; i<ETHER_ADDR_LEN; i++)
            DEBUG_CALL(2, ("ether_addr[%u] = 0x%02d\n", i, ((struct arpcom *)ifp)->ac_enaddr[i]));
    } else {
        DEBUG_CALL(2, ("size = %d\n", (int)(sizeof(((struct arpcom *)ifp)->ac_enaddr))));

        return EADDRNOTAVAIL;
    }

    bzero(&ifreqa, sizeof(ifreqa));
    bcopy(name, ifreqa.ifra_name, strlen(name)+1);
    ifreqa.ifra_addr.slpx_len = sizeof(struct sockaddr_lpx);
    ifreqa.ifra_addr.slpx_family = PF_LPX;
    bcopy(((struct arpcom *)ifp)->ac_enaddr, ifreqa.ifra_addr.slpx_node, LPX_NODE_LEN);
    ifreqa.ifra_broadaddr.slpx_len = sizeof(struct sockaddr_lpx);
    ifreqa.ifra_broadaddr.slpx_family = PF_LPX;
    bcopy(LPX_BROADCAST_NODE, ifreqa.ifra_broadaddr.slpx_node, LPX_NODE_LEN);
    stat = Lpx_CTL_control(NULL, SIOCDIFADDR, (caddr_t)&ifreqa, ifp, NULL);
    if(stat) {
        u_long lpx_dl_tag;

        DEBUG_CALL(2, ("Lpx_CTL_control SIOCDIFADDR is fail\n"));
        stat = dlil_find_dltag(ifp->if_family, ifp->if_unit, PF_LPX, &lpx_dl_tag);
        if (stat != 0)
            return 0;
        else
            return EBUSY;
    }

    return 0;
}

