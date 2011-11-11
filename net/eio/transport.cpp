/***************************************************************
 * transport.cpp : implementation of TCP, UDP transport
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
#include "common.h"
#include "transport.h"
#include <sys/sendfile.h>
#include "serializer.h"

namespace yue {

DSCRPTR udp_socket(const char *addr,void *cfg) {
	SKCONF *skcfg = reinterpret_cast<SKCONF *>(cfg);
	object *pcfg = reinterpret_cast<object *>(skcfg->proto_p);
	UDPCONF conf;
	if (pcfg) {
		conf.ttl = (*pcfg)("ttl", 1);
		conf.mcast_addr = const_cast<char *>((*pcfg)("group", "239.192.1.2"));
		skcfg->proto_p = &conf;
	}
	DSCRPTR fd = nbr_osdep_udp_socket(addr, skcfg);
	return fd;
}

DSCRPTR mcast_socket(const char *addr,void *cfg) {
	SKCONF *skcfg = reinterpret_cast<SKCONF *>(cfg);
	object *pcfg = reinterpret_cast<object *>(skcfg->proto_p);
	UDPCONF conf;
	char _mcastg[16], _addr[16];
	if (addr) {
		const char *_port = util::str::divide(':', addr, _mcastg, sizeof(_mcastg));
		util::str::printf(_addr, sizeof(_addr), "0.0.0.0:%s", _port);
		addr = _addr;
	}
	if (pcfg) {
		conf.ttl = (*pcfg)("ttl", 1);
		conf.mcast_addr = const_cast<char *>((*pcfg)("group", _mcastg));
		skcfg->proto_p = &conf;
	}
	DSCRPTR fd = nbr_osdep_udp_socket(addr, skcfg);
	return fd;
}

int	mcast_connect(DSCRPTR, void*, socklen_t) {
	return NBR_OK;	/* nop */
}


}

extern "C" {

static	transport
g_tcp = {
	"tcp",
	NULL,
	false,
	NULL,
	NULL,
	NULL,
	nbr_osdep_tcp_str2addr,
	nbr_osdep_tcp_addr2str,
	(DSCRPTR (*)(const char *,void*))nbr_osdep_tcp_socket,
	nbr_osdep_tcp_connect,
	nbr_osdep_tcp_handshake,
	(DSCRPTR (*)(DSCRPTR, void *, socklen_t*, void*))nbr_osdep_tcp_accept,
#if defined(_DEBUG)
	nbr_osdep_tcp_close,
	(RECVFUNC)nbr_osdep_tcp_recv,
	(SENDFUNC)nbr_osdep_tcp_send,
#else
	nbr_osdep_tcp_close,
	(RECVFUNC)recv,
	(SENDFUNC)send,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
},
g_udp = {
	"udp",
	NULL,
	true,
	NULL,
	NULL,
	NULL,
	nbr_osdep_udp_str2addr,
	nbr_osdep_udp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::udp_socket,
	nbr_osdep_udp_connect,
	nbr_osdep_udp_handshake,
	NULL,
#if defined(_DEBUG)
	nbr_osdep_udp_close,
	(RECVFUNC)nbr_osdep_udp_recvfrom,
	(SENDFUNC)nbr_osdep_udp_sendto,
#else
	nbr_osdep_tcp_close,
	(RECVFUNC)recvfrom,
	(SENDFUNC)sendto,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
},
g_mcast = {
	"mcast",
	NULL,
	true,
	NULL,
	NULL,
	NULL,
	nbr_osdep_udp_str2addr,
	nbr_osdep_udp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::mcast_socket,
	(int (*)(DSCRPTR, void*, socklen_t))yue::mcast_connect,
	nbr_osdep_udp_handshake,
	NULL,
#if defined(_DEBUG)
	nbr_osdep_udp_close,
	(RECVFUNC)nbr_osdep_udp_recvfrom,
	(SENDFUNC)nbr_osdep_udp_sendto,
#else
	nbr_osdep_tcp_close,
	(RECVFUNC)recvfrom,
	(SENDFUNC)sendto,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
};

transport *tcp_transport() { return &g_tcp; }
transport *udp_transport() { return &g_udp; }
transport *mcast_transport() { return &g_mcast; }
}
