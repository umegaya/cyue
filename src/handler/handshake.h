/***************************************************************
 * handshake.h : check handshake timeout
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__HANDSHAKE_H__)
#define __HANDSHAKE_H__

#include "map.h"
#include "loop.h"

namespace yue {
namespace util {
class handshake {
public:
	typedef util::functional<int (DSCRPTR, bool)> handler;
	struct handshaker {
		handshaker() {}
		handshaker(DSCRPTR fd, handler &h, UTIME limit) :
			m_fd(fd), m_limit(limit) { m_h = h; }
		DSCRPTR m_fd;
		handler m_h;
		UTIME m_limit;
	};
protected:
	util::map<handshaker, DSCRPTR> m_hsm;
	UTIME m_now;
	static int timeout_iterator(handshaker *hs, handshake &hsm) {
		TRACE("check_timeout: %u, limit=%llu, now=%llu\n", hs->m_fd, hs->m_limit, hsm.now());
		if (hs->m_limit < hsm.now()) {
			TRACE("check_timeout: erased %u\n", hs->m_fd);
			if (hsm.get_map().erase(hs->m_fd)) {
				hs->m_h(hs->m_fd, false);
			}
			else {
				TRACE("timeout but establish happen during timeout processed.\n");
			}
		}
		return NBR_OK;
	}
	inline util::map<handshaker, DSCRPTR> &get_map() { return m_hsm; }
public:
	inline UTIME now() const { return m_now; }
	inline bool find_and_erase(DSCRPTR fd, handshaker &hs) { 
		return m_hsm.find_and_erase(fd, hs);
	}
#if defined(__ENABLE_TIMER_FD__) || defined(USE_KQUEUE_TIMER)
	int operator () (U64) {
#else
	int operator () (loop::timer_handle) {
#endif
		m_now = util::time::now();
		m_hsm.iterate(timeout_iterator, *this);
		return 0;
	}
	template <class H>
	inline int start_handshake(DSCRPTR fd, H &h, double timeout) {
		handler hd(h);
		return start_handshake(fd, hd, timeout);
	}
	inline int start_handshake(DSCRPTR fd, handler &ch, double timeout) {
		handshaker hs(fd, ch, util::time::now() + (UTIME)(timeout * 1000 * 1000));
		if (m_hsm.insert(hs, fd) < 0) { return NBR_EEXPIRE; }
		return NBR_OK;
	}
	int init(int maxfd);
	inline void fin() {
		m_hsm.fin();
	}
};
}
}
#endif
