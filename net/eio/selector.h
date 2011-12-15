/***************************************************************
 * selector.h : abstruction of event IO kernel API (epoll, kqueue...)
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
#if !defined(__SELECTOR_H__)
#define __SELECTOR_H__

#if defined(__ENABLE_EPOLL__)
#include <sys/epoll.h>
#include <sys/types.h>
#include "syscall.h"


#if !defined(EPOLLRDHUP)
#define EPOLLRDHUP 0x2000
#endif

namespace yue {
namespace module {
namespace net {
namespace eio {
namespace selector {
	class epoll {
		DSCRPTR m_fd;
	public:
		static const U32 EV_READ = EPOLLIN;
		static const U32 EV_WRITE = EPOLLOUT;
		typedef struct epoll_event event;
		epoll() : m_fd(INVALID_FD) {}
		int open(int max_nfd) {
			if ((m_fd = ::epoll_create(max_nfd)) < 0) {
				TRACE("epoll_create: %d,errno=%d",max_nfd,error_no());
				return NBR_ESYSCALL;
			}
			return NBR_OK;
		}
		DSCRPTR fd() { return m_fd; }
		void close() { ::close(m_fd); }
		inline int error_no() { return util::syscall::error_no(); }
		inline bool error_again() { return util::syscall::error_again(); }
		int attach(DSCRPTR d, U32 flag) {
			event e;
			e.events = (flag | EPOLLONESHOT | EPOLLRDHUP);
			e.data.fd = d;
			return ::epoll_ctl(fd(), EPOLL_CTL_ADD, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		int retach(DSCRPTR d, U32 flag) {
			event e;
			e.events = (flag | EPOLLONESHOT | EPOLLRDHUP);
			e.data.fd = d;
			return ::epoll_ctl(fd(), EPOLL_CTL_MOD, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		int detach(DSCRPTR d) {
			event e;
			return ::epoll_ctl(fd(), EPOLL_CTL_DEL, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		static DSCRPTR from(event &e) { return e.data.fd; }
		static bool readable(event &e) { return e.events & EV_READ; }
		static bool writable(event &e) { return e.events & EV_WRITE; }
		static bool closed(event &e) { return e.events & EPOLLRDHUP; }
		int wait(event *ev, int size, int timeout) {
			return ::epoll_wait(fd(), ev, size, timeout);
		}
	private:
		const epoll &operator = (const epoll &);
	};
	typedef epoll method;
}
}
}
}
}

#elif defined(__ENABLE_KQUEUE__)

#include <sys/event.h>
#include <sys/types.h>
#include "syscall.h"


#if !defined(EPOLLRDHUP)
#define EPOLLRDHUP 0x2000
#endif

namespace yue {
namespace module {
namespace net {
namespace eio {
namespace selector {
	class kqueue {
		DSCRPTR m_fd;
	public:
		static const U32 EV_READ = 0x1;
		static const U32 EV_WRITE = 0x2;
		typedef struct { U32 events; struct {DSCRPTR fd; } data; } event;
		kqueue() : m_fd(INVALID_FD) {}
		int open(int max_nfd) {
			return NBR_OK;
		}
		DSCRPTR fd() { return m_fd; }
		void close() { ::close(m_fd); }
		inline int error_no() { return util::syscall::error_no(); }
		inline bool error_again() { return util::syscall::error_again(); }
		int attach(DSCRPTR d, U32 flag) {
			ASSERT(false);
			return NBR_ENOTSUPPORT;
		}
		int retach(DSCRPTR d, U32 flag) {
			ASSERT(false);
			return NBR_ENOTSUPPORT;
		}
		int detach(DSCRPTR d) {
			ASSERT(false);
			return NBR_ENOTSUPPORT;
		}
		static DSCRPTR from(event &e) { return e.data.fd; }
		static bool readable(event &e) { return e.events & EV_READ; }
		static bool writable(event &e) { return e.events & EV_WRITE; }
		static bool closed(event &e) { return e.events & EPOLLRDHUP; }
		int wait(event *ev, int size, int timeout) {
			ASSERT(false);
			return NBR_ENOTSUPPORT;
		}
	private:
		const kqueue &operator = (const kqueue &);
	};
	typedef kqueue method;
}
}
}
}
}

#else
#error no suitable poller function
#endif


#endif
