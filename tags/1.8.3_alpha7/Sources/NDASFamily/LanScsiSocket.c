/***************************************************************************
 *
 *  LanScsiSocket.c
 *
 *    LanScsi Socket commands
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

//#include <sys/param.h>
#include <sys/systm.h>
//#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
68 
69 #include <sys/kauth.h>
70 #include <sys/mount_internal.h>
71 #include <sys/kernel.h>
72 #include <sys/kpi_mbuf.h>
73 #include <sys/malloc.h>
74 #include <sys/vnode.h>
75 #include <sys/domain.h>
76 #include <sys/protosw.h>
77 #include <sys/socket.h>
78 #include <sys/syslog.h>
79 #include <sys/tprintf.h>
80 #include <sys/uio_internal.h>
81 #include <libkern/OSAtomic.h>
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <machine/spl.h>

#include <IOKit/IOLib.h>

#include "LanScsiSocket.h"
#include "DebugPrint.h"

//extern unsigned long strtoul(const char *, char **, int);
//extern int inet_aton(char * cp, struct in_addr * pin);

errno_t	xi_sock_socket(int domain, int type, int protocol, sock_upcall callback, void *cookie, xi_socket_t *new_so)
{
#ifdef __KPI_SOCKET__
	return sock_socket(domain, type, protocol, callback, cookie, new_so);
#else
	thread_funnel_set(network_flock, TRUE);
	errno_t	error;
	
	error = socreate(domain, new_so, type, protocol); 
	
	(void)thread_funnel_set(network_flock, FALSE);
	
	return error;
#endif	
}

void xi_sock_close(xi_socket_t so) {

	if (so != NULL) {
#ifdef __KPI_SOCKET__
		sock_close(so);
#else
		thread_funnel_set(network_flock, TRUE);

		soclose(so);
	
		(void)thread_funnel_set(network_flock, FALSE);
#endif
	} 
	
	so = NULL;
}

errno_t xi_sock_bind(xi_socket_t so, struct sockaddr *to)
{
	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	return sock_bind(so, to);
#else
	thread_funnel_set(network_flock, TRUE);
	errno_t	error;

	error = sobind(so, to);
	
	(void)thread_funnel_set(network_flock, FALSE);

	return error;
#endif
}

errno_t xi_sock_connect(xi_socket_t so, struct sockaddr *to, int flags)
{
	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	return sock_connect(so, to, flags);
#else
	thread_funnel_set(network_flock, TRUE);
	errno_t	error;

	error = soconnect(so, to);
	
	(void)thread_funnel_set(network_flock, FALSE);
	
	do {
		//		struct sockaddr_lpx sin;
		int s;
		
		thread_funnel_set(network_flock, TRUE);
		
		s = splnet();
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			//        IOLog("before sleep\n");
			(void) tsleep((caddr_t)&so->so_timeo, PSOCK | PCATCH, "xiscsicon", 0);
			//        IOLog("after sleep\n");
			break;
		}
		//    IOLog("so->so_error = %d\n", so->so_error);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			
			goto bad;
		}
		splx(s);
		
		(void) thread_funnel_set(network_flock, FALSE);
		
	} while(0);

	return error;

bad:
	(void) thread_funnel_set(network_flock, FALSE);	
    	
	return error;
#endif
}

errno_t xi_sock_receivefrom(xi_socket_t so, void *buf, size_t *len, int flags, struct sockaddr *from, int *fromlen) {

	struct iovec aio;
	errno_t			error;

	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	
	struct msghdr msg;
	size_t			recvLen;
	
	aio.iov_base = buf;
	aio.iov_len = *len;
	bzero(&msg, sizeof(msg));
	msg.msg_iov = (struct iovec *) &aio;
	msg.msg_iovlen = 1;
	
	if(from != NULL && fromlen != NULL && *fromlen > 0) {
		msg.msg_name = from;
		msg.msg_namelen = *fromlen;
	}
	
	error = sock_receive(so, &msg, flags, &recvLen);
	
	*len = recvLen;
	
	return error;

#else
		
    struct uio auio;
	struct sockaddr *fromsa = 0;
	
	aio.iov_base = (char *)buf;
	aio.iov_len = *len;
	
	auio.uio_iov = &aio;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = 0;                    /* XXX */
	auio.uio_resid = *len;
	
	thread_funnel_set(network_flock, TRUE);
				
	error = soreceive(so, &fromsa, &auio, NULL, 0, &flags);
	
	(void)thread_funnel_set(network_flock, FALSE);

	if (from != NULL && fromsa) {
		bcopy(fromsa, from, sizeof(struct sockaddr_lpx));
		
		FREE(fromsa, M_SONAME);
	}

	*len =  *len - auio.uio_resid;

	return error;
