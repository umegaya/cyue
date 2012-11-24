/***************************************************************
 * popen.hpp : communicate with other process
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "osdep.h"
#include "util.h"

namespace yue {
namespace net {
static int
fd_setoption(DSCRPTR fd, SKCONF *cfg)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK, 1) < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"fcntl fail (nonblock) errno=%d", errno);
		return -5;
	}
	if (cfg->timeout >= 0) {
		struct timeval timeout = { cfg->timeout, 0 };
		if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndtimeo) errno=%d", errno);
		}
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvtimeo) errno=%d", errno);
		}
	}
	/*
	* 	you may change your system setting for large wb, rb. 
	*	eg)
	*	macosx: sysctl -w kern.ipc.maxsockbuf=8000000 & 
	*			sysctl -w net.inet.tcp.sendspace=4000000 sysctl -w net.inet.tcp.recvspace=4000000 
	*	linux:	/proc/sys/net/core/rmem_max       - maximum receive window
    *			/proc/sys/net/core/wmem_max       - maximum send window
    *			(but for linux, below page will not recommend manual tuning because default it set to 4MB)
	*	see http://www.psc.edu/index.php/networking/641-tcp-tune for detail
	*/
	if (cfg->wblen > 0) {
		TRACE("%d, set wblen to %u\n", fd, cfg->wblen);
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
			(char *)&cfg->wblen, sizeof(cfg->wblen)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndbuf) errno=%d", errno);
			return -3;
		}
	}
	if (cfg->rblen > 0) {
		TRACE("%d, set rblen to %u\n", fd, cfg->rblen);
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			(char *)&cfg->rblen, sizeof(cfg->rblen)) < 0) {
			OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvbuf) errno=%d", errno);
			return -4;
		}
	}
	return 0;
}
/* tcp related */
int
tcp_str2addr(const char *str, void *addr, socklen_t *len)
{
	char				addr_buf[256], url_buf[256];
	U16					port;
	struct hostent 		*hp;
	struct sockaddr_in	*sa = (struct sockaddr_in *)addr;

	if (*len < sizeof(*sa)) {
		OSDEP_ERROUT(INFO,INVAL,"addrlen %d not enough %u required",
			*len, (int)sizeof(*sa));
		return LASTERR;
	}
	*len = sizeof(*sa);
	if (util::str::parse_url(str, sizeof(addr_buf), addr_buf, &port, url_buf) < 0) {
		OSDEP_ERROUT(INFO,INVAL,"addr (%s) invalid", str);
		return LASTERR;
	}
	if ((hp = gethostbyname(addr_buf)) == NULL) {
		OSDEP_ERROUT(INFO,HOSTBYNAME,"addr (%s) invalid", addr_buf);
		return LASTERR;
 	}
	sa->sin_family = AF_INET;
	sa->sin_addr.s_addr = GET_32(hp->h_addr);
	sa->sin_port = htons(port);
//	TRACE("str2addr: %08x,%u,%u\n", sa->sin_addr.s_addr, sa->sin_port, *len);
//	ASSERT(sa->sin_addr.s_addr == 0 || sa->sin_addr.s_addr == 0x0100007f);
	return *len;
}

int
tcp_addr2str(void *addr, socklen_t len, char *str_addr, int str_len)
{
	struct sockaddr_in	*sa = (struct sockaddr_in *)addr;
	char a[16];	//xxx.xxx.xxx.xxx + \0
	if (len != sizeof(*sa)) {
		OSDEP_ERROUT(INFO,INVAL,"addrlen %d is not match %u required",
			len, (int)sizeof(*sa));
		return LASTERR;
	}
#if defined(_DEBUG)
	//util::syscall::inet_ntoa_r(sa->sin_addr, a, sizeof(a));
	//TRACE("addr2str: %08x(%s):%hu\n", sa->sin_addr.s_addr, a, sa->sin_port);
	//ASSERT(sa->sin_addr.s_addr != 0);
#endif
	return util::str::printf(str_addr, str_len, "%s:%hu", 
		util::syscall::inet_ntoa_r(sa->sin_addr, a, sizeof(a)),
		ntohs(sa->sin_port));
}

