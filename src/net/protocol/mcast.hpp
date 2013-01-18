/***************************************************************
 * popen.hpp : communicate with other process
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "osdep.h"

namespace yue {
namespace net {
/* mcast transport API */
DSCRPTR mcast_socket(const char *addr,void *cfg) {
	SKCONF *skcfg = reinterpret_cast<SKCONF *>(cfg);
	object *pcfg = reinterpret_cast<object *>(skcfg->proto_p);
	UDPCONF conf;
	char _mcastg[16], _addr[16];
	if (addr) {
		const char *_port = util::str::divide_tag_and_val(
			':', addr, _mcastg, sizeof(_mcastg));
		util::str::printf(_addr, sizeof(_addr), "0.0.0.0:%s", _port);
		addr = _addr;
	}
	if (pcfg) {
		conf.ttl = (*pcfg)("ttl", 1);
		conf.ifname = (*pcfg)("ifname", (char *)NULL);
		conf.mcast_addr = const_cast<char *>((*pcfg)("group", _mcastg));
		skcfg->proto_p = &conf;
	}
	DSCRPTR fd = _udp_socket(addr, skcfg);
	return fd;
}

int	nop_connect(DSCRPTR, void*, socklen_t) {
	return NBR_OK;	/* nop */
}
}
}

extern "C" {

static	transport
g_mcast = {
	"mcast",
	NULL,
	true,
	NULL,
	NULL,
	NULL,
	yue::net::udp_str2addr,
	yue::net::udp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::net::mcast_socket,
	(int (*)(DSCRPTR, void*, socklen_t))yue::net::nop_connect,
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
	(ssize_t (*)(DSCRPTR, iovec*, size_t))yue::util::syscall::writev,
	sendfile,
};

transport *mcast_transport() { return &g_mcast; }

}