#endif
	
}

inline errno_t xi_sock_receive(xi_socket_t so, void *buf, size_t *len, int flags)
{
	return xi_sock_receivefrom(so, buf, len, flags, NULL, 0);
}

inline errno_t xi_sock_send(xi_socket_t so, void *buf, size_t *len, int flags) {
	
	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	struct iovec aio;
	struct msghdr msg;
	
	size_t sentLen = *len;
	errno_t	error;
	
	aio.iov_base = buf;
	aio.iov_len = sentLen;
	bzero(&msg, sizeof(msg));
	msg.msg_iov = (struct iovec *) &aio;
	msg.msg_iovlen = 1;
	
	error = sock_send(so, &msg, flags, &sentLen);
	
#if 1
    if(error)
		DebugPrint(1, false, "xi_sock_send: so = %p, buf_len = %d error = %d\n", so, (int)*len, error);
#endif /* if 0 */     
	
	*len = sentLen;
	
	return error;
#else
	struct iovec aiov;
    struct uio auio = { 0 };
    register struct proc *p = current_proc();
    register int error = 0;
    
    aiov.iov_base = buf;
    aiov.iov_len = *len;
    
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_WRITE;
    auio.uio_procp = p;
    auio.uio_offset = 0;                    /* XXX */
    auio.uio_resid = *len;

//    IOLog("Before: so = %p, buf_len = %d, aiov.iov_len = %d, error = %d, auio.uio_resid = %d\n", so, (int)len, (int)aiov.iov_len, error,auio.uio_resid);
    thread_funnel_set(network_flock, TRUE);

    error = sosend(so, NULL, &auio, NULL, 0, flags);
    
	(void) thread_funnel_set(network_flock, FALSE);

#if 0
    if(error)
        IOLog("After: so = %p, buf_len = %d, aiov.iov_len = %d, error = %d, auio.uio_resid = %d\n", so, (int)*len, (int)aiov.iov_len, error, auio.uio_resid);
#endif /* if 0 */     
   
	*len = *len - auio.uio_resid;
	
	return error;
	
#endif
}

errno_t xi_sock_shutdown(xi_socket_t so, int how)
{
	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	return sock_shutdown(so, how);
#else
	thread_funnel_set(network_flock, TRUE);
	errno_t	error;

	error = soshutdown(so, how);
	
	(void)thread_funnel_set(network_flock, FALSE);

	return error;
#endif
}

errno_t xi_sock_setsocketopt(xi_socket_t	so, 
							 int			sopt_level,
							 int			sopt_name,
							 void			*val,
							 size_t			valSize)
{

	errno_t	error;
	
#ifdef __KPI_SOCKET__
	error = sock_setsockopt(
							so,
							sopt_level,
							sopt_name,
							val,
							valSize
							); 
#else 
	
	struct sockopt sopt;

	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = sopt_level;
	sopt.sopt_name = sopt_name;
	sopt.sopt_val = val;
	sopt.sopt_valsize = valSize;
	sopt.sopt_p = current_proc();
	
	thread_funnel_set(network_flock, TRUE);
	
	error = sosetopt(so, &sopt);
	
	(void) thread_funnel_set(network_flock, FALSE);
#endif
	
	return error;
}

/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

