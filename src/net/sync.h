/***************************************************************
 * sync.h : event dispatcher for sync IO
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SYNC_H__)
#define __SYNC_H__

#include "selector.h"

namespace yue {
namespace net {

class sync_poller : public selector::method {
public:
	typedef selector::method poller;
	sync_poller() : poller() {}
	inline int wait_event(DSCRPTR wfd, U32 event, poller::timeout &to, poller::event *ev, int n_ev) {
		int r = NBR_OK;
		if (fd() == INVALID_FD) {
			if ((r = open(1)) < 0) { ASSERT(false); goto end; }
		}
		if ((r = attach(wfd, event)) < 0) {
			TRACE("errno = %d\n", error_no());ASSERT(false);
			goto end;
		}
	retry:
		if ((r = wait(ev, n_ev, to)) < 0) {
			if (error_again()) { TRACE("eagain\n"); goto retry; }
			TRACE("errno = %d\n", error_no());ASSERT(false);
			goto end;
		}
#if defined(_DEBUG)
		for (int i = 0; i < r; i++) {
			ASSERT(poller::from(ev[i]) == wfd);
		}
#endif
	end:
		detach(wfd);
		return r;
	}
};

}
}

#endif
