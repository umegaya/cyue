/***************************************************************
 * loop.hpp : template customized part of loop
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__LOOP_HPP__)
#define __LOOP_HPP__

#include "handler.h"
#define EIO_TRACE(...)

namespace yue {
inline void loop::read(poller::event &e) {
	int r; DSCRPTR fd = poller::from(e);
	EIO_TRACE("read: fd = %d\n", fd);
	switch((r = from(fd).on_read(*this, e))) {
	case handler::base::keep: {
		EIO_TRACE("read: %d: process again\n", fd);
		task t(e, task::io::READ_AGAIN);
		que().mpush(t);
	} break;
	case handler::base::again: {
		EIO_TRACE("read: %d: back to poller\n", fd);
		r = p().retach(fd, poller::EV_READ);
		ASSERT(r >= 0);
	} break;
	case handler::base::again_rw: {
		EIO_TRACE("read: %d: back to poller\n", fd);
		p().retach(fd, poller::EV_READ | poller::EV_WRITE);
	} break;
	case handler::base::nop: {
		EIO_TRACE("read: %d: wait next retach\n", fd);
	} break;
	case handler::base::destroy:
	default: {
		ASSERT(r == handler::base::destroy);
		TRACE("read: %d: close %d\n", fd, r);
		task t(fd);
		que().mpush(t);
	} break;
	}
}
inline void loop::write(poller::event &e) {
	int r; DSCRPTR fd = poller::from(e);
	EIO_TRACE("read: fd = %d\n", fd);
	switch((r = from(fd).on_write(p(), fd))) {
	case handler::base::keep: {
		EIO_TRACE("write: %d: process again\n", fd);
		task t(e, task::io::WRITE_AGAIN);
		que().mpush(t);
	} break;
	case handler::base::again: {
		EIO_TRACE("write: %d: back to poller\n", fd);
		r = p().retach(fd, poller::EV_WRITE);
		ASSERT(r >= 0);
	} break;
	case handler::base::again_rw: {
		EIO_TRACE("write: %d: back to poller\n", fd);
		p().retach(fd, poller::EV_READ | poller::EV_WRITE);
	} break;
	case handler::base::nop: {
		EIO_TRACE("write: %d: wait next retach\n", fd);
	} break;
	case handler::base::destroy:
	default: {
		ASSERT(r == handler::base::destroy);
		TRACE("read: %d: close %d\n", fd, r);
		task t(fd);
		que().mpush(t);
	} break;
	}
}
inline void loop::poll() {
	int n_ev;
	task t;
	/* TODO: add task throughput control */
	while (que().pop(t)) { t(*this); }
	poller::event occur[maxfd()];
	if ((n_ev = p().wait(occur, maxfd(), timeout())) < 0) {
		if (p().error_again()) { return; }
		TRACE("poller::wait: %d", p().error_no());
		return;
	}
	for (int i = 0; i < n_ev; i++) {
		read(occur[i]);
	}
}
inline DSCRPTR loop::open(basic_handler &h) {
	U32 flag = poller::EV_READ; transport *t = NULL;
	DSCRPTR fd = h.on_open(flag, &t);
	if (fd == INVALID_FD) { ASSERT(false); return NBR_EINVAL; }
	if (util::syscall::fcntl_set_nonblock(fd) != 0) {
		if (util::syscall::error_no() != ENOTTY) {
			ASSERT(false);
			return NBR_ESYSCALL;
		}
	}
	ms_transport[fd] = t;
	ms_h[fd] = &h;
	return p().attach(fd, flag);
}
inline int loop::close(DSCRPTR fd) {
	basic_handler *h = ms_h[fd];
	ms_h[fd] = NULL;
	ms_transport[fd] = NULL;
	p().detach(fd);
	if (h) { h->on_close(); }
	return NBR_OK;
}
inline int loop::handshake(poller::event &ev) {
	DSCRPTR fd = poller::from(ev);
	return net::syscall::handshake(fd,
		poller::readable(ev),
		poller::writable(ev), ms_transport[fd]);
}
}

#endif