int xi_lpx_connect(xi_socket_t *oso, struct sockaddr_lpx daddr, struct sockaddr_lpx saddr, int timeoutsec)
{
    xi_socket_t so = NULL;
	errno_t	error;
	struct timeval timeout;
	
	*oso = NULL;
	
	error = xi_sock_socket(AF_LPX, SOCK_STREAM, 0, NULL, NULL, &so);
	if(error) {
		DebugPrint(1, false, "socreate error %d\n", error);
        goto bad;
	}

	error = xi_sock_bind(so, (struct sockaddr *) &saddr);
	
	if(error) {
		DebugPrint(1, false, "xi_lpx_connect: sobind error\n");
		goto bad;
	}

#if 0
    DebugPrint(4, false, "xi_lpx_connect to ");
    for(i=0; i<6; i++)
        DebugPrint(4, false, "02x ", daddr.slpx_node[i]);
#endif
	
    error = xi_sock_connect(so, (struct sockaddr *)&daddr, 0);
    if(error) {
        DebugPrint(4, false, "soconnect error %d\n", error);
        goto bad;
    }
			
	*oso = so;

	// Set Read Timeout.
	timeout.tv_sec = timeoutsec;
	timeout.tv_usec = 0;

	error = xi_sock_setsocketopt(so, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

	if(error) {
        DebugPrint(1, false, "xi_lpx_connect: Can't set Receive Time out. error %d\n", error);
		
		goto bad;	
	}
		
	return 0;
	
bad:
	
    xi_lpx_disconnect(so);
	
	*oso = so = NULL;
    
	return error;
}

void xi_lpx_disconnect(xi_socket_t so)
{
	if (so != NULL) {
		if(so) {
			xi_sock_shutdown (so, SHUT_RDWR);
			xi_sock_close(so);
		}
	} else {
		DebugPrint(1, false, "xi_lpx_disconnect: So is null\n");
	}	
}

 inline ssize_t xi_lpx_send(xi_socket_t so, void *msg, size_t len, int flags)
 {
	 register int error = 0;
	 size_t		sendLen = len;

	 if (so == NULL) {
		 return EBADF;
	 }
	 
	 if (!xi_lpx_isconnected(so)) {
		 DebugPrint(1, true, "xi_lpx_send: socket was disconnected. so = %p, len = %d\n", so, len);
		 
		 return -ENOTCONN;
	 }
	 
	 error = xi_sock_send(so, msg, &sendLen, flags);	 
	 	 
#if 1
	 if(error)
		 DebugPrint(1, true, "xi_lpx_send: After: so = %p, error = %d, len = %d\n", so, error, sendLen);
#endif    

	if(error) {
		return (error > 0) ? -error : error;
	} else { 
		return sendLen;
	}
 }

inline ssize_t xi_lpx_recv(xi_socket_t so, void *msg, size_t len, int flags)
{
	register int error = 0;
	size_t recvLen = len;
	
	if (so == NULL) {
		return EBADF;
	}
	
	if (!xi_lpx_isconnected(so)) {
		DebugPrint(1, true, "xi_lpx_recv: socket was disconnected. so = %p, len = %d\n", so, len);
		
		return -ENOTCONN;
	}
	
	error = xi_sock_receive(so, msg, &recvLen, flags);	 
			
#if 1
	if(error) {
		DebugPrint(1, true, "xi_lpx_recv: After: so = %p, error = %d, len = %d\n", so, error, recvLen);
	}
#endif    
	
	if(error) {
		return (error > 0) ? -error : error;
	} else { 
		return recvLen;
	}
}

inline int xi_lpx_isconnected(xi_socket_t so)
{
//	DebugPrint(1, true, "xi_lpx_isconnected: so = %p\n", so);

	if (so == NULL) {
		return EBADF;
	}
	
#ifdef __KPI_SOCKET__
	return sock_isconnected(so);
#else
	int retval;
	int s;
	
	thread_funnel_set(network_flock, TRUE);
	
	s = splnet();
	
	retval = (so->so_state & SS_ISCONNECTED) != 0;
	
	splx(s);
	
	(void) thread_funnel_set(network_flock, FALSE);
	
	return retval;
#endif
}
