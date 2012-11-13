/***************************************************************
 * wpoller.h : poller for write descriptro
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__WPOLLER_H__)
#define __WPOLLER_H__

#include "selector.h"
#include "handler.h"
#include "sbuf.h"
#include "wbuf.h"

#define WP_TRACE(...)

namespace yue {
namespace handler {
class write_poller : public base {
	FORBID_COPY(write_poller);
	typedef net::wbuf wbuf;
	struct wfd {
		wbuf *m_wp;
		wfd() : m_wp(NULL) {}
	} *m_wb;
	poller m_p;
	int m_maxfd;
	poller &p() { return m_p; }
public:
	write_poller() : base(WPOLLER), m_wb(NULL), m_p(), m_maxfd(-1) {}
	write_poller(int maxfd) : base(WPOLLER), m_wb(NULL), m_p(), m_maxfd(maxfd) {}
	~write_poller() { fin(); }
	inline DSCRPTR fd() { return p().fd(); }
	void configure(int maxfd) { m_maxfd = maxfd; }
	int init(int maxfd) {
		m_maxfd = maxfd;
		if (!(m_wb = new wfd[maxfd])) { return NBR_EMALLOC; }
		return p().open(maxfd);
	}
	void fin() {
		p().close();
		if (m_wb) {
			delete []m_wb;
			m_wb = NULL;
		}
	}
	INTERFACE DSCRPTR on_open(U32 &) {
		return (init(m_maxfd) < 0) ? NBR_ESYSCALL : fd();
	}
	INTERFACE void on_close() { fin(); }
	INTERFACE result on_read(loop &l, poller::event &e);
	void write(loop &l, poller::event &e);
	inline void reset_wbuf(DSCRPTR fd, wbuf *wbf) {
		ASSERT(!m_wb[fd].m_wp || m_wb[fd].m_wp == wbf);
		if (__sync_bool_compare_and_swap(&(m_wb[fd].m_wp), wbf, NULL)) {
			TRACE("reset_wbuf: %d\n", fd);
			return;
		}
		ASSERT(!m_wb[fd].m_wp);
	}
	inline int set_wbuf(DSCRPTR fd, wbuf *wbf) {
		ASSERT(!m_wb[fd].m_wp || m_wb[fd].m_wp == wbf);
		if (__sync_bool_compare_and_swap(&(m_wb[fd].m_wp), NULL, wbf)) {
			m_wb[fd].m_wp = wbf;
			return p().attach(fd, poller::EV_WRITE);
		}
		ASSERT(m_wb[fd].m_wp);
		return NBR_EALREADY;
	}
	inline int attach(DSCRPTR fd, wbuf *wbf) {
		TRACE("init_wbuf: %d %p %p\n", fd, wbf, m_wb[fd].m_wp);
		ASSERT(m_wb[fd].m_wp == wbf);
		return p().retach(fd, poller::EV_WRITE);
	}
	inline void detach(DSCRPTR fd) { p().detach(fd); }
	inline wbuf *get_wbuf(DSCRPTR fd) {
		return m_wb[fd].m_wp;
	}
};
}
}
#endif
