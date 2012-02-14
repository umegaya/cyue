/***************************************************************
 * signalfd.h : event IO signal handling
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SIGNALFD_H__)
#define __SIGNALFD_H__

#include "handler.h"
#include "selector.h"
#include "parking.h"
#include "syscall.h"
#include "functional.h"
#include <signal.h>

#define SIG_TRACE(...)

namespace yue {
class loop;
namespace handler {

class signalfd : public base {
public:
	typedef util::functional<void (int)> handler;
protected:
	static DSCRPTR m_pair[2];
	static const int SIGMAX = 32;
	static handler m_hmap[SIGMAX];
public:
	signalfd() : base(SIGNAL) { m_pair[0] = m_pair[1] = -1; }
	~signalfd() { fin(); }
	INTERFACE result on_read(loop &l, poller::event &e) {
		return process(e);
	}
	INTERFACE DSCRPTR on_open(U32 &, transport **) {
		int r;
		if (m_pair[0] < 0 && (r = init()) < 0) { return r; }
		return fd();
	}
	INTERFACE void on_close() { fin(); }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	static void fin() {
		if (m_pair[0] >= 0) { ::close(m_pair[0]); }
		if (m_pair[1] >= 0) { ::close(m_pair[1]); }
		m_pair[0] = m_pair[1] = -1;
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
		if (m_pair[1] > 0) {
			net::syscall::write(m_pair[1], (char *)&sig, sizeof(sig));
		}
	}
	static result process(poller::event &e) {
		int sig;
		ASSERT(poller::from(e) == fd());
		SIG_TRACE("sigfd:process %d(%d)\n", fd(), poller::from(e));
		while (net::syscall::read(fd(), (char *)&sig, sizeof(sig)) > 0) {
			SIG_TRACE("sig[%d] notice\n", sig);
			if (sig < 0 || sig >= SIGMAX) { ASSERT(false); continue; }
			m_hmap[sig](sig);
		}
		return error_again() ? again : destroy;
	}
};

}
}

#endif
