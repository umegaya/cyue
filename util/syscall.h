/***************************************************************
 * syscall.h : thin wrapper of system call functions.
 *  except event IO api (kqueue, epoll, ...) and sock APIs
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
#if !defined(__SYSCALL_H__)
#define __SYSCALL_H__

#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"

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
inline int fork(char *cmd, char *argv[], char *envp[]) {
	return nbr_osdep_fork(cmd, argv, envp);
}
inline int daemonize() {
	return nbr_osdep_daemonize();
}
inline int shutdown(DSCRPTR fd) {
	return ::shutdown(fd, 2);
}
inline int get_sock_addr(DSCRPTR fd, char *addr, socklen_t *alen) {
	return nbr_osdep_sockname(fd, addr, alen);
}

}
}
}

#endif
