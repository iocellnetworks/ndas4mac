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
#include "lpx_if.h"
#include "lpx_var.h"
#include "lpx_base.h"

struct lpx_ifaddr *lpx_ifaddrs = NULL;
u_int32_t	min_mtu;

errno_t lpx_ifnet_input(ifnet_t ifp, protocol_family_t protocol,
						mbuf_t packet, char* header);

ifnet_t lpx_find_ifnet_by_addr(struct lpx_addr* lpx_addr)
{
	struct lpx_ifaddr *ifa;
	
	if (lpx_ifaddrs == NULL) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_find_ifnet_by_addr: lpx_ifaddrs == NULL. Calling lpx_ifnet_attach_protocol() again\n"));
		lpx_ifnet_attach_protocol();
	}
	
	if (lpx_addr == NULL || lpx_ifaddrs == NULL) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_find_ifnet_by_addr: lpx_addr(%p) or lpx_ifaddrs(%p) is NULL\n", lpx_addr, lpx_ifaddrs));
		
		return NULL;
	}
	
	ifa = lpx_ifaddrs;
	
	for(ifa = lpx_ifaddrs; ifa != NULL; ifa = ifa->ia_next) {
		
		struct lpx_addr* lpx_addr2;
		
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_find_ifnet_by_addr: lpx_addr %2x:%2x:%2x:%2x:%2x:%2x\n",
										  lpx_addr->x_host.c_host[0], lpx_addr->x_host.c_host[1], lpx_addr->x_host.c_host[2], 
										  lpx_addr->x_host.c_host[3], lpx_addr->x_host.c_host[4], lpx_addr->x_host.c_host[5] ));
		
		lpx_addr2 = &satolpx_addr(ifa->ia_addr);
		
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_find_ifnet_by_addr: lpx_addr %2x:%2x:%2x:%2x:%2x:%2x\n",
										  lpx_addr2->x_host.c_host[0], lpx_addr2->x_host.c_host[1], lpx_addr2->x_host.c_host[2], 
										  lpx_addr2->x_host.c_host[3], lpx_addr2->x_host.c_host[4], lpx_addr2->x_host.c_host[5] ));
		
		if (lpx_hosteq(*lpx_addr, satolpx_addr(ifa->ia_addr)) ) {
			return (ifnet_t)ifa->ia_ifp;
		}
	}
	
	return NULL;
}

errno_t lpx_ifnet_pre_output(ifnet_t ifp, 
							 protocol_family_t protocol,
							 mbuf_t *packet, 
							 const struct sockaddr *dest,
							 void *route, 
							 char *frame_type, 
							 char *link_layer_dest)
{
	 struct sockaddr_lpx *dest_addr;
	
	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_pre_output: Entered.\n"));
	
	if (!dest)
        return EAFNOSUPPORT;
	
    dest_addr = (struct sockaddr_lpx *)dest;
    
    if (dest_addr->slpx_family != AF_LPX) {
        return EAFNOSUPPORT;
	}
	
	// Assign Packet Type and Dest Address.
	*(__u16 *)frame_type = htons(ETHERTYPE_LPX);
	bcopy(dest_addr->slpx_node, link_layer_dest, LPX_NODE_LEN); 
    	
	return 0;
}

