/***************************************************************************
 *
 *  LanScsiSocket.h
 *
 *    LanScsi Socket command definitions and prototype
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef	_LANSCSISOCKET_H_
#define _LANSCSISOCKET_H_

#include <sys/types.h> // OS X 10.3 need this
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/malloc.h>

#include "lpx.h"

#define NORMAL_DATACONNECTION_TIMEOUT	30	// 30 seconds. Sometimes wake-up time is too long.
#define PACKET_DATACONNECTION_TIMEOUT	60	// 60 secnods.

#ifdef __KPI_SOCKET__
typedef socket_t xi_socket_t;
#else
typedef struct socket*	xi_socket_t;

#ifndef errno_t
typedef int	errno_t;
#endif

#endif

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/***************************************************************************
 *
 *  Function prototypes
 *
 **************************************************************************/
//extern int xi_connect(struct socket **so, struct sockaddr_in daddr, u_short dport);
//extern int xi_connect(struct socket **so, char *, u_short dport);
//extern void xi_disconnect(struct socket *so);

#ifndef __KPI_SOCKET__
typedef void*	sock_upcall;
#endif
	
errno_t	xi_sock_socket(int domain, int type, int protocol, sock_upcall callback, void *cookie, xi_socket_t *new_so);
void xi_sock_close(xi_socket_t so);

errno_t xi_sock_bind(xi_socket_t so, struct sockaddr *to);

errno_t xi_sock_receivefrom(xi_socket_t so, void *buf, size_t *len, int flags, struct sockaddr *from, int *fromlen);
errno_t xi_sock_receive(xi_socket_t so, void *buf, size_t *len, int flags);

errno_t xi_sock_setsocketopt(xi_socket_t	so, 
								 int			sopt_level,
								 int			sopt_name,
								 void			*val,
							 size_t			valSize);
	
int xi_lpx_connect(xi_socket_t *oso, struct sockaddr_lpx daddr, struct sockaddr_lpx saddr, int timeoutsec);
void xi_lpx_disconnect(xi_socket_t so);

ssize_t xi_lpx_send(xi_socket_t so, void *msg, size_t len, int flags);
ssize_t xi_lpx_recv(xi_socket_t so, void *buf, size_t len, int flags);

inline int xi_lpx_isconnected(xi_socket_t so);

#ifdef __cplusplus
}
#endif // __cplusplus 
	
#endif // _LANSCSISOCKET_H_
