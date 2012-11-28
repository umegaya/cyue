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

#include "transport.h"

#if defined(__ENABLE_EPOLL__)
#include <sys/epoll.h>
#include <sys/types.h>
#include "syscall.h"


#if !defined(EPOLLRDHUP)
#define EPOLLRDHUP 0x2000
#endif

namespace yue {
namespace net {
namespace selector {
	class epoll {
		DSCRPTR m_fd;
	public:
		static const U32 EV_READ = EPOLLIN;
		static const U32 EV_WRITE = EPOLLOUT;
		typedef struct epoll_event event;
		typedef int timeout;
		epoll() : m_fd(INVALID_FD) {}
		int open(int max_nfd) {
			if ((m_fd = ::epoll_create(max_nfd)) < 0) {
				TRACE("epoll_create: %d,errno=%d",max_nfd,error_no());
				return NBR_ESYSCALL;
			}
			return NBR_OK;
		}
		inline DSCRPTR fd() { return m_fd; }
		inline void close() { ::close(m_fd); }
		inline int error_no() { return util::syscall::error_no(); }
		inline bool error_again() { return util::syscall::error_again(); }
		inline int attach(DSCRPTR d, U32 flag) {
			event e;
			e.events = (flag | EPOLLONESHOT | EPOLLRDHUP);
			e.data.fd = d;
			return ::epoll_ctl(fd(), EPOLL_CTL_ADD, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		inline int retach(DSCRPTR d, U32 flag) {
			event e;
			e.events = (flag | EPOLLONESHOT | EPOLLRDHUP);
			e.data.fd = d;
			return ::epoll_ctl(fd(), EPOLL_CTL_MOD, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		inline int detach(DSCRPTR d) {
			event e;
			return ::epoll_ctl(fd(), EPOLL_CTL_DEL, d, &e) != 0 ? NBR_ESYSCALL : NBR_OK;
		}
		static inline void init_event(event &e, DSCRPTR fd = INVALID_FD) { e.events = 0; e.data.fd = fd; }
		static inline DSCRPTR from(event &e) { return e.data.fd; }
		static inline bool readable(event &e) { return e.events & EV_READ; }
		static inline bool writable(event &e) { return e.events & EV_WRITE; }
		static inline bool closed(event &e) { return e.events & EPOLLRDHUP; }
		static inline bool initialized(event &e) { return e.events == 0; }
		inline int wait(event *ev, int size, timeout &to) {
			return ::epoll_wait(fd(), ev, size, to);
		}
		static inline void init_timeout(int timeout_ns, timeout &to) { to = (timeout_ns / (1000 * 1000)); }
	private:
		const epoll &operator = (const epoll &);
	};
	typedef epoll method;
}
}
}

#elif defined(__ENABLE_KQUEUE__)

#include <sys/event.h>
#include <sys/types.h>
#include "syscall.h"

namespace yue {
namespace net {
namespace selector {
	class kqueue {
		DSCRPTR m_fd;
	public:
		static const U32 EV_READ = 0x01;
		static const U32 EV_WRITE = 0x02;
		typedef struct kevent event;
		typedef struct timespec timeout;
		kqueue() : m_fd(INVALID_FD) {}
		int open(int max_nfd) {
			m_fd = ::kqueue();
			return m_fd < 0 ? NBR_ESYSCALL : NBR_OK;
		}
		inline DSCRPTR fd() { return m_fd; }
		inline void close() { ::close(m_fd); }
		inline int error_no() { return util::syscall::error_no(); }
		inline bool error_again() { return util::syscall::error_again(); }
		inline int attach(DSCRPTR d, U32 flag) {
			return register_from_flag(d, flag, EV_ADD | EV_ONESHOT);
		}
		inline int retach(DSCRPTR d, U32 flag) {
			return register_from_flag(d, flag, EV_ADD | EV_ONESHOT);
		}
		inline int detach(DSCRPTR d) {
			return register_from_flag(d, EV_READ, EV_DELETE);
		}
		static inline void init_event(event &e, DSCRPTR fd = INVALID_FD) { e.filter = 0; e.ident = fd; }
		static inline DSCRPTR from(event &e) { return e.ident; }
		static inline bool readable(event &e) { return e.filter == EVFILT_READ; }
		static inline bool writable(event &e) { return e.filter == EVFILT_WRITE; }
		static inline bool initialized(event &e) { return e.filter == 0; }
		/* TODO: not sure about this check */
		static inline bool closed(event &e) { return e.flags & EV_ERROR;}
		inline int wait(event *ev, int size, timeout &to) {
			return ::kevent(m_fd, NULL, 0, ev, size, &to);
		}
		static inline void init_timeout(int timeout_ns, timeout &to) {
			to.tv_sec = (timeout_ns / (1000 * 1000 * 1000));
			to.tv_nsec = (timeout_ns % (1000 * 1000 * 1000));
		}
	private:
		const kqueue &operator = (const kqueue &);
		inline int register_from_flag(DSCRPTR d, U32 flag, U32 control_flag) {
			int r = NBR_OK, cnt = 0;
			event ev[2];
			if (flag & EV_WRITE) {
				EV_SET(&(ev[cnt++]), d, EVFILT_WRITE, control_flag, 0, 0, NULL);
			}
			if (flag & EV_READ) {
				EV_SET(&(ev[cnt++]), d, EVFILT_READ, control_flag, 0, 0, NULL);
			}
			if (::kevent(m_fd, ev, cnt, NULL, 0, NULL) != 0) {
				r = NBR_EKQUEUE;
			}
			return r;
		}
	};
	typedef kqueue method;
}
}
}

#else
#error no suitable poller function
#endif

namespace yue {
typedef net::selector::method poller;
}
#endif