DSCRPTR
tcp_socket(const char *addr, SKCONF *cfg)
{
	struct sockaddr_in sa;
	int reuse; socklen_t alen;
	DSCRPTR fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		OSDEP_ERROUT(ERROR,INVAL,"TCP: create socket fail errno=%d", errno);
		goto error;
	}
	if (fd_setoption(fd, cfg) < 0) {
		goto error;
	}
	if (addr) {
		alen = sizeof(sa);
		if (tcp_str2addr(addr, &sa, &alen) < 0) {
			OSDEP_ERROUT(ERROR,INTERNAL,"TCP: str2addr fail errno=%d,addr=%s",
				errno, addr);
			goto error;
		}
		reuse = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"TCP: setsockopt(reuseaddr) fail errno=%d", errno);
			goto error;
		}
		if (bind(fd, (struct sockaddr *)&sa, alen) < 0) {
			OSDEP_ERROUT(ERROR,BIND,"TCP: bind fail errno=%d,len=%u,fd=%d",
				errno, alen, fd);
			goto error;
		}
		#define NBR_TCP_LISTEN_BACKLOG (512)
		/*  TODO: do following configuration autometically 
			if os somaxconn is less than NBR_TCP_LISTEN_BACKLOG, you should increase value by
			linux:	sudo /sbin/sysctl -w net.core.somaxconn=NBR_TCP_LISTEN_BACKLOG
					(and sudo /sbin/sysctl -w net.core.netdev_max_backlog=3000)
			osx:	sudo sysctl -w kern.ipc.somaxconn=NBR_TCP_LISTEN_BACKLOG
			(from http://docs.codehaus.org/display/JETTY/HighLoadServers)
		*/
		if (listen(fd, NBR_TCP_LISTEN_BACKLOG) < 0) {
			OSDEP_ERROUT(ERROR,LISTEN,"TCP: listen fail errno=%d", errno);
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

int
tcp_connect(DSCRPTR fd, void *addr, socklen_t len)
{
	ASSERT(addr);
	struct sockaddr_in *sa;
	if (len != sizeof(struct sockaddr_in)) {
		OSDEP_ERROUT(ERROR,INVAL,"addrlen %d is not match %u required",
			len, (int)sizeof(*sa));
		return LASTERR;
	}
	sa = (struct sockaddr_in *)addr;
	if (connect(fd, (const sockaddr *)addr, len) < 0) {
		if (errno != EINPROGRESS) {
			OSDEP_ERROUT(ERROR,CONNECT,"connect fail errno=%d", errno);
			return LASTERR;
		}
	}
	return NBR_OK;
}

int
tcp_handshake(DSCRPTR fd, int r, int w)
{
	TRACE("tcp_handshake: %d %d %d\n", fd, r, w);
	return w ? NBR_OK : NBR_ESEND;
}

DSCRPTR
tcp_accept(DSCRPTR fd, void *addr, socklen_t *len, SKCONF *cfg)
{
	ASSERT(addr);
	DSCRPTR cfd;
	if (*len < sizeof(struct sockaddr_in)) {
		OSDEP_ERROUT(ERROR,INVAL,"TCP: addrlen %d is not match %u required",
			*len, (int)sizeof(struct sockaddr_in));
		return INVALID_FD;
	}
	if ((cfd = accept(fd, (sockaddr *)addr, len)) == -1) {
		OSDEP_ERROUT(ERROR,ACCEPT,"TCP: accept fail errno=%d", errno);
		return INVALID_FD;
	}
	if (fd_setoption(fd, cfg) < 0) {
		close(cfd);
		return INVALID_FD;
	}
	return cfd;
}

int
tcp_close(DSCRPTR fd)
{
//	TRACE("fd(%d) closed\n", fd);
	struct timeval timeout = { 0, 1 };
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
		OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (sndtimeo) errno=%d", errno);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		OSDEP_ERROUT(ERROR,SOCKOPT,"setsockopt (rcvtimeo) errno=%d", errno);
	}
	close(fd);
	return NBR_OK;
}

int
tcp_recv(DSCRPTR fd, void *data, size_t len, int flag)
{
	int r = recv(fd, data, len, flag);
//	TRACE("recv(%d,%d)=%d\n", fd, len, r);
	return r;
}

int
tcp_send(DSCRPTR fd, const void *data, size_t len, int flag)
{
//	TRACE("send(%d,%d)\n", fd, len);
	return send(fd, data, len, flag);
}

int
tcp_addr_from_fd(DSCRPTR fd, char *addr, int alen)
{
#define MAX_INTERFACE (16)
#define IFNAMSIX		(256)
	struct ifconf	ifc;
	struct ifreq	iflist[MAX_INTERFACE];
	char ifname[IFNAMSIX]; int i, ret;
	socklen_t len;
	len = sizeof(ifname);
#if defined(SO_BINDTODEVICE)
	if (getsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, ifname, &len) < 0) {
		OSDEP_ERROUT(ERROR,SOCKOPT,"getsockopt (bindtodevice) errno=%d", errno);
		goto error;
	}
#else
	util::str::copy(ifname, DEFAULT_IF, IFNAMSIX);
#endif
	ifc.ifc_len = MAX_INTERFACE;
	ifc.ifc_req = iflist;
	if ((ret = ioctl(fd, SIOCGIFCONF, &ifc)) < 0) {
		OSDEP_ERROUT(INFO,IOCTL,"ioctl fail: ret=%d,errno=%d",ret,errno);
		goto error;
	}
	for (i = 0; i < ifc.ifc_len; i++) {
		if (util::str::cmp(ifc.ifc_req[i].ifr_name, ifname, IFNAMSIX) == 0) {
			return util::str::_copy(addr, alen, 
				(char *)&(ifc.ifc_req[i].ifr_addr), alen);
		}
	}
	return NBR_ENOTFOUND;
error:
	return LASTERR;
}
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
	yue::net::tcp_str2addr,
	yue::net::tcp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::net::tcp_socket,
	yue::net::tcp_connect,
	yue::net::tcp_handshake,
	(DSCRPTR (*)(DSCRPTR, void *, socklen_t*, void*))yue::net::tcp_accept,
#if defined(_DEBUG)
	yue::net::tcp_close,
	(RECVFUNC)yue::net::tcp_recv,
	(SENDFUNC)yue::net::tcp_send,
#else
	yue::net::tcp_close,
	(RECVFUNC)recv,
	(SENDFUNC)send,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
};

transport *tcp_transport() { return &g_tcp; }

}
