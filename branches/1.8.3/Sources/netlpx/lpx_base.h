#ifndef _LPX_BASE_H_
#define _LPX_BASE_H_

extern struct  lpxpcb lpx_datagram_pcb;
extern struct  lpxpcb lpx_stream_pcb;

void lpx_domain_init();
void lpx_init();
void lpx_input(mbuf_t packet, int len);
int	lpx_output( mbuf_t m0,
				int flags,
				struct lpxpcb *lpxp);

struct lpx_ifaddr * lpx_iaonnetof( register struct lpx_addr *dst );

int lpx_peeraddr( struct socket *so, struct sockaddr **nam );

int lpx_sockaddr( struct socket *so, struct sockaddr **nam );

#endif // _LPX_BASE_H_
