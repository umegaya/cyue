/***************************************************************
 * server.cpp : main loop of worker thread
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "server.h"
#include "app.h"
#if !defined(NON_VIRTUAL)
#include "handler.hpp"
#endif

namespace yue {
ll server::m_config_ll;
config server::m_cfg = { 1000000, 100000, 1000000, 5000000 };/* default */;
util::array<handler::listener> server::m_stream_listener_pool;
util::map<server::thread, const char *> server::m_thread_pool;
util::array<handler::socket> server::m_socket_pool;
util::array<handler::timerfd> server::m_timer_pool;
util::array<server::task> server::m_task_pool;
server::poller_local_resource_pool server::m_resource_pool;
server::sig server::m_signal_pool[handler::signalfd::SIGMAX];

void *server::thread::operator () () {
	emittable::wrap w(this);
	launch_args args = { this };
	return util::app::start<server>(&args);
}
int server::thread::start() {
	return loop::app().tpool().addjob(this);
}
int server::thread::wait() {
	int cnt = (m_timeout_sec * 1000);
	//initialize timeout: 1sec.
	while (!m_server && cnt--) { util::time::sleep(1 * 1000 * 1000); }
	if (!m_server) {
		ASSERT(false);
		kill();
		return NBR_ETIMEOUT;
	}
	return NBR_OK;
}
void server::run(launch_args &args) {
	thread *th = args.m_thread;
	util::app &a = loop::app();
	ASSERT(a.alive());
	th->set_server(this);
	if (th->enable_event_loop()) {
		while(a.alive() && th->alive()) { poll(); }
	}
	else {
		fabric::task t;
		while(a.alive() && th->alive()) {
			while (m_fque.pop(t)) { TRACE("fabric::task processed3 %u\n", t.type()); t(*this); }
			util::time::sleep(1 * 1000 * 1000);
		}
	}
}
}

