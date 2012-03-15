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
		if (p().error_again()) { return again; }
		WP_TRACE("poller::wait: %d", p().error_no());
		return destroy;
	}
	for (int i = 0; i < n_ev; i++) {
		write(l, occur[i]);
	}
	/* all event fully read? */
	return n_ev < loop::maxfd() ? again : keep;
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
	ASSERT(h);
	switch((r = wbf->write(fd, loop::tl()[fd]))) {
	case keep: {
		TRACE("write: %d: process again\n", fd);
		loop::task t(e, loop::task::WRITE_AGAIN, h->serial());
		l.que().mpush(t);
	} break;
	case again: {
		WP_TRACE("write: %d: back to poller\n", fd);
		p().attach(fd, poller::EV_WRITE);
	} break;
	case nop: {
		WP_TRACE("write: %d: wait next retach\n", fd);
	} break;
	case destroy:
	default: {
		ASSERT(r == destroy);
		TRACE("write: %d: close %d\n", fd, r);
		DEBUG_SET_CLOSE(loop::hl()[fd]);
		loop::task t(fd, h->serial());
		l.que().mpush(t);
	} break;
	}
}
}
}

#endif
