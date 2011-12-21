/***************************************************************
 * session.h : abstruction of message stream
 * 			(thus it does reconnection, re-send... when connection closes)
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of pfm framework.
 * pfm framework is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * pfm framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__SESSION_H__)
#define __SESSION_H__

#include "thread.h"
#include "serializer.h"

namespace yue {
namespace module {
namespace net {
namespace eio {
class loop;

struct session : public wbuf {
	typedef yue::util::functional<bool (session *, int)> watcher;
	typedef wbuf::serial serial;
	struct watch_entry {
		watch_entry(watcher &w) : m_w(w) {}
		watcher m_w;
		struct watch_entry *m_next, *m_prev;
	};
	static bool nop(session *, int) { return false; }
	struct session_event_message {
		session *m_s;
		session::serial m_sn;
		int m_state;
		session_event_message(session *s, int state) :
			m_s(s), m_state(state) { m_sn = s->serial_id(); }
	};
	DSCRPTR m_fd;
	address m_addr;
	transport *m_t;
	U8 m_failure, m_state, m_kind, m_raw;
	thread::mutex m_mtx;
	struct watch_entry *m_top;
	object *m_opt;
	static loop *m_server;
	static array<watch_entry> m_wl;
	static struct watch_entry *m_gtop;
	static thread::mutex m_gmtx;
	static array<session_event_message> m_msgpool;
	static const int MAX_CONN_RETRY = 2;
	static const int CONN_TIMEOUT_SEC = 5;
	static const double CONN_TIMEOUT = 1.f * CONN_TIMEOUT_SEC;
	static const int CONN_TIMEOUT_US = (1000 * 1000 * CONN_TIMEOUT_SEC);
	static const bool KEEP_WATCH = true;
	static const bool STOP_WATCH = false;
	static const U8 DEFAULT_KIND = 0;
	enum {
		INIT,
		CONNECTING,
		ESTABLISH,
		DISABLED,
		CLOSED,
		RECVDATA,
	};
public:
	inline session() : m_fd(INVALID_FD), m_failure(0), m_state(INIT),
		m_kind(DEFAULT_KIND), m_top(NULL), m_opt(NULL) {}
	inline ~session() { setopt(NULL); }
	static inline int init(int maxfd, loop *s) {
		m_server = s;
		if (m_gmtx.init() < 0) { return NBR_EPTHREAD; }
		if (!m_msgpool.init(maxfd, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		return m_wl.init(maxfd, -1, opt_threadsafe | opt_expandable) ?
			NBR_OK : NBR_EEXPIRE;
	}
	static inline void fin() {
		m_gmtx.fin();
		m_msgpool.fin();
		m_wl.fin();
	}
	inline void notice(int state) {
		notice(m_top, m_mtx, state);
		notice(m_gtop, m_gmtx, state);
	}
	void notice(watch_entry *&top, thread::mutex &mtx, int state) {
		if (!top) { return; }
		//TRACE("notice\n");
		util::thread::scoped<util::thread::mutex> lk(mtx);
		if (lk.lock() < 0) {
			DIE("mutex lock fails (%d)\n", util::syscall::error_no());
			return;
		}
		watch_entry *w = top, *pw, *tw = NULL, *last = NULL;
		top = NULL;
		lk.unlock();
		while((pw = w)) {
			w = w->m_next;
			if (!pw->m_w(this, state)) { m_wl.free(pw); }
			else {
				if (!last) { last = pw; }
				pw->m_next = tw;
				tw = pw;
			}
		}
		if (last) {
			if (lk.lock() < 0) {
				DIE("mutex lock fails (%d)\n", util::syscall::error_no());
				return;
			}
			ASSERT(tw);
			last->m_next = top;
			top = tw;
		}
	}
	static inline int add_static_watcher(watcher &wh) {
		return add_watcher(m_gtop, wh, m_gmtx);
	}
	inline int add_watcher(watcher &wh) {
		return add_watcher(m_top, wh, m_mtx);
	}
	static int add_watcher(watch_entry *&we, watcher &wh, thread::mutex &mtx) {
		watch_entry *w = m_wl.alloc(wh);
		w->m_next = we;
		w->m_w = wh;
		//TRACE("add watcher %p\n", w);
		util::thread::scoped<util::thread::mutex> lk(mtx);
		if (lk.lock() < 0) {
			m_wl.free(w);
			return NBR_EPTHREAD;
		}
		we = w;
		return NBR_OK;
	}
	inline void state_change(int st) {
		TRACE("state_change: %p: %d -> %d\n", this, m_state, st);
		m_state = st;
		notice(st);
	}
	int operator () (DSCRPTR fd, int state) {
		switch(state) {
		case S_ESTABLISH:
		case S_SVESTABLISH:
			if (__sync_bool_compare_and_swap(&m_fd, INVALID_FD, fd)) {
				ASSERT( (state == S_ESTABLISH && m_state == CONNECTING) ||
						(state == S_SVESTABLISH && m_state == INIT) );
				m_failure = 0;	/* reset failure times */
				wbuf::update_serial();
				wbuf::write_detach();
				loop::basic_processor::wp().set_wbuf(m_fd, this);
				/* notice connection est to watchers only first time. */
				state_change(ESTABLISH);
			}
			else {
				LOG("%p:this session call connect 2times %d %d\n", this, m_fd, fd);
				session::close(fd);/* close second connection and continue */
				ASSERT(false);
			}
			break;
		case S_SVEST_FAIL:
		case S_SVCLOSE:
			m_failure = MAX_CONN_RETRY;	/* server connection never try reconnect. */
		case S_EST_FAIL:
		case S_CLOSE:
			/* invalidate m_fd */
			if (__sync_bool_compare_and_swap(&m_fd, fd, INVALID_FD)) {
				ASSERT(m_state == ESTABLISH);
				/* invalidate all writer which relate with current fd */
				wbuf::invalidate();
				/* change state and notice */
				state_change(DISABLED);
				/* detach this wbuf from write poller if still attached
				 * (another thread may already initialize wbuf) */
				loop::basic_processor::wp().reset_wbuf(fd, this);
				/* if m_end or server connection failure, wait for next connect */
				TRACE("%p: m_failure = %d\n", this, m_failure);
				if (++m_failure > MAX_CONN_RETRY || connect() < 0) {
					state_change(CLOSED);/* notice connection close to watchers */
					return NBR_OK;
				}
				/* state may change to ESTABLISH, because after static session::connect
				 * called from session::connect (below), then fd attached to read poller,
				 * so another thread already process this fd and S_ESTABLISH is processed.
				 * such a situation happens */
				ASSERT(m_state == CONNECTING || m_state == ESTABLISH);
			}
			else {	/* different fd is noticed to this session */
				if (m_fd != INVALID_FD) {
					LOG("%p:session notice another fd closed %d %d\n", this, m_fd, fd);
				}
				/* if m_fd == fd, ok. eventually happen close event 2 times
				 * or m_fd == INVALID_FD case. connection timed out / closed
				 * without any establishment */
				ASSERT(m_fd == INVALID_FD || m_fd == fd);
				state_change(CLOSED);/* notice connection close to watchers */
			}
			break;
		case S_RECEIVE_DATA:
			notice(RECVDATA);
			break;
		}
		return NBR_OK;
	}
	static inline session_event_message *alloc_event_message(session *s, int state) {
		return m_msgpool.alloc(s, state);
	}
	static inline void free_event_message(session_event_message *m) {
		m_msgpool.free(m);
	}
	inline int attach() {
		TRACE("session::write attach %d\n", m_fd);
		return loop::basic_processor::wp().init_wbuf(m_fd, this);
	}
	inline void reuse() {
		if (m_state == CLOSED) { m_state = INIT; }
	}
	inline int connect() {
		connect_handler ch(*this);
		U8 st = m_state; m_state = CONNECTING;
		int r = session::connect(m_addr, m_t, ch, CONN_TIMEOUT, m_opt, raw());
		if (r < 0) { ASSERT(false); m_state = st; }
		return r;
	}
	inline int reconnect(watcher &wh) { ASSERT(parking::valid(m_t)); return connect(NULL, wh); }
	inline int reconnect() {
		watcher dummy(nop);
		return connect(NULL, dummy);
	}
	static void close(DSCRPTR fd);
	void shutdown();
	const char *addr(char *b, size_t bl) const { return m_addr.get(b, bl, m_t); }
	inline int setaddr(const char *addr) {
		if (!parking::valid(m_t = m_server->divide_addr_and_transport(addr, m_addr))) {
			return NBR_EINVAL;
		}
		return NBR_OK;
	}
	inline void setopt(object *o) {
		if (m_opt) { m_opt->fin(); }
		else if (o) {
			m_opt = new object;
		}
		else {
			return;
		}
		if (o) { (*m_opt) = (*o); }
		else {
			delete m_opt;
			m_opt = NULL;
		}
	}
	inline object *opt() { return m_opt; }
	inline void setraw(U8 f) { m_raw = f; }
	inline bool raw() const { return m_raw != 0; }
