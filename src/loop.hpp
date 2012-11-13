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
#define EIO_TRACE(...) //TRACE(__VA_ARGS__)

namespace yue {
inline void loop::read(poller::event &e) {
	read( *( hl() [ poller::from(e) ] ), e);
}
inline void loop::read(handler::base &h, poller::event &e) {
	int r; DSCRPTR fd = h.fd();
	EIO_TRACE("read: fd = %d\n", fd);
	switch((r = h.on_read(*this, e))) {
	case handler::base::keep: {
		EIO_TRACE("read: %d: process again\n", fd);
		task::io t(&h, e, task::io::READ_AGAIN);
		que().mpush(t);
	} break;
	case handler::base::read_again: {
		EIO_TRACE("read: %d,%d,%d: back to poller\n", fd, h->type(), util::syscall::error_no());
		r = p().retach(fd, poller::EV_READ);
		ASSERT(r >= 0);
	} break;
	case handler::base::write_again: {
		EIO_TRACE("write: %d: back to poller\n", fd);
		p().retach(fd, poller::EV_WRITE);
	} break;
	case handler::base::nop: {
		EIO_TRACE("read: %d: wait next retach\n", fd);
	} break;
	case handler::base::destroy:
	default: {
		ASSERT(r == handler::base::destroy);
		TRACE("read: %d: close %d\n", fd, r);
		task::io t(&h, task::io::CLOSE);
		que().mpush(t);
	} break;
	}
}
inline void loop::write(poller::event &e) {
	write( *( hl() [ poller::from(e) ] ) );
}
inline void loop::write(handler::base &h) {
	int r; DSCRPTR fd = h.fd();
	EIO_TRACE("write: fd = %d\n", fd);
	switch((r = h.on_write(p()))) {
	case handler::base::keep: {
		EIO_TRACE("write: %d: process again\n", fd);
		task::io t(&h, task::io::WRITE_AGAIN);
		que().mpush(t);
	} break;
	case handler::base::write_again: {
		EIO_TRACE("write: %d: back to poller\n", fd);
		r = p().retach(fd, poller::EV_WRITE);
		ASSERT(r >= 0);
	} break;
	case handler::base::read_again: {
		EIO_TRACE("write: %d: back to poller\n", fd);
		p().retach(fd, poller::EV_READ);
	} break;
	case handler::base::nop: {
		EIO_TRACE("write: %d: wait next retach\n", fd);
	} break;
	case handler::base::destroy:
	default: {
		ASSERT(r == handler::base::destroy);
		TRACE("write: %d: close %d\n", fd, r);
		task::io t(&h, task::io::CLOSE);
		que().mpush(t);
	} break;
	}
}
inline void loop::poll() {
	int n_ev;
	task::io t;
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
	int r, cnt; U32 flag = poller::EV_READ;
	DSCRPTR fd = h.on_open(flag);
	if (fd < 0) { 
		TRACE("invalid fd:%d\n", fd); ASSERT(false); 
		return NBR_EINVAL; 
	}
	if (util::syscall::fcntl_set_nonblock(fd) != 0) {
		TRACE("set non block fails %d\n", fd);
		if (util::syscall::error_no() != ENOTTY) {
			ASSERT(false);
			return NBR_ESYSCALL;
		}
	}
	REFER_EMPTR(&h);
	cnt = 0;
retry:
	if (__sync_bool_compare_and_swap(&(ms_h[fd]), NULL, &h)) {
		/* because attach only called thread success to set h to handler list,
		 * so once attach is success, its ready to close anytime. */
		if ((r = p().attach(fd, flag)) < 0) { goto error; }
	}
	else {
		/* I'm not sure that when fd value start to re-used. if actually connection that represents fd.
		 * if until this fd is closed (call syscall close), maybe such an problem never happen.
		 * but once connection close (and read(fd,...) returns 0), but before loop::close called for such an fd,
		 * if this fd is reused on such a timing, this may happen. */
		if (cnt <= 10) {
			/* in this case, experimentally we retry compare_and_swap.
			 * because sooner or later, previous handler for fd
			 * called loop::close(handler) and ms_h[fd] will be empty. */
			util::time::sleep(500 * 1000);	//sleep 500us
			cnt++;
			goto retry;
		}
		r = NBR_EALREADY;
		ASSERT(false);
		goto error;
	}
	return NBR_OK;
error:
	UNREF_EMPTR(&h);
	__sync_bool_compare_and_swap(&(ms_h[fd]), &h, NULL);
	ASSERT(false);
	return r;
}
inline int loop::close(basic_handler &h) {
	DSCRPTR fd = h.fd();
	p().detach(fd);
	/*  */
	if (__sync_bool_compare_and_swap(&(ms_h[fd]), &h, NULL)) {
		h.on_close();
		UNREF_EMPTR(&h);
	}
	else {
		ASSERT(false);
	}
	return NBR_OK;
}
inline int loop::handshake(poller::event &ev, transport *t) {
	DSCRPTR fd = poller::from(ev);
	TRACE("loop::handshake, %d %d %d\n", fd, 
		poller::readable(ev), poller::writable(ev));
	return net::syscall::handshake(fd,
		poller::readable(ev),
		poller::writable(ev), t);
}
}

#endif
