/***************************************************************
 * loop.h : abstract main loop of worker thread
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__LISTENER_H__)
#define __LISTENER_H__

#include "loop.h"
#include "task.h"
#include "address.h"
#include "serializer.h"
#include "parking.h"

namespace yue {
namespace handler {
typedef util::functional<int (DSCRPTR, DSCRPTR, base**)> accept_handler;
class listener : public base {
	char *m_addr;
	transport *m_t;
	accept_handler m_ah;
	object *m_opt;
	DSCRPTR m_fd;
public:
	listener() : base(LISTENER),
		m_addr(NULL), m_t(NULL), m_opt(NULL), m_fd(INVALID_FD) {}
	~listener() { fin(); }
	inline void fin() {
		if (m_opt) {
			m_opt->fin();
			delete m_opt;
			m_opt = NULL;
		}
		if (m_addr) {
			util::mem::free(m_addr);
			m_addr = NULL;
		}
		if (m_fd != INVALID_FD) {
			net::syscall::close(m_fd, m_t);
			m_fd = INVALID_FD;
		}
	}
	inline DSCRPTR fd() const { return m_fd; }
	bool configure(const char *addr, accept_handler &ah, object *o) {
		fin();
		m_ah = ah;
		if (o && (m_opt = new object)) { *m_opt = *o; }
		return (m_addr = util::str::dup(addr));
	}
	INTERFACE DSCRPTR on_open(U32 &, transport **ppt) {
		char a[256];
		*ppt = (m_t = yue::loop::pk().divide_addr_and_transport(m_addr, a, sizeof(a)));
		if (!net::parking::valid(*ppt)) { return NBR_ENOTFOUND; }
		SKCONF skc = { 120, 65536, 65536, m_opt };
		if ((m_fd = net::syscall::socket(a, &skc, *ppt)) < 0) { return NBR_ESYSCALL; }
		return m_fd;
	}
	INTERFACE void on_close() { fin(); }
	INTERFACE result on_read(loop &l, poller::event &ev) {
		net::address a; DSCRPTR fd, afd = poller::from(ev);
		SKCONF skc = { 120, 65536, 65536, m_opt };
		ASSERT(m_fd == afd);
		while(INVALID_FD != 
			(fd = net::syscall::accept(afd, a.addr_p(), a.len_p(), &skc, l.tl()[afd]))) {
			base *h;
			if (m_ah(fd, afd, &h) < 0 || l.open(*h) < 0) {
				TRACE("accept: fail %d\n", fd);
				h->on_close();
				net::syscall::close(fd, l.tl()[afd]);
				ASSERT(false);
				return again;
			}
		}
		/* after attach is success, it may access from another thread immediately.
		 * so should not perform any kind of write operation to m_clist[fd]. */
		return again;
	}
};
}
}

#endif