int lpx_ifnet_attach_by_name(const char *name)
{
	errno_t							error;
	ifnet_t							interface = NULL;
	struct ifnet_attach_proto_param	pr;
	struct ifnet_demux_desc			ddesc[1];					
	u_short							etherTypeLpx = htons(ETHERTYPE_LPX);
	struct lpx_ifaddr				*oia, *ia;
//	struct ifaddr					*ifa;
	unsigned char					ifAddress[6];
	
	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_attach: Entered.\n"));
	
	// Find ifnet_t by name such as "en0"
	error = ifnet_find_by_name(name, &interface);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_attach: Can't find the interface of %s.\n", name));
		
		return error;
	} else {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_attach: Found the interface of %s.\n", name));	
	}
	
	// Check ifnet type.
	if ( IFT_ETHER != ifnet_type(interface) ) {
		DEBUG_PRINT(DEBUG_MASK_IF_WARING, ("lpx_ifnet_attach: Type of interface of %s is not ethernet.\n", name));
		
		error = ENOENT;
		goto Out;
	}
	
	// fill out pr and attach the protocol 
	ddesc[0].type = DLIL_DESC_ETYPE2;
	ddesc[0].data = &etherTypeLpx;
	ddesc[0].datalen = sizeof(etherTypeLpx);
	pr.demux_array = ddesc;
	pr.demux_count = 1;
	pr.input = lpx_ifnet_input;
	pr.pre_output = lpx_ifnet_pre_output;
	pr.event = NULL;
	pr.ioctl = NULL;
	pr.detached = NULL;
	pr.resolve = NULL;
	pr.send_arp = NULL;
	
	error = ifnet_attach_protocol(interface, AF_LPX, &pr);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_WARING, ("lpx_ifnet_attach: Can't attach protocol to interface %s.\n", name));
		
		goto Out;
	}
	
	MALLOC(oia, struct lpx_ifaddr *, sizeof *oia, M_IFADDR, M_WAITOK);
	if (oia == NULL) {		
		error = ENOBUFS;
		
		goto Out;
	}
	
	bzero(oia, sizeof(struct lpx_ifaddr));
	oia->ia_next = NULL;

	error = ifnet_lladdr_copy_bytes(
									interface,
									ifAddress,
									6	// Ethernet.
									);
	if (error != 0) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_attach: Can't get address of interface %s.\n", name));
		
		goto Out;
	}
					
	DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_attach: Address of %s: %2x:%2x:%2x:%2x:%2x:%2x\n"
									 , name, ifAddress[0], ifAddress[1], ifAddress[2], 
									 ifAddress[3], ifAddress[4], ifAddress[5] ));
	
	if (min_mtu > ifnet_mtu(interface)) {
		min_mtu = ifnet_mtu(interface);
	}
	
	bcopy(ifAddress, &(satolpx_addr(oia->ia_addr).x_host), 6);
			
//	ifa = &oia->ia_ifa;
	
//	ifa->ifa_addr = (struct sockaddr *)&oia->ia_addr;
//	ifa->ifa_dstaddr = (struct sockaddr *)&oia->ia_dstaddr;
//	ifa->ifa_netmask = (struct sockaddr *)&oia->ia_sockmask;
	
	//ifnet_lock_exclusive((struct ifnet *)interface);
	
	oia->ia_ifp = (struct ifnet *)interface;

	//if_attach_ifa((struct ifnet *)interface, ifa);
	//ifnet_lock_done((struct ifnet *)interface);

	if ((ia = lpx_ifaddrs) != NULL) {
		for ( ; ia->ia_next != NULL; ia = ia->ia_next)
			;
		ia->ia_next = oia;
	} else {
		lpx_ifaddrs = oia;
	}           
	
Out:
	if (interface) {
		ifnet_release(interface);
	}
	
	return 0;
}

void lpx_ifnet_detach_by_name(const char *name)
{
	errno_t	error;
	ifnet_t	interface = NULL;
	struct lpx_ifaddr *ia, *oia;
	
	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_detach: Entered.\n"));
	
	// Find ifnet_t by name such as "en0"
	error = ifnet_find_by_name(name, &interface);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_detach: Can't find the interface of %s.\n", name));
		
		return;
	}
				
	error = ifnet_detach_protocol(interface, AF_LPX);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Can't detach protocol from interface %s.\n", name));
		
		goto Out;
	}
	
	// Search oia.
	oia = NULL;
	ia = lpx_ifaddrs;
	for(ia = lpx_ifaddrs; ia != NULL; ia = ia->ia_next) {
		if ((ifnet_t)ia->ia_ifp == interface) {
			oia = ia;
			break;
		}
	}

	if (oia == NULL) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Didn't find oia.\n"));
		goto Out;
	}
	
//	ifnet_lock_exclusive((struct ifnet *)interface);	
//	if_detach_ifa((struct ifnet *)interface, &oia->ia_ifa);
//	ifnet_lock_done((struct ifnet *)interface);
		
	// Unlink oia.
	if (oia == (ia = lpx_ifaddrs)) {
		lpx_ifaddrs = ia->ia_next;
	} else {
		while (ia->ia_next && (ia->ia_next != oia)) {
			ia = ia->ia_next;
		}
		if (ia->ia_next) {
			ia->ia_next = oia->ia_next;
		} else {
			DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Didn't unlink lpxifadr from list\n"));
		}
	}
	
	// Free oia.
	FREE(oia, M_IFADDR);
	
