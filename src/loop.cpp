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
#define DEFAULT_TIMEOUT_NS (1 * 1000 * 1000) //50ms
int loop::ms_maxfd = -1;
int loop::test = 0;
poller::timeout loop::ms_timeout;
util::app *loop::m_a = NULL;
util::pattern::shared_allocator<loop::rpoller, const char *> loop::m_pmap;
loop::basic_handler **loop::ms_h = NULL;
poller **loop::ms_pl = NULL;
loop::signalfd loop::m_signal;
loop::timerfd loop::m_timer;
loop::wpoller loop::m_wp;
loop::parking loop::m_parking;
loop::fs loop::m_fs;
loop::sync_poller loop::m_sync;
loop::rpoller *loop::m_mainp = NULL;
const char loop::NO_EVENT_LOOP[] = "none";
const char loop::USE_MAIN_EVENT_LOOP[] = "main";


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
	if ((r = timerfd::static_init()) < 0) { return r; }
	if (m_pmap.init(DEFAULT_POLLER_MAP_SIZE_HINT) < 0) { return NBR_EMALLOC; }
	if (!(m_mainp = m_pmap.alloc(USE_MAIN_EVENT_LOOP))) { return NBR_ESHORT; }
	if ((r = m_parking.init()) < 0) { return r; }
	/* default: use TCP */
	if ((r = m_parking.add("tcp", NULL/* default */)) < 0) { return r; }
	if ((r = m_parking.add("udp", udp_transport())) < 0) { return r; }
	if ((r = m_parking.add("mcast", mcast_transport())) < 0) { return r; }
	if ((r = m_parking.add("popen", popen_transport())) < 0) { return r; }
	if ((r = m_parking.add("ws", ws_transport())) < 0) { return r; }
	if (!(ms_h = new basic_handler*[ms_maxfd])) { return NBR_EMALLOC; }
	if (!(ms_pl = new poller*[ms_maxfd])) { return NBR_EMALLOC; }
    util::mem::bzero(ms_h, sizeof(basic_handler *) * ms_maxfd);
    util::mem::bzero(ms_pl, sizeof(poller *) * ms_maxfd);
	m_wp.configure(ms_maxfd);
	if ((r = loop::open(m_wp, m_mainp)) < 0) { return r; }
	if ((r = loop::open(m_signal, m_mainp)) < 0) { return r; }
	if ((r = m_timer.init_taskgrp()) < 0) { return r; }
#if defined(__ENABLE_TIMER_FD__) || defined(USE_KQUEUE_TIMER)
	if ((r = loop::open(m_timer, m_mainp)) < 0) { return r; }
#else
	if ((r = m_signal.hook(SIGALRM, m_timer)) < 0) {
		return r;
	}
	TRACE("initialize timer singal after sigaction finished\n");
	if ((r = m_timer.init(0.0f, ((double)timerfd::taskgrp::RESOLUTION_US) / (1000 * 1000))) < 0) {
		return r;
	}
#endif
	void (*tmp)(int) = process_signal;
	if ((r = m_signal.hook(SIGINT, tmp)) < 0) {
		return r;
	}
	if ((r = m_signal.ignore(SIGPIPE)) < 0) { return r; }
	if ((r = m_signal.ignore(SIGHUP)) < 0) { return r; }
	if ((r = m_fs.init(m_mainp)) < 0) { return r; }
	return NBR_OK;
}

bool loop::is_global_handler(basic_handler *h) {
	int t = h->type();
	return (t == basic_handler::SIGNAL ||
			t == basic_handler::WPOLLER ||
			t == basic_handler::FILESYSTEM);
}
void loop::close_attached_handlers(poller *p) {
	for (int i = 0; i < maxfd(); i++) {
		if (p && ms_pl[i] != p) { continue; }
		if (ms_h[i]) {
			if (p) {
				//because global handlers (static initialized handlers) are strongly related with
				//other handler (socket, listener, ...), removal these handler during other handler finalization is ongoing
				//causes many problems. so these only freed last step (close_attached_handlers(NULL)
				if (!is_global_handler(ms_h[i])) {
					loop::close(*ms_h[i]);
				}
			}
			else {
				loop::close(*ms_h[i]);
			}
		}
	}
}
void loop::fin_handlers() {
	if (m_mainp) {
		m_pmap.free(m_mainp);
		m_mainp = NULL;
	}
#if !defined(__ENABLE_TIMER_FD__) && !defined(USE_KQUEUE_TIMER)
	m_timer.on_close();
#endif
#if defined(__ENABLE_INOTIFY__)
	m_fs.fin();
#endif
	if (ms_h && ms_pl) {
		close_attached_handlers(NULL);
		delete []ms_h; ms_h = NULL;
		delete []ms_pl; ms_pl = NULL;
	}
}
void loop::static_fin() {
	timerfd::static_fin();
	m_pmap.fin();
	m_parking.fin();
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
		TRACE("tls does not initialized correctly %p %p\n", loop::tls(), this);
		return NBR_EPTHREAD;
	}
	ASSERT(a.m_pgname);
	if (0 != util::str::cmp(a.m_pgname, NO_EVENT_LOOP)) {
		m_p = m_pmap.alloc(a.m_pgname);
		if (0 == util::str::cmp(a.m_pgname, USE_MAIN_EVENT_LOOP)) {
			rpoller *rp = m_mainp;
			if (__sync_bool_compare_and_swap(&m_mainp, rp, NULL)) {
				if (rp) { m_pmap.free(rp); }
			}
		}
	}
	return m_que.init();
}
void loop::fin() {
	if (m_p) { m_pmap.free(m_p); }
	m_que.fin();
}
void loop::run(launch_args &args) {
	const util::app &a = *(args.m_a);
	ASSERT(a.alive());
	while(a.alive()) { poll(); }
}

}

#if defined(__NBR_IOS__)
void libyue_timer_callback() {
    yue::loop::timer().operator()(1);
}
#endif

