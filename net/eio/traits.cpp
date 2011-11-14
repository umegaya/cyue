/***************************************************************
 * traits.cpp : implementation of specialization method of traits.h
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
#include "common.h"
#include "net.h"
#include "traits.h"
#include "dispatch.h"
#include "serializer.h"
#include "rpc.h"
#include "session.h"
#include "parking.h"

namespace yue {

typedef module::net::eio::stream_dispatcher<serializer, fabric>
	stream_dispatcher;
typedef module::net::eio::loop::processor<stream_dispatcher>
	stream_processor;

/* traits.h */
int loop_traits<loop>::m_worker_num = 0;
void *loop_traits<loop>::m_tls = NULL;
void *loop_traits<loop>::m_em_handle = NULL;
/* session.h */
loop *module::net::eio::session::m_server = NULL;
util::array<module::net::eio::session::watch_entry>
	module::net::eio::session::m_wl;
module::net::eio::session::watch_entry
	*module::net::eio::session::m_gtop = NULL;
thread::mutex module::net::eio::session::m_gmtx;



static inline loop::em<stream_processor> *_get_tls() {
	return loop_traits<loop>::tls_p() ?
		reinterpret_cast<loop::em<stream_processor> *>(
			loop_traits<loop>::tls_p()) :
		loop::em<stream_processor>::tlem();
}


bool module::net::eio::local_actor::feed(object &o) {
	module::net::eio::loop::em<yue::stream_processor> *pem =
		reinterpret_cast<module::net::eio::loop::em<
			yue::stream_processor> *>(m_em);
	yue::stream_dispatcher::task t(*pem, o);
	return pem->que().mpush(t);
}

bool module::net::eio::local_actor::delegate(fiber *f, object &o) {
	module::net::eio::loop::em<yue::stream_processor> *pem =
		reinterpret_cast<module::net::eio::loop::em<
			yue::stream_processor> *>(m_em);
	yue::stream_dispatcher::task t(f, o);
	return pem->que().mpush(t);
}

bool module::net::eio::local_actor::delegate(fiber_handler &fh, object &o) {
	module::net::eio::loop::em<yue::stream_processor> *pem =
		reinterpret_cast<module::net::eio::loop::em<
			yue::stream_processor> *>(m_em);
	yue::stream_dispatcher::task t(fh, o);
	return pem->que().mpush(t);
}

void module::net::eio::remote_actor::close() {
	if (super::valid()) {
		yue::stream_dispatcher::task t(super::fd);
		_get_tls()->que().mpush(t);
	}
}

/* session.h */
void module::net::eio::session::close() {
	if (valid()) {
		TRACE("close: called %d\n", m_fd);
		m_failure = MAX_CONN_RETRY;
		close(m_fd);
	}
}
void module::net::eio::session::shutdown() {
	if (valid()) {
		yue::stream_processor::close(m_fd);
	}
}
void module::net::eio::session::close(DSCRPTR fd) {
	yue::stream_dispatcher::task t(fd);
	_get_tls()->que().mpush(t);
}
int module::net::eio::session::connect(
	address &to, transport * t, connect_handler &ch, double timeout, object *opt) {
	SKCONF skc = { 120, 65536, 65536, NULL };
	if (opt) {
		skc.rblen = (*opt)("rblen",65536);
		skc.wblen = (*opt)("wblen",65536);
		skc.timeout = (*opt)("timeout",120);
		skc.proto_p = opt;
	}
	DSCRPTR fd = module::net::eio::syscall::socket(NULL, &skc, t);
	stream_processor::handler h/* .ch = ch (but not access) */;
	if (fd < 0) { goto end; }
	if (module::net::eio::syscall::connect(fd, to.addr_p(), to.len(), t) < 0) {
		goto end;
	}
	if (yue::stream_dispatcher::start_handshake(fd, ch, timeout) < 0) {
		goto end;
	}
	if (yue::stream_processor::attach(fd,
			t && t->dgram ?
				loop::basic_processor::fd_type::DGCONN :
				loop::basic_processor::fd_type::CONNECTION,
			h, t) < 0) {
		goto end;
	}
	ASSERT(((int)fd) >= 0 && (sizeof(DSCRPTR) == sizeof(int)));
	TRACE("session : connect success\n");
	return ((int)fd);
end:
	if (fd >= 0) { module::net::eio::syscall::close(fd, t); }
	return NBR_ESYSCALL;
}

int module::net::eio::session::sync_read(local_actor &la, object &o, int timeout) {
	return yue::stream_dispatcher::sync_read(m_fd,
		*reinterpret_cast<loop::em<stream_processor> *>(la.m_em), o, timeout);
}