Out:
	if (interface) {
		ifnet_release(interface);
	}
	
	return;
}

errno_t
lpx_ifnet_attach(ifnet_t ifp)
{
	errno_t							error;
	struct ifnet_attach_proto_param	pr;
	struct ifnet_demux_desc			ddesc[1];					
	u_short							etherTypeLpx = htons(ETHERTYPE_LPX);
	struct lpx_ifaddr				*oia, *ia;
	unsigned char					ifAddress[6];
	
	DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_attach: Entered. ifp(%p)\n", ifp));
		
	// Check ifnet type.
	if ( IFT_ETHER != ifnet_type(ifp) ) {
		DEBUG_PRINT(DEBUG_MASK_IF_WARING, ("lpx_ifnet_attach: Type of interface of %s is not ethernet.\n", ifnet_name(ifp)));
		
		error = ENOENT;
		goto Out;
	}
	
	// fill out pr and attach the protocol 
	ddesc[0].type = DLIL_DESC_ETYPE2;
	ddesc[0].data = &etherTypeLpx;
	ddesc[0].datalen = sizeof(etherTypeLpx);
	pr.demux_array = ddesc;
	pr.demux_count = 1;
	pr.input = lpx_ifnet_input;
	pr.pre_output = lpx_ifnet_pre_output;
	pr.event = NULL;
	pr.ioctl = NULL;
	pr.detached = NULL;
	pr.resolve = NULL;
	pr.send_arp = NULL;
	
	error = ifnet_attach_protocol(ifp, AF_LPX, &pr);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_WARING, ("lpx_ifnet_attach: Can't attach protocol to interface %s.\n", ifnet_name(ifp)));
		
		goto Out;
	}
	
	MALLOC(oia, struct lpx_ifaddr *, sizeof *oia, M_IFADDR, M_WAITOK);
	if (oia == NULL) {		
		error = ENOBUFS;
		
		goto Out;
	}
	
	bzero(oia, sizeof(struct lpx_ifaddr));
	oia->ia_next = NULL;
	
	error = ifnet_lladdr_copy_bytes(
									ifp,
									ifAddress,
									6	// Ethernet.
									);
	if (error != 0) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_attach: Can't get address of interface %s.\n", ifnet_name));
		
		goto Out;
	}
	
	DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_attach: Address of %s: %2x:%2x:%2x:%2x:%2x:%2x\n",
									 ifnet_name(ifp), ifAddress[0], ifAddress[1], ifAddress[2], 
									 ifAddress[3], ifAddress[4], ifAddress[5] ));
	
	if (min_mtu > ifnet_mtu(ifp)) {
		min_mtu = ifnet_mtu(ifp);
	}
	
	bcopy(ifAddress, &(satolpx_addr(oia->ia_addr).x_host), 6);
	oia->ia_ifp = (struct ifnet *)ifp;
	
	if ((ia = lpx_ifaddrs) != NULL) {
		for ( ; ia->ia_next != NULL; ia = ia->ia_next)
			;
		ia->ia_next = oia;
	} else {
		lpx_ifaddrs = oia;
	}           
	
Out:
		
	return error;
}

errno_t
lpx_ifnet_detach(ifnet_t ifp)
{
	errno_t	error;
	struct lpx_ifaddr *ia, *oia;
	
	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_detach: Entered. ifp(%p)\n", ifp));
		
	error = ifnet_detach_protocol(ifp, AF_LPX);
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Can't detach protocol from interface %s.\n",  ifnet_name(ifp)));
		
		goto Out;
	}
	
	// Search oia.
	oia = NULL;
	ia = lpx_ifaddrs;
	for (ia = lpx_ifaddrs; ia != NULL; ia = ia->ia_next) {
		if (ia->ia_ifp == (struct ifnet *)ifp) {
			oia = ia;
			break;
		}
	}
	
	if (oia == NULL) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Didn't find oia.\n"));
		goto Out;
	}
	
	// Unlink oia.
	if (oia == (ia = lpx_ifaddrs)) {
		lpx_ifaddrs = ia->ia_next;
	} else {
		while (ia->ia_next && (ia->ia_next != oia)) {
			ia = ia->ia_next;
		}
		if (ia->ia_next) {
			ia->ia_next = oia->ia_next;
		} else {
			DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_detach: Didn't unlink lpxifadr from list\n"));
		}
	}
	
	// Free oia.
	FREE(oia, M_IFADDR);
	
