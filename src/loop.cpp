/***************************************************************
 * loop.cpp : abstract main loop of worker thread
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "loop.h"
#include "app.h"
#include "server.h"
#include "fs.h"

namespace yue {
#define MINIMUM_MAXFD (1024)
#define DEFAULT_TIMEOUT_NS (50 * 1000 * 1000) //50ms
int loop::ms_maxfd = -1;
int loop::test = 0;
poller::timeout loop::ms_timeout;
util::app *loop::m_a = NULL;
poller loop::m_p;
loop::basic_handler **loop::ms_h = NULL;
loop::signalfd loop::m_signal;
loop::timerfd loop::m_timer;
loop::wpoller loop::m_wp;
loop::parking loop::m_parking;
loop::fs loop::m_fs;
loop::sync_poller loop::m_sync;


int loop::static_init(util::app &a) {
	int r; util::syscall::rlimit rl;
	m_a = &a;
	//ASSERT(test == 0);
	test = 1;
	if(util::syscall::getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		ASSERT(false);
		return NBR_ESYSCALL;
	}
	if (rl.rlim_cur < MINIMUM_MAXFD) {
		TRACE("rlimit maxfd %llu => %d\n", (U64)rl.rlim_cur, MINIMUM_MAXFD);
		rl.rlim_cur = rl.rlim_max = MINIMUM_MAXFD;
		if (util::syscall::setrlimit(RLIMIT_NOFILE, &rl) < 0) {
			ASSERT(false);
			return NBR_ESYSCALL;
		}
	}
	ms_maxfd = rl.rlim_cur;
	poller::init_timeout(DEFAULT_TIMEOUT_NS, ms_timeout);
	if ((r = m_parking.init()) < 0) { return r; }
	/* default: use TCP */
	if ((r = m_parking.add("tcp", NULL/* default */)) < 0) { return r; }
	if ((r = m_parking.add("udp", udp_transport())) < 0) { return r; }
	if ((r = m_parking.add("mcast", mcast_transport())) < 0) { return r; }
	if ((r = m_parking.add("popen", popen_transport())) < 0) { return r; }
	if ((r = m_parking.add("ws", ws_transport())) < 0) { return r; }
	if (m_p.open(ms_maxfd) < 0) { return NBR_ESYSCALL; }
	if (!(ms_h = new basic_handler*[ms_maxfd])) { return NBR_EMALLOC; }
	m_wp.configure(ms_maxfd);
	if ((r = loop::open(m_wp)) < 0) { return r; }
	if ((r = loop::open(m_signal)) < 0) { return r; }
#if defined(__ENABLE_TIMER_FD__)
	if ((r = loop::open(m_timer)) < 0) { return r; }
#else
	if ((r = m_signal.hook(SIGALRM, signalfd::handler(m_timer))) < 0) {
		return r;
	}
	if ((r = m_timer.init()) < 0) { return r; }
#endif
	if ((r = m_signal.hook(SIGINT, process_signal)) < 0) {
		return r;
	}
	if ((r = m_signal.ignore(SIGPIPE)) < 0) { return r; }
	if ((r = m_signal.ignore(SIGHUP)) < 0) { return r; }
	if ((r = m_fs.init()) < 0) { return r; }
	return NBR_OK;
}
void loop::fin_handlers() {
	loop::close(m_wp);
	loop::close(m_signal);
#if defined(__ENABLE_TIMER_FD__)
	loop::close(m_timer);
#else
	m_timer.on_close();
#endif
	m_fs.fin();
	if (ms_h) {
		for (int i = 0; i < maxfd(); i++) {
			if (ms_h[i]) { UNREF_EMPTR(ms_h[i]); }
		}
		delete []ms_h; ms_h = NULL;
	}
}
void loop::static_fin() {
	m_parking.fin();
	m_p.close();
}
void loop::process_signal(int sig) {
	switch (sig) {
	case SIGINT: m_a->die(); break;
	default: ASSERT(false); break;
	}
}
int loop::init(launch_args &a) {
	if (osdep_init() < 0) { return NBR_EPTHREAD; }
	util::thread::init_tls(this);
	if (loop::tls() != this) {
		TRACE("tls does not init correctly %p %p\n", loop::tls(), this);
		return NBR_EPTHREAD;
	}
	return m_que.init();
}
void loop::fin() {
	m_que.fin();
}
void loop::run(launch_args &a) {
	ASSERT(a.alive());
	while(a.alive()) { poll(); }
}

}
