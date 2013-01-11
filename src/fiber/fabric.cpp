/***************************************************************
 * fabric.cpp : implementation of fabric.h
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
#include "fabric.h"
#include "serializer.h"
#include "server.h"

namespace yue {
util::map<fabric::yielded, MSGID> fabric::m_yielded_fibers;
int fabric::m_max_fiber = 0,
	fabric::m_fiber_pool_size = 0,
	fabric::m_max_object = 0,
	fabric::m_fiber_timeout_us = 0,
	fabric::m_timeout_check_intv = 0;
util::msgid_generator<U32> serializer::m_gen;
util::array<fiber::watcher> fiber::m_watcher_pool;


int fabric::static_init(config &cfg) {
	/* apply config */
	m_max_fiber = cfg.max_fiber;
	m_max_object = cfg.max_object;
	m_fiber_timeout_us = cfg.fiber_timeout_us;
	m_timeout_check_intv = cfg.timeout_check_intv_us;
	m_fiber_pool_size = (int)(server::thread_count() > 1 ?
		(m_max_fiber + (server::thread_count() - 1)) / server::thread_count() :
		m_max_fiber);
	if (!m_yielded_fibers.init(
		m_max_fiber, m_max_fiber, -1, util::opt_threadsafe | util::opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!fiber::watcher_pool().init(m_max_fiber, -1, util::opt_threadsafe | util::opt_expandable)) {
		return NBR_EMALLOC;
	}
	/* enable fiber timeout checker */
#if defined(__ENABLE_TIMER_FD__)
	int (*fn)(U64) = check_timeout;
#else
	int (*fn)(loop::timer_handle) = check_timeout;
#endif
	if (!server::create_timer(fn, 0.0f, m_timeout_check_intv / (1000 * 1000) /* to sec */, true /* open now */)) {
		return NBR_EEXPIRE;
	}
	return ll::static_init();
}
int fabric::init(const util::app &a, server *l) {
	int flags = util::opt_threadsafe | util::opt_expandable;
	m_server = l;
	if (!m_fiber_pool.init(m_fiber_pool_size, -1, flags)) {
		return NBR_EMALLOC;
	}
	return lang().init(a, m_server);
}
}
