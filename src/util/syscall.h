/***************************************************************
 * syscall.h : thin wrapper of system call functions.
 *  except event IO api (kqueue, epoll, ...) and sock APIs
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 *  see license.txt for license detail
 ****************************************************************/
#if !defined(__SYSCALL_H__)
#define __SYSCALL_H__

#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"
#include "transport.h"

namespace yue {
namespace util {
namespace syscall {
typedef struct rlimit rlimit;
inline int getrlimit(int resource, rlimit *rlim) {
	return ::getrlimit(resource, rlim);
}
inline int setrlimit(int resource, const rlimit *rlim) {
	return ::setrlimit(resource, rlim);
}
inline int error_no() {
	return errno;
}
inline bool error_again() {
	return error_no() == EINTR ||
			error_no() == EAGAIN ||
			error_no() == EWOULDBLOCK;
}
inline bool error_conn_reset() {
	return error_no() == ECONNRESET || error_no() == EBADF;
}
inline bool error_pipe() {
	return error_no() == EPIPE;
}
inline int pipe(DSCRPTR pair[2]) {
	return ::pipe(reinterpret_cast<int *>(pair));
}
inline int fcntl_set_nonblock(DSCRPTR fd) {
	int flag;
	return (((flag = ::fcntl(fd, F_GETFL)) >= 0) &&
		::fcntl(fd, F_SETFL, flag | O_NONBLOCK) >= 0) ? 0 : -1;
}
inline int shutdown(DSCRPTR fd) {
	return ::shutdown(fd, 2);
}
extern int getpid();
extern int forkexec(char *cmd, char *argv[], char *envp[]);
extern int daemonize();
extern int get_sock_addr(DSCRPTR fd, char *addr, socklen_t *alen);
extern int get_if_addr(DSCRPTR fd, const char *ifn, char *addr, int alen);
extern int get_macaddr(char *ifname, U8 *addr);
}
}
}

#endif
