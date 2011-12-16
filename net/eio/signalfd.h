/***************************************************************
 * signalfd.h : event IO signal handling
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
#if !defined(__SIGNALFD_H__)
#define __SIGNALFD_H__

#include "selector.h"
#include "syscall.h"
#include "functional.h"
#include <signal.h>

#define SIG_TRACE(...)

namespace yue {
namespace module {
namespace net {
namespace eio {

class signalfd {
public:
	typedef functional<void (int)> handler;
protected:
	static DSCRPTR m_pair[2];
	static const int SIGMAX = 32;
	static handler m_hmap[SIGMAX];
public:
	signalfd() { m_pair[0] = m_pair[1] = -1; }
	~signalfd() { fin(); }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	static void fin() {
		if (m_pair[0] >= 0) { ::close(m_pair[0]); }
		if (m_pair[1] >= 0) { ::close(m_pair[1]); }
	}
	template <class EM>
	int operator () (EM &em, selector::method::event &e) {
		return process(e);
	}
	static int hook(int sig, handler h) {
		if (sig < 0 || sig >= SIGMAX) { return NBR_EINVAL; }
		struct sigaction sa;
		sa.sa_handler = signal_handler;
		sa.sa_flags = SA_RESTART;
		if (sigemptyset(&sa.sa_mask) != 0) { return NBR_ESYSCALL; }
		if (::sigaction(sig, &sa, 0) != 0) { return NBR_ESYSCALL; }
		m_hmap[sig] = h;
		return NBR_OK;
	}
	static int ignore(int sig) {
		if (sig < 0 || sig >= SIGMAX) { return NBR_EINVAL; }
		signal(sig, SIG_IGN);
		return NBR_OK;
	}
	static int init() {
		if (util::syscall::pipe(m_pair) != 0) { return NBR_ESYSCALL; }
		SIG_TRACE("pipe: %d %d\n", m_pair[0], m_pair[1]);
		return NBR_OK;
	}
	static int fd() { return m_pair[0]; }
protected:
	static void  signal_handler( int sig ) {
		SIG_TRACE("sig[%d] raise\n", sig);
		syscall::write(m_pair[1], (char *)&sig, sizeof(sig));
	}
	static int process(selector::method::event &e) {
		DSCRPTR d = selector::method::from(e);int sig;
		ASSERT(d == fd());
		SIG_TRACE("sigfd:process %d(%d)\n", fd(), d);
		while (syscall::read(fd(), (char *)&sig, sizeof(sig)) > 0) {
			SIG_TRACE("sig[%d] notice\n", sig);
			if (sig < 0 || sig >= SIGMAX) { ASSERT(false); continue; }
			m_hmap[sig](sig);
		}
		return error_again() ? handler_result::again :
			handler_result::destroy;/* if EAGAIN, back to epoll fd set */
	}
};

}
}
}
}

#endif
