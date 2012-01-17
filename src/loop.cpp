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

namespace yue {
int loop::ms_maxfd = -1, loop::ms_timeout = 50;
util::app *loop::m_a = NULL;
poller loop::m_p;
loop::basic_handler **loop::ms_h = NULL;
transport **loop::ms_transport;
loop::signalfd loop::m_signal;
loop::timerfd loop::m_timer;
loop::wpoller loop::m_wp;
loop::parking loop::m_parking;
loop::sync_poller loop::m_sync;
loop *loop::m_l = NULL;


int loop::static_init(util::app &a, int thn, int argc, char *argv[]) {
	int r; util::syscall::rlimit rl;
	m_a = &a;
	if(util::syscall::getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		return NBR_ESYSCALL;
	}
	ms_maxfd = rl.rlim_cur;
	if ((r = m_parking.init()) < 0) { return r; }
	/* default: use TCP */
	if ((r = m_parking.add("tcp", NULL/* default */)) < 0) { return r; }
	if ((r = m_parking.add("udp", udp_transport())) < 0) { return r; }
	if ((r = m_parking.add("mcast", mcast_transport())) < 0) { return r; }
	if ((r = m_parking.add("popen", popen_transport())) < 0) { return r; }
	if (m_p.open(ms_maxfd) < 0) { return NBR_ESYSCALL; }
	if (!(ms_h = new basic_handler*[ms_maxfd])) { return NBR_EMALLOC; }
	if (!(ms_transport = new transport*[ms_maxfd])) { return NBR_EMALLOC; }
	m_wp.configure(ms_maxfd);
	if ((r = loop::open(m_wp)) < 0) { return r; }
	if ((r = loop::open(m_signal)) < 0) { return r; }
#if defined(__ENABLE_TIMER_FD__)
	if ((r = loop::open(m_timer)) < 0) { return r; }
#else
	if ((r = m_timer.init()) < 0) { return r; }
	if ((r = m_signal.hook(SIGALRM, signalfd::handler(m_timer))) < 0) {
		return r;
	}
#endif
	if ((r = m_signal.hook(SIGINT, signalfd::handler(process_signal))) < 0) {
		return r;
	}
	if ((r = m_signal.ignore(SIGPIPE))) { return r; }
	if ((r = m_signal.ignore(SIGHUP))) { return r; }
	return thn;
}
void loop::static_fin() {
	loop::close(m_wp.fd());
	loop::close(m_signal.fd());
#if defined(__ENABLE_TIMER_FD__)
	loop::close(m_timer.fd());
#else
	m_timer.on_close();
#endif
	m_parking.fin();
	if (ms_h) { delete []ms_h; ms_h = NULL; }
	if (ms_transport) { delete []ms_transport; ms_transport = NULL; }
	m_p.close();
}
void loop::process_signal(int sig) {
	switch (sig) {
	case SIGINT: m_a->die(); break;
	default: ASSERT(false); break;
	}
}
int loop::init(util::app &a) {
	if (a.thn() <= 1) { m_l = this; }
	else { thread::init_tls(this); }
	return m_que.init();
}
void loop::fin() {
	m_que.fin();
}
void loop::run(util::app &a) {
	while(a.alive()) { poll(); }
}

}
