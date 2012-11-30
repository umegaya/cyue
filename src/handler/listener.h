/***************************************************************
 * listener.h : stream socket accept listener
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
#include "event.h"

namespace yue {
namespace handler {
typedef util::functional<int (DSCRPTR, class listener *, net::address &, base**)> accept_handler;
class listener : public base {
	DSCRPTR m_fd;
	transport *m_t;
	char *m_addr;
	object *m_opt;
	accept_handler m_ah;
	SKCONF m_conf;
	U8 m_flag, padd[3];
public:
	listener() : base(LISTENER), m_fd(INVALID_FD), m_t(NULL), m_addr(NULL), m_opt(NULL), m_flag(0) {}
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
		m_flag = 0;
	}
	inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
	INTERFACE DSCRPTR fd() { return m_fd; }
	INTERFACE transport *t() { return m_t; }
	inline SKCONF &config() { return m_conf; }
	inline SKCONF skconf() {
		SKCONF skc = { 120, 65536, 65536, NULL };
		if (m_opt) {
			skc.rblen = (*m_opt)("rblen",65536);
			skc.wblen = (*m_opt)("wblen",65536);
			skc.timeout = (*m_opt)("timeout",120);
			skc.proto_p = m_opt;
		}
		return skc;
	}
	template <class H>
	int configure(const char *addr, H &h, object *o) {
		fin();
		m_ah = accept_handler(h);
		if (o && (m_opt = new object)) { *m_opt = *o; }
		m_conf = skconf();
		return (m_addr = util::str::dup(addr)) ? NBR_OK : NBR_EMALLOC;
	}
	int open() {
		if (!__sync_bool_compare_and_swap(&m_flag, 0, 1)) {
			return NBR_EALREADY;
		}
		return loop::open(*this);
	}
	INTERFACE DSCRPTR on_open(U32 &) {
		char a[256];
		m_t = yue::loop::pk().divide_addr_and_transport(m_addr, a, sizeof(a));
		if (!net::parking::valid(m_t)) { return NBR_ENOTFOUND; }
		if ((m_fd = net::syscall::socket(a, &m_conf, m_t)) < 0) { return NBR_ESYSCALL; }
		return m_fd;
	}
	INTERFACE void on_close() { fin(); }
	INTERFACE result on_read(loop &l, poller::event &ev) {
		net::address a; DSCRPTR fd;
		ASSERT(m_fd == poller::from(ev));
		while(INVALID_FD != 
			(fd = net::syscall::accept(m_fd, a.addr_p(), a.len_p(), &m_conf, m_t))) {
			base *h = NULL;
			if (m_ah(fd, this, a, &h) < 0) {
				TRACE("accept: fail %d\n", fd);
				if (h) { h->on_close(); }
				net::syscall::close(fd, m_t);
				return read_again;
			}
			emit(h);
		}
		/* after attach is success, it may access from another thread immediately.
		 * so should not perform any kind of write operation to m_clist[fd]. */
		return read_again;
	}
	void emit(base *h) {
		event::listener ev(this, h);
		emittable::emit_one(event::ID_LISTENER, ev);
	}
};
}
}

#endif
