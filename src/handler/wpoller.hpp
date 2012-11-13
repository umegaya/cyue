/***************************************************************
 * wpoller.hpp : write poller implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__WPOLLER_HPP__)
#define __WPOLLER_HPP__

#include "wpoller.h"

namespace yue {
namespace handler {
inline base::result write_poller::on_read(loop &l, poller::event &e) {
	poller::event occur[loop::maxfd()]; int n_ev;
	if ((n_ev = p().wait(occur, loop::maxfd(), loop::timeout())) < 0) {
		if (p().error_again()) { return read_again; }
		WP_TRACE("poller::wait: %d", p().error_no());
		return destroy;
	}
	for (int i = 0; i < n_ev; i++) {
		write(l, occur[i]);
	}
	/* all event fully read? */
	return n_ev < loop::maxfd() ? read_again : keep;
}
inline void write_poller::write(loop &l, poller::event &e) {
	int r; DSCRPTR fd = poller::from(e);
	WP_TRACE("write: fd = %d\n", fd);
	wbuf *wbf = get_wbuf(fd);
	if (!wbf) {
		WP_TRACE("write: wbuf detached %d\n", fd);
		return;
	}
	handler::base *h = loop::hl()[fd];
	if (!h) {
		WP_TRACE("write: handler already closed %d\n", fd);
		return;
	}
	switch((r = wbf->write(fd, h->t()))) {
	case keep: {
		TRACE("write: %d: process again\n", fd);
		task::io t(h, e, task::io::WRITE_AGAIN);
		l.que().mpush(t);
	} break;
	case write_again: {
		WP_TRACE("write: %d: back to poller\n", fd);
		p().attach(fd, poller::EV_WRITE);
	} break;
	case read_again: {
		WP_TRACE("read: %d: back to poller\n", fd);
		p().attach(fd, poller::EV_READ);
	} break;
	case nop: {
		WP_TRACE("write: %d: wait next retach\n", fd);
	} break;
	case destroy:
	default: {
		ASSERT(r == destroy);
		TRACE("write: %d: close %d\n", fd, r);
		DEBUG_SET_CLOSE(loop::hl()[fd]);
		task::io t(h, task::io::CLOSE);
		l.que().mpush(t);
	} break;
	}
}
}
}

#endif
