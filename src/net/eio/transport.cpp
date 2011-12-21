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
#include "syscall.h"
#include "map.h"
#if defined(__ENABLE_SENDFILE__)
#include <sys/sendfile.h>
#else
#define sendfile NULL
#endif
#include "serializer.h"
#if defined(__NBR_OSX__)
#include <sys/uio.h>
#endif

namespace yue {

/* udp transport API */
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


/* mcast transport API */
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

int	nop_connect(DSCRPTR, void*, socklen_t) {
	return NBR_OK;	/* nop */
}

/* popen transport API */
struct popen_fd_data {
	DSCRPTR m_wfd;
	pid_t m_pid;
	U32 m_connected;
	popen_fd_data() : m_wfd(INVALID_FD), m_pid(0), m_connected(0) {}
	~popen_fd_data() { if (m_wfd >= 0) { ::close(m_wfd); m_wfd = INVALID_FD; } }
};
static util::map<popen_fd_data, DSCRPTR> g_popen_fdset;
int popen_init(void *ctx) {
	return g_popen_fdset.init(64, 64, -1, opt_threadsafe | opt_expandable) ? NBR_OK : NBR_EMALLOC;
}
int popen_fin(void *ctx) {
	g_popen_fdset.fin();
	return NBR_OK;
}
int	popen_write(DSCRPTR fd, const void *p, size_t l) {
	popen_fd_data *pfd = g_popen_fdset.find(fd);
	if (!pfd) { return NBR_ENOTFOUND; }
	return write(pfd->m_wfd, p, l);
}
ssize_t	popen_writev(DSCRPTR fd, struct iovec *iov, size_t l) {
	popen_fd_data *pfd = g_popen_fdset.find(fd);
	if (!pfd) { return NBR_ENOTFOUND; }
	return writev(pfd->m_wfd, iov, l);
}
int popen_handshake(DSCRPTR, int, int) {
	return NBR_SUCCESS;
}
#define POPEN_MAX_CMD_BUFF (4096)
#define POPEN_MAX_CMD_ARG (256)
DSCRPTR popen_socket(const char *addr,void *cfg) {
	int r;
	DSCRPTR pipes1[2];
	DSCRPTR pipes2[2];

	if ((r = util::syscall::pipe(pipes1)) != 0) {
		TRACE("pipe1 failed\n");
		return NBR_ESYSCALL;
	}
	if ((r = util::syscall::pipe(pipes2)) != 0) {
		TRACE("pipe2 failed\n");
		return NBR_ESYSCALL;
	}

	TRACE("[%d] pipe1 -> rfd:%d,wfd:%d\n", (int)getpid(), pipes1[0], pipes1[1]);
	TRACE("[%d] pipe2 -> rfd:%d,wfd:%d\n", (int)getpid(), pipes2[0], pipes2[1]);

	if ((r = fork()) == -1) {
		TRACE("[%d] fork() fails. %d\n", (int)getpid(), util::syscall::error_no());
		return NBR_ESYSCALL;
	}
	else if (r == 0) {
		TRACE("[%d] child process\n", (int)getpid());
		close(0);	//close original stdin
		close(1);	//close original stdout
		dup(pipes1[0]); // stdin
		dup(pipes2[1]); // stdout

		close(pipes1[0]);
		close(pipes1[1]);
		close(pipes2[0]);
		close(pipes2[1]);

		TRACE("[%d] wait for command from parent process\n", (int)getpid());
		char buff[POPEN_MAX_CMD_BUFF + 1], *pbuff, *args[POPEN_MAX_CMD_ARG + 1];
		if (!(pbuff = fgets(buff, sizeof(buff), stdin))) {
			TRACE("[%d] read from parent process fails\n", (int)getpid());
			exit(EXIT_FAILURE);
		}
		if (util::str::length(pbuff) >= POPEN_MAX_CMD_BUFF) {	//too long.
			TRACE("[%d] command too long\n", (int)getpid());
			exit(EXIT_FAILURE);
		}
		TRACE("[%d] exec command: %s\n", (int)getpid(), pbuff);
		if ((r = util::str::split(pbuff, " ", args, POPEN_MAX_CMD_ARG)) <= 0) {
			TRACE("[%d] parse command line fails: %d\n", (int)getpid(), r);
		}
		args[r] = NULL;
		TRACE("[%d] execute command: %s\n", (int)getpid(), args[0]);
		execv(args[0], &(args[1]));
		exit(EXIT_SUCCESS);
	}
	else { // Parent Process
		close(pipes1[0]);
		close(pipes2[1]);
		TRACE("[%d] for child process %d, rfd=%d, wfd=%d\n", (int)getpid(), r, pipes2[0], pipes1[1]);
		popen_fd_data *pfd = g_popen_fdset.alloc(pipes2[0]);
		if (!pfd) {
			close(pipes1[1]);
			close(pipes2[0]);
			ASSERT(false);
			return INVALID_FD;
		}
		pfd->m_wfd = pipes1[1];
		pfd->m_pid = r;
		return pipes2[0];	//return read fd
	}
}
int	popen_connect(DSCRPTR fd, void *a, socklen_t al) {
	ASSERT(al == 0);
	char *p = *(reinterpret_cast<char **>(a)), rf = '\n';
	TRACE("[%d] popen_connect: command = %s\n", (int)getpid(), p);
	popen_fd_data *pfd = g_popen_fdset.find(fd);
	if (!pfd) { return NBR_ENOTFOUND; }
	if (pfd->m_connected) { return NBR_EALREADY; }
	TRACE("[%d] send command to child %s\n", (int)getpid(), p);
	if (write(pfd->m_wfd, p, util::str::length(p)) < 0) { return NBR_ESEND; }
	if (write(pfd->m_wfd, &rf, 1) < 0) { return NBR_ESEND; }
	return NBR_OK;
}

int	popen_str2addr(const char *s, void *a, socklen_t *al) {
	TRACE("s2a: %s\n", s);
	char **pbuf = reinterpret_cast<char **>(a);
	*al = 0;
	return ((*pbuf = util::str::dup(s)) ? NBR_OK : NBR_EMALLOC);
}
int	popen_addr2str(void *a, socklen_t al, char *s, int sl) {
	if (al != 0) { ASSERT(false); return NBR_EINVAL; }
	return util::str::copy(s, *(reinterpret_cast<char **>(a)), sl);
}
int popen_close(DSCRPTR fd) {
	g_popen_fdset.erase(fd);
	close(fd);
	return NBR_OK;
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
	(int (*)(DSCRPTR, void*, socklen_t))yue::nop_connect,
	nbr_osdep_udp_handshake,
	NULL,
#if defined(_DEBUG)
	nbr_osdep_udp_close,
	(RECVFUNC)nbr_osdep_udp_recvfrom,
	(SENDFUNC)nbr_osdep_udp_sendto,
#else
	nbr_osdep_udp_close,
	(RECVFUNC)recvfrom,
	(SENDFUNC)sendto,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))writev,
	sendfile,
},
g_popen = {
	"popen",
	NULL,
	false,
	yue::popen_init,
	yue::popen_fin,
	NULL,
	yue::popen_str2addr,
	yue::popen_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::popen_socket,
	(int (*)(DSCRPTR, void*, socklen_t))yue::popen_connect,
	yue::popen_handshake,
	NULL,
#if defined(_DEBUG)
	yue::popen_close,
	(RECVFUNC)read,
	(SENDFUNC)yue::popen_write,
#else
	yue::popen_close,
	(RECVFUNC)read,
	(SENDFUNC)yue::popen_write,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))yue::popen_writev,
	NULL,
};

transport *tcp_transport() { return &g_tcp; }
transport *udp_transport() { return &g_udp; }
transport *mcast_transport() { return &g_mcast; }
transport *popen_transport() { return &g_popen; }
}
