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
/* popen transport API */
struct popen_fd_data {
	DSCRPTR m_wfd;
	pid_t m_pid;
	U8 m_connected, m_recved, padd[2];
	popen_fd_data() : m_wfd(INVALID_FD), m_pid(0), m_connected(0), m_recved(0) {}
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
	TRACE("popen_write %d %u\n", pfd->m_wfd, (int)l);
	return ::write(pfd->m_wfd, p, l);
}
ssize_t	popen_writev(DSCRPTR fd, struct iovec *iov, size_t l) {
	popen_fd_data *pfd = g_popen_fdset.find(fd);
	if (!pfd) { return NBR_ENOTFOUND; }
	return ::writev(pfd->m_wfd, iov, l);
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
		if ((r = util::str::length(pbuff)) >= POPEN_MAX_CMD_BUFF) {	//too long.
			TRACE("[%d] command too long\n", (int)getpid());
			exit(EXIT_FAILURE);
		}
		pbuff[r - 1] = '\0';	//chop \n
		if ((r = util::str::split(pbuff, " ", args, POPEN_MAX_CMD_ARG)) <= 0) {
			TRACE("[%d] parse command line fails: %d\n", (int)getpid(), r);
		}
		TRACE("[%d] execute command: %s:%d\n", (int)getpid(), args[0], r);
		for (int i = 0; i < r; i++) {
			TRACE("[%d] args[%d]:%s\n", (int)getpid(), i, args[i]);
		}
		args[r] = NULL;
		fwrite("\n", 1, 1, stdout);	/* DIRTY HACK: for activating read fd of parent process */
		if (execv(args[0], args) == -1) {
			fprintf(stdout, "[%d] execv(%s) fails %d\n", (int)getpid(), args[0],
				util::syscall::error_no());
			exit(EXIT_FAILURE);
		}
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
	pfd->m_connected = 1;
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
int popen_read(DSCRPTR fd, char *p, size_t ln) {
	popen_fd_data *pfd = g_popen_fdset.find(fd);
	if (!pfd) { return NBR_ENOTFOUND; }
	TRACE("popen_read: from %d, limit %u\n", fd, (int)ln);
	if (!pfd->m_recved) {
		TRACE("read first rf\n");
		::read(fd, p, 1);//read and discard first \n
		ASSERT(*p == '\n');
		pfd->m_recved = 1;
	}
	return ::read(fd, p, ln);
}
}
}

extern "C" {
static transport g_popen = {
	"popen",
	NULL,
	false,
	yue::net::popen_init,
	yue::net::popen_fin,
	NULL,
	yue::net::popen_str2addr,
	yue::net::popen_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::net::popen_socket,
	(int (*)(DSCRPTR, void*, socklen_t))yue::net::popen_connect,
	yue::net::popen_handshake,
	NULL,
#if defined(_DEBUG)
	yue::net::popen_close,
	(RECVFUNC)yue::net::popen_read,
	(SENDFUNC)yue::net::popen_write,
#else
	yue::net::popen_close,
	(RECVFUNC)yue::net::popen_read,
	(SENDFUNC)yue::net::popen_write,
#endif
	(ssize_t (*)(DSCRPTR, iovec*, size_t))yue::net::popen_writev,
	NULL,
};

transport *popen_transport() { return &g_popen; }
}
