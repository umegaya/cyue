/***************************************************************
 * transport.h : abstruction of transport layer API (socket API)
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of pfm framework.
 * pfm framework is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * pfm framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__TRANSPORT_H__)
#define __TRANSPORT_H__

#include <sys/socket.h>

#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
extern "C" {
#endif

typedef int DSCRPTR;
/* this config is given to proto modules */
typedef struct 	_sockconf
{
	int timeout;
	int rblen, wblen;
	void *proto_p;
}							SKCONF;
typedef struct 	_udpconf
{
	char *mcast_addr;
	int	ttl;
}							UDPCONF;
typedef int	(*RECVFUNC) (DSCRPTR, void*, size_t, ...);
typedef int	(*SENDFUNC)	(DSCRPTR, const void*, size_t, ...);

#define INVALID_FD (-1)
struct transport {
	const char	*name;
	void	*context;
	bool	dgram;
	int		(*init)		(void *);
	int		(*fin)		(void *);
	int		(*poll)		(void *);
	int		(*str2addr)	(const char*, void*, socklen_t*);
	int		(*addr2str)	(void*, socklen_t, char*, int);
	DSCRPTR	(*socket)	(const char*, void*);
	int		(*connect)	(DSCRPTR, void*, socklen_t);
	int		(*handshake)(DSCRPTR, int, int);
	DSCRPTR	(*accept)	(DSCRPTR, void*, socklen_t*, void*);
	int		(*close)	(DSCRPTR);
	int		(*read) 	(DSCRPTR, void*, size_t, ...);
	int		(*write)	(DSCRPTR, const void*, size_t, ...);
	ssize_t		(*writev)	(DSCRPTR, struct iovec *, size_t);
	ssize_t		(*sendfile)	(DSCRPTR, DSCRPTR, off_t *, size_t);
};

extern transport *tcp_transport();
extern transport *udp_transport();
extern transport *ws_transport();
extern transport *mcast_transport();
extern transport *popen_transport();

#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
}
#endif

#endif

