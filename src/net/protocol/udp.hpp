/***************************************************************
 * udp.hpp: udp transport
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "osdep.h"
#include "util.h"

namespace yue {
namespace net {
#define udp_str2addr	tcp_str2addr
#define udp_addr2str	tcp_addr2str
#define	udp_connect	tcp_connect
#define udp_handshake	tcp_handshake
#define	udp_close		tcp_close
#define udp_recv		tcp_recv
#define udp_send		tcp_send
/* UDP related */
static DSCRPTR
_udp_socket(const char *addr, SKCONF *cfg)
{
	struct sockaddr_in sa;
	struct ip_mreq mreq;
	socklen_t alen; int reuse;
	UDPCONF *ucf = (UDPCONF *)cfg->proto_p;
	DSCRPTR fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"UDP: create socket fail errno=%d", errno);
		goto error;
	}
	if (fd_setoption(fd, cfg) < 0) {
		goto error;
	}
	if (addr) {
		alen = sizeof(sa);
		if (udp_str2addr(addr, &sa, &alen) < 0) {
			OSDEP_ERROUT(ERROR,INTERNAL,"UDP: str2addr fail errno=%d,addr=%s",
				errno, addr);
			goto error;
		}
		reuse = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"TCP: setsockopt(reuseaddr) fail errno=%d",
				errno);
			goto error;
		}
		if (bind(fd, (struct sockaddr *)&sa, alen) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"UDP: bind fail errno=%d", errno);
			goto error;
		}
	}
	if (ucf && ucf->mcast_addr) {
		struct ifreq ifr;
		util::mem::bzero(&ifr, sizeof(ifr));
		ifr.ifr_addr.sa_family = AF_INET;
		TRACE("ifn = %s\n", ucf->ifname ? ucf->ifname : DEFAULT_IF);
		util::str::_copy(ifr.ifr_name, IFNAMSIZ-1, ucf->ifname ? ucf->ifname : DEFAULT_IF, IFNAMSIZ-1);
retry:
		if (-1 == ioctl(fd, SIOCGIFADDR, &ifr)) {
			OSDEP_ERROUT(ERROR,SYSCALL,"get local addr fail %u\n", errno);
#if defined(__NBR_OSX__)
			//in case of disconnected from lan (but need to run test), fall back to lo0.
			if (errno == EADDRNOTAVAIL) {
				#define FALLBACK_IF ("lo0")	
				util::str::_copy(ifr.ifr_name, IFNAMSIZ-1, FALLBACK_IF, IFNAMSIZ-1);
				goto retry;
			}
#endif
			goto error;
		}
#if defined(_DEBUG)
			char tmpstr[256];
			struct sockaddr tmp;
			socklen_t sl = sizeof(tmp);
			util::syscall::get_sock_addr(fd, (char *)&tmp, &sl);
			udp_addr2str((char *)&(ifr.ifr_addr), sizeof(ifr.ifr_addr),
					tmpstr, sizeof(tmpstr));
			TRACE("localaddr/addr = %s/%s\n", tmpstr, addr);
#endif
		//mreq.imr_ifindex = 0;
#if !defined(__NBR_OSX__)
		if (setsockopt(fd,
			IPPROTO_IP, IP_MULTICAST_IF,
			(char *)&(ifr.ifr_addr), sizeof(ifr.ifr_addr)) == -1) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setmcastif : %d\n", errno);
			goto error;
		}
#endif
		if (addr) { /* if bind is done */
			struct in_addr in;
			struct sockaddr_in *sa = (struct sockaddr_in *)&(ifr.ifr_addr);
			if (0 == inet_aton(ucf->mcast_addr, &in)) {
				OSDEP_ERROUT(ERROR,SOCKET,"get mcast addr: (%s)\n", ucf->mcast_addr);
				goto error;
			}
			util::mem::bzero(&mreq, sizeof(mreq));
			mreq.imr_multiaddr.s_addr = in.s_addr;
			mreq.imr_interface.s_addr = sa->sin_addr.s_addr;
			if (setsockopt(fd,
				IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&(mreq), sizeof(mreq)) == -1) {
				OSDEP_ERROUT(ERROR,SOCKOPT,"add member ship : %d\n", errno);
				goto error;
			}
		}
		if (setsockopt(fd,
			IPPROTO_IP, IP_MULTICAST_TTL, (char *)&(ucf->ttl), sizeof(ucf->ttl)) == -1) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"set multicast ttl : %d %d\n", errno, ucf->ttl);
			goto error;
		 }
	}
	return fd;

error:
	if (fd >= 0) {
		close(fd);
	}
	return INVALID_FD;
}

DSCRPTR udp_socket(const char *addr,void *cfg) {
	SKCONF *skcfg = reinterpret_cast<SKCONF *>(cfg);
	object *pcfg = reinterpret_cast<object *>(skcfg->proto_p);
	UDPCONF conf;
	if (pcfg) {
		conf.ttl = (*pcfg)("ttl", 1);
		conf.ifname = (*pcfg)("ifname", (char *)NULL);
		conf.mcast_addr = const_cast<char *>((*pcfg)("group", "239.192.1.2"));
		skcfg->proto_p = &conf;
	}
	DSCRPTR fd = _udp_socket(addr, skcfg);
	return fd;
}

int
udp_recvfrom(DSCRPTR fd, void *data, size_t len, int flag, void *addr, socklen_t *alen)
{
//	TRACE("recv(%d,%d)\n", fd, len);
	return recvfrom(fd, data, len, 0, (sockaddr *)addr, alen);
}
int
udp_sendto(DSCRPTR fd, const void *data, size_t len, int flag, const void *addr, size_t alen)
{
//	TRACE("send(%d,%d)\n", fd, len);
	return sendto(fd, data, len, 0, (const sockaddr *)addr, alen);
}
}
}

extern "C" {

static	transport
g_udp = {
	"udp",
	NULL,
	true,
	NULL,
	NULL,
	NULL,
	yue::net::udp_str2addr,
	yue::net::udp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::net::udp_socket,
	yue::net::udp_connect,
	yue::net::udp_handshake,
	NULL,
#if defined(_DEBUG)
	yue::net::udp_close,
	(RECVFUNC)yue::net::udp_recvfrom,
	(SENDFUNC)yue::net::udp_sendto,
#else
	yue::net::udp_close,
	(RECVFUNC)recvfrom,
	(SENDFUNC)sendto,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
};

transport *udp_transport() { return &g_udp; }

}