Out:
	
	return error;
}	

typedef errno_t (*interface_function)(ifnet_t ifp);

errno_t
lpx_ifnet_enum_interfaces(interface_function func)
{
	errno_t errno = 0;
	ifnet_t *interfaces = NULL;
	u_int32_t i, count = 0;
	errno = ifnet_list_get(IFNET_FAMILY_ETHERNET, &interfaces, &count);
	DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("ifnet_list_get returned interfaces(%p), count(%d), errno(%d)\n", interfaces, count, errno));
	
	if (0 != errno)
		return errno;

	if (NULL == interfaces)
		return 0;
		
	for (i = 0; i < count; i++) {
		errno = func(interfaces[i]);
		if (0 != errno) {
			DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("function %p(%p) failed\n", func, interfaces[i]));
		}
	}

	ifnet_list_free(interfaces);
	interfaces = NULL;
	
	return errno;
}

errno_t
lpx_ifnet_attach_protocol()
{
	return lpx_ifnet_enum_interfaces(lpx_ifnet_attach);
}

errno_t
lpx_ifnet_detach_protocol()
{
	return lpx_ifnet_enum_interfaces(lpx_ifnet_detach);
}


static unsigned char ether_broadcasst_address[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

errno_t lpx_ifnet_input(ifnet_t ifp, protocol_family_t protocol,
									 mbuf_t packet, char* header)
{
	struct ether_header *etherHeader = (struct ether_header *) header;
	struct lpxhdr		*lpxHeader;
	__u16				packetSize;
	errno_t				error;
	char				*ethernetTag;
	int					iIndex;

	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_input: Entered.\n"));
	
	// Check Protocol.
	if ( AF_LPX != protocol ) {
		DEBUG_PRINT(DEBUG_MASK_IF_ERROR, ("lpx_ifnet_input: Protocol is not LPX 0x%x\n", protocol));

		// Free mbuf.
		mbuf_freem(packet);
		
		return EJUSTRETURN;
	}
	
	// Look for peer address table for interface, and register if it's new
	for (iIndex = 0; iIndex < MAX_LPX_ENTRY; iIndex++) {
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("Lpx_EA_ether_input: g_RtTbl[%d]=%02x:%02x:%02x:%02x:%02x:%02x <=> %02x:%02x:%02x:%02x:%02x:%02x \n",
					   iIndex, 
					   g_RtTbl[iIndex].ia_faddr.x_node[0], g_RtTbl[iIndex].ia_faddr.x_node[1], g_RtTbl[iIndex].ia_faddr.x_node[2], g_RtTbl[iIndex].ia_faddr.x_node[3], g_RtTbl[iIndex].ia_faddr.x_node[4],g_RtTbl[iIndex].ia_faddr.x_node[5], 
					   etherHeader->ether_shost[0], etherHeader->ether_shost[1], etherHeader->ether_shost[2], etherHeader->ether_shost[3], etherHeader->ether_shost[4], etherHeader->ether_shost[5])
				   );
		if (bcmp((caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, (caddr_t)etherHeader->ether_shost,  LPX_NODE_LEN) == 0) {
            // found peer address already in the table -> just update ifp
            DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("Lpx_EA_ether_input: found address at g_RtTbl[%d]\n",iIndex));
            g_RtTbl[iIndex].rt_ifp = ifp;
            break;
		} else {
            if (lpx_nullhost(g_RtTbl[iIndex].ia_faddr)) {
				/* Searched to the end of entries, register to entry */
				bcopy((caddr_t)etherHeader->ether_shost, (caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, LPX_NODE_LEN);
				DEBUG_PRINT(DEBUG_MASK_IF_INFO,("Lpx_EA_ether_input: New address written to g_RtTbl[%d]\n",iIndex));
				g_RtTbl[iIndex].rt_ifp = ifp;
				break;
            }
		}
	}
	if (iIndex == MAX_LPX_ENTRY) {
		// Peer address table is full
		// Replace with oldest entry : TODO: This is not true
		if ((g_iRtLastIndex >= MAX_LPX_ENTRY) || (g_iRtLastIndex < 0)) {
            g_iRtLastIndex = 0;
		}
		iIndex = g_iRtLastIndex++;
		bcopy((caddr_t)etherHeader->ether_shost, (caddr_t)g_RtTbl[iIndex].ia_faddr.x_node, LPX_NODE_LEN);
		DEBUG_PRINT(DEBUG_MASK_IF_INFO,("Lpx_EA_ether_input: Table is full, written to g_RtTbl[%d], g_iRtLastIndex=%d\n",iIndex,g_iRtLastIndex));
		g_RtTbl[iIndex].rt_ifp = ifp;
	}

	if ((mbuf_len(packet) < sizeof(struct lpxhdr)) 
	   && (0 != mbuf_pullup(&packet, sizeof(struct lpxhdr)))) {
		// Free mbuf.
		mbuf_freem(packet);
		
		return EJUSTRETURN;		
	}
		
	lpxHeader = (struct lpxhdr *)mbuf_data(packet);
	packetSize = ntohs(lpxHeader->pu.pktsize & ~LPX_TYPE_MASK);
	
	//
	// Lpx Packets are alignmented by 4 byte. So Delete Stubs.
	//
	if (mbuf_len(packet) > packetSize) {
		DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_input: Total size %d, real size %d\n", mbuf_len(packet), packetSize));
		
		mbuf_adj(packet, - (mbuf_len(packet) - packetSize));
	}
	
	// Print Ethernet Header.
	DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_input: %02x:%2x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x [%04x]\n",
									 etherHeader->ether_shost[0], etherHeader->ether_shost[1], etherHeader->ether_shost[2],
									 etherHeader->ether_shost[3], etherHeader->ether_shost[4], etherHeader->ether_shost[5],
									 etherHeader->ether_dhost[0], etherHeader->ether_dhost[1], etherHeader->ether_dhost[2],
									 etherHeader->ether_dhost[3], etherHeader->ether_dhost[4], etherHeader->ether_dhost[5],
									 etherHeader->ether_type));
	
	// Print LPX Header.
	DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_input: Lpx Type:%d, Size:%d, DPort:0x%x, SPort:0x%x\n",
									 lpxHeader->pu.p.type, packetSize, 
									 ntohs(lpxHeader->dest_port), ntohs(lpxHeader->source_port)));
	
	DEBUG_PRINT(DEBUG_MASK_IF_INFO, ("lpx_ifnet_input: headerLen %d, buffer Len %d\n", mbuf_pkthdr_len(packet), mbuf_len(packet)));
	
	// Copy Ethernet Address of source and destination.
	error = mbuf_tag_allocate(
							  packet,
							  lpxTagId,
							  LPX_TAG_TYPE_ETHERNET_ADDR,
							  sizeof(struct ether_header),
							  MBUF_DONTWAIT,
							  (void **)&ethernetTag
							  );
	if ( 0 != error ) {
		DEBUG_PRINT(DEBUG_MASK_IF_WARING, ("lpx_ifnet_input: Can't make tag Error : %d\n", error));
		
		// Free mbuf.
		mbuf_freem(packet);
		
		return EJUSTRETURN;				
	}
	
	bcopy(header, ethernetTag, sizeof(struct ether_header));
		
	// Broadcasting?
	if (bcmp(etherHeader->ether_dhost, ether_broadcasst_address, 6) == 0) {
		
		// Copy Destination address to if address.
		ifnet_lladdr_copy_bytes(
								ifp,
								((struct ether_header *)ethernetTag)->ether_dhost,
								6	// Ethernet.
								);
	}

	// Pass to LPX.
	
	lpx_input(packet, packetSize);
	
	return 0;
}

errno_t lpx_ifnet_output(
						 ifnet_t interface,
						 mbuf_t	packet,
						 struct sockaddr *dest
						 )
{
	DEBUG_PRINT(DEBUG_MASK_IF_TRACE, ("lpx_ifnet_output: Entered.\n"));
		
	return ifnet_output(
						interface, 
						AF_LPX, 
						packet,
						NULL, 
						dest
						);
}