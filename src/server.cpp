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
util::map<server::listener, const char *> server::m_listener_pool;
util::array<handler::listener> server::m_stream_listener_pool;
util::map<server::thread, const char *> server::m_thread_pool;
util::array<handler::socket> server::m_socket_pool;
util::map<handler::socket, const char *> server::m_cached_socket_pool;
util::array<server::timer> server::m_timer_pool;
util::map<server::peer, net::address> server::m_peer_pool;
server::sig server::m_signal_pool[handler::signalfd::SIGMAX];

void *server::thread::operator () () {
	emittable::wrap w(this);
	launch_args args = { this };
	return util::app::start<server>(&args);
}
volatile server *server::thread::start() {
	int r = loop::app().tpool().addjob(this), cnt = (m_timeout_sec * 1000);
	if (r < 0) { return NULL; }
	//initialize timeout: 1sec.
	while (!m_server && cnt--) { util::time::sleep(1 * 1000 * 1000); }
	if (!m_server) {
		ASSERT(false);
		kill();
		return NULL;
	}
	return m_server;
}
void server::run(launch_args &args) {
	thread *t = args.m_thread;
	util::app &a = loop::app();
	ASSERT(a.alive());
	t->set_server(this);
	while(a.alive() && t->alive()) { poll(); }
}
}

