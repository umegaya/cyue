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

/* before yue::loop::init runs, yue::server::tlsv may be null. 
and test.cpp actually call session::connect before yue::loop::init 
(thus, before app::run called)
so we add null check only for test.cpp case */
void osdep_set_last_error(int e) {
	if (yue::server::tlsv()) {
		yue::server::tlsv()->set_osdep_last_error(e);
	}
}
int osdep_last_error() {
	return yue::server::tlsv() ? yue::server::tlsv()->osdep_last_error() : NBR_OK;
}

namespace yue {
server::accept_handler server::m_ah;
server::session_pool server::m_sp;	/* server connections */
const char *server::m_bootstrap;
server::config server::m_cfg = { 1000000, 100000, 2, 1.0f, 5000000 };/* default */;
server **server::m_sl = NULL, **server::m_slp = NULL;
int server::m_thn = -1;
int server::m_argc = 0;
char **server::m_argv = NULL;

void server::run(util::app &a) {
	while(a.alive()) { poll(); }
}
}

