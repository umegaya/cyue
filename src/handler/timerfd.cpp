/***************************************************************
 * timerfd.cpp : event IO timer handling (can efficiently handle 10k timers)
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "timerfd.h"
#include "loop.h"
#include "server.h"
#if defined(USE_KQUEUE_TIMER)
#include <sys/event.h>
#include <sys/types.h>
#endif

namespace yue {
namespace handler {
#if defined(USE_KQUEUE_TIMER)
DSCRPTR timerfd::m_original_fd = INVALID_FD;

int timerfd::open_kqueue_timer(U64, U64 intval_us) {
	DSCRPTR fd = ::dup(m_original_fd); //dummy for reserving unique fd value.
	if (fd == INVALID_FD) { return NBR_ESYSCALL; }
	struct kevent ev;
	EV_SET(&ev, fd, EVFILT_TIMER, EV_ADD, 0, intval_us / 1000/* msec */, NULL);
	loop *l = loop::tls();
	poller &p = (l ? l->p() : loop::mainp());
	if (p.attach_by_event(ev) < 0) {
		::close(fd);
		return NBR_EKQUEUE;
	}
	m_fd = fd;
	return NBR_OK;
}
void timerfd::close_kqueue_timer() {
	struct kevent ev;
	EV_SET(&ev, m_fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	loop *l = loop::tls();
	poller &p = (l ? l->p() : loop::mainp());
	if (p.detach_by_event(ev) < 0) { ASSERT(false); }
	return;
}
#endif
}
}