int module::net::eio::session::sync_write(local_actor &la, int timeout) {
	return yue::stream_dispatcher::sync_write(m_fd,
		*reinterpret_cast<loop::em<stream_processor> *>(la.m_em), timeout);
}

int module::net::eio::session::sync_connect(local_actor &la, int timeout) {
	loop::poller::event ev;
	UTIME now = util::time::now();
	loop::em<yue::stream_processor> *em = 
		reinterpret_cast<loop::em<yue::stream_processor> *>(la.m_em);
	/* init wbuf for write */
	if (wbuf::init() < 0) { return NBR_EMALLOC; }
	/* open connection (m_fd finally initialized by this value) */
	DSCRPTR fd = module::net::eio::session::connect();
	if (fd < 0) { return NBR_ESYSCALL; }
	/* wait established */
	while (loop::synchronizer().wait_event(fd,
		loop::poller::EV_WRITE | loop::poller::EV_READ, timeout, ev) >= 0) {
		TRACE("poller readable %d %s %s\n", fd,
				loop::poller::readable(ev) ? "r" : "nr",
				loop::poller::writable(ev) ? "w" : "nw");
		yue::stream_dispatcher::read(fd, *em, ev);
		TRACE("after read: %d %s\n", fd, valid() ? "valid" : "invalid");
		if (valid()) {
			ASSERT(fd == m_fd);
			return NBR_OK;
		}
		if ((now + timeout) < util::time::now()) {
			return NBR_ETIMEOUT;
		}
	}
	ASSERT(false);
	return NBR_ESYSCALL;
}



//object &object_from(fiber &f) { return f.obj(); }

int loop_traits<loop>::maxfd(loop &l) {
	return l.maxfd();
}
int loop_traits<loop>::init(loop &l) {
	int r;
	if ((r = l.init_with<stream_processor>()) < 0) { return r; }
	return session::init(l.maxfd(), &l);
}
void loop_traits<loop>::fin(loop &l) {
	module::net::eio::loop::em<yue::stream_processor>::fin();
	session::fin();
	l.~loop();
	m_tls = NULL;
	m_worker_num = 0;
}
void loop_traits<loop>::die(loop &l) {
	l.die();
}
int loop_traits<loop>::run(loop &l, int num) {
	m_worker_num = num;
	return l.run_with<stream_processor>(num);
}

int loop_traits<loop>::start(loop &l) {
	m_worker_num = 1;
	return (m_em_handle = l.start_with<stream_processor>()) ? NBR_OK : NBR_EINTERNAL;
}
void loop_traits<loop>::stop(loop &l) {
	l.stop_with<stream_processor>(m_em_handle);
}
void loop_traits<loop>::poll(loop &l) {
	l.poll_with<stream_processor>(m_em_handle);
}

int loop_traits<loop>::listen(loop &l, const char *addr, accept_handler &ah, object *opt) {
	char a[256];
	transport *t = l.divide_addr_and_transport(addr, a, sizeof(a));
	if (!module::net::eio::parking::valid(t)) { return NBR_ENOTFOUND; }
	SKCONF skc = { 120, 65536, 65536, opt };
	DSCRPTR fd = module::net::eio::syscall::socket(a, &skc, t);
	if (fd < 0) { return NBR_ESYSCALL; }
	stream_processor::handler h; h.ah = ah;
	if (stream_processor::attach(fd,
		t && t->dgram ? fd_type::DGLISTENER : fd_type::LISTENER, h, t) < 0) {
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

local_actor *loop_traits<loop>::get_thread(loop &l, int idx) {
	return l.from(0, idx);
}

int loop_traits<loop>::signal(loop &l, int signo, functional<void (int)> &sh) {
	return stream_processor::sig().hook(signo, sh);
}

timer loop_traits<loop>::set_timer(loop &l, double start, double intval,
	functional<int (timer)> &sh) {
	return stream_processor::timer().add_timer(sh, start, intval);
}

void loop_traits<loop>::stop_timer(loop &l, timer t) {
	stream_processor::timer().remove_timer_reserve(t);
}

void loop_traits<loop>::set_tls(void *tls) {
	if (m_worker_num == 1) {/* single thread mode */
		m_tls = tls;
		return;
	}
	loop::em<stream_processor>::tlem()->set_tls_p(tls);
}

void *loop_traits<loop>::tls() {
	return m_tls ? m_tls : loop::em<stream_processor>::tlem()->tls_p();
}

}