public:	/* APIs */
	DSCRPTR fd() const { return m_fd; }
	inline bool valid() const { return m_fd != INVALID_FD; }
	void close();
	inline int connect(const char *addr, watcher &wh) {
		int r;
		if (!wbuf::initialized()) {
			if (m_mtx.init() < 0) { ASSERT(false); return NBR_EPTHREAD; }
		}
		if (valid()) { /* call handler now
			(and it still wanna notice, add_hander) */
			TRACE("%p: already established\n", this);
			ASSERT(m_state == ESTABLISH);
			return wh(this, ESTABLISH) ? add_watcher(wh) : NBR_OK;
		}
		if (!__sync_bool_compare_and_swap(&m_state, INIT, CONNECTING)) {
			TRACE("%p: already start connect\n", this);
			/* add to notification list. handler called later */
			return add_watcher(wh);
		}
		if (addr) {
			if ((r = setaddr(addr)) < 0) { TRACE("setaddr = err(%d)\n", r); return r; }
		}
		if ((r = add_watcher(wh)) < 0) { TRACE("add_watcher = err(%d)\n", r);return r; }
		if ((r = wbuf::init()) < 0) { TRACE("wbuf::init = err(%d)\n", r);return r; }
		m_failure = 0;
		m_raw = 0;
		m_fd = INVALID_FD;
		return ((r = connect()) >= 0) ? NBR_OK : r;
	}
	inline int accept(DSCRPTR fd) {
		int r;
		if (!wbuf::initialized()) {
			if (m_mtx.init() < 0) { ASSERT(false); return NBR_EPTHREAD; }
		}
		if ((r = wbuf::init()) < 0) { return r; }
		m_failure = 0;
		m_state = INIT;
		m_fd = INVALID_FD;
		return util::syscall::get_sock_addr(fd, m_addr.addr_p(), m_addr.len_p());
	}
	inline int write(char *p, size_t l) {
		return wbuf::send<wbuf::raw>(wbuf::raw::arg(reinterpret_cast<U8 *>(p), l), *this);
	}
	inline int writev(struct iovec *v, U32 s) {
		return wbuf::send<wbuf::iov>(wbuf::iov::arg(v, s), *this);
	}
	inline int writef(DSCRPTR s, size_t ofs, size_t sz) {
		return wbuf::send<wbuf::file>(wbuf::file::arg(s, ofs, sz), *this);
	}
	template <class SR, class OBJ>
	inline int writeo(SR &sr, OBJ &o) {
		if (m_t && m_t->dgram) {
			return wbuf::send<wbuf::template obj2<OBJ, SR, wbuf::dgram> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::dgram>::arg_dgram(o, sr, m_addr), *this);
		}
		else {
			return wbuf::send<wbuf::template obj2<OBJ, SR, wbuf::raw> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::raw>::arg(o, sr), *this);
		}
	}
	inline int read(char *p, size_t l) {
		return syscall::read(m_fd, p, l, m_t);
	}
	void set_kind(U8 k) { m_kind = k; }
	U8 kind() const { return m_kind; }
public: /* synchronized socket APIs */
	int sync_connect(local_actor &la, int timeout = CONN_TIMEOUT_US);
	int sync_read(local_actor &la, object &o, int timeout = CONN_TIMEOUT_US);
	int sync_write(local_actor &la, int timeout = CONN_TIMEOUT_US);
public:
	static int connect(address &a, transport *t, connect_handler &ch, 
		double t_o, object *o, bool raw);
	FORBID_COPY(session);
};

}
}
}
}

#endif
