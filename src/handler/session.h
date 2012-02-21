/***************************************************************
 * session.h : abstraction of network connection
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SESSION_H__)
#define __SESSION_H__

#include "loop.h"
#include "thread.h"
#include "serializer.h"
#include "wbuf.h"
#include "sbuf.h"
#include "handshake.h"
#include "monitor.h"

namespace yue {
class server;
namespace handler {

struct session : public base {
public:
	static const int MAX_CONN_RETRY = 5;
	static const int CONN_TIMEOUT_SEC = 5;
	static const double CONN_TIMEOUT = 1.f * CONN_TIMEOUT_SEC;
	static const int CONN_TIMEOUT_US = (1000 * 1000 * CONN_TIMEOUT_SEC);
	static const U8 DEFAULT_KIND = 0;
	static const size_t MINIMUM_FREE_PBUF = 1024;
	enum {
		INVALID,
		HANDSHAKE,
		SVHANDSHAKE,
		ESTABLISH,
		SVESTABLISH,
		DISABLED,
		CLOSED,
		RECVDATA,
	};
	enum {
		STREAM,
		DGRAM,
		RAW,
	};
	typedef enum {
		S_ESTABLISH,
		S_SVESTABLISH,
		S_EST_FAIL,
		S_SVEST_FAIL,
		S_CLOSE,
		S_SVCLOSE,
		S_RECEIVE_DATA,	//only RAWESTABLISH emit this event.
	} message;
public:
	typedef net::wbuf::serial serial;
	typedef monitor::watcher watcher;
	typedef net::wbuf wbuf;
	typedef util::pbuf pbuf;
	typedef net::address address;
	typedef util::handshake handshake;
	struct handle {
		session *m_s;
		session::serial m_sn;
		inline handle(session *s) : m_s(s), m_sn(s->serial_id()) {}
		inline bool valid() const { return m_sn == m_s->serial_id(); }
		inline DSCRPTR parent_fd() const { return m_s->afd(); }
	};
	struct datagram_handle : public handle {
		address m_a;
		inline datagram_handle(session *s, net::address &a) : handle(s), m_a(a) {}
		template <class SR, class OBJ>
		inline int send(SR &sr, OBJ &o) {
			if (!valid()) { return NBR_EINVAL; }
			return m_s->wbf().send<wbuf::template obj2<OBJ, SR, wbuf::dgram> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::dgram>::arg_dgram(o, sr, m_a), *m_s);
		}
	};
	struct stream_handle : public handle {
		inline stream_handle(session *s) : handle(s) {}
		template <class SR, class OBJ>
		inline int send(SR &sr, OBJ &o) {
			if (!valid()) { return NBR_EINVAL; }
			return m_s->wbf().send<wbuf::template obj2<OBJ, SR, wbuf::raw> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::raw>::arg(o, sr), *m_s);
		}
	};
	struct loop_handle {
		server *m_l;
		loop_handle(server *l) : m_l(l) {}
		template <class SR, class OBJ> inline int send(SR &sr, OBJ &o);
	};
	struct session_event_message {
		handle m_h;
		int m_state;
		session_event_message(session *s, int state) : m_h(s), m_state(state) {}
	};
protected:
	DSCRPTR m_fd, m_afd;
	wbuf m_wbuf;
	pbuf m_pbuf;
	serializer m_sr;
	address m_addr;
	transport *m_t;
	U8 m_failure, m_state, padd;
	U8 m_kind:4, m_type:3, m_raw:1;
	object *m_opt;
	monitor m_mon;
	static handshake m_hs;
	static array<session_event_message> m_msgpool;
public:
	inline session() : base(SESSION), m_fd(INVALID_FD), m_afd(INVALID_FD),
		m_wbuf(), m_pbuf(), m_sr(), m_addr(),
		m_failure(0), m_state(INVALID), m_kind(DEFAULT_KIND),m_type(STREAM),m_raw(0),
		m_opt(NULL), m_mon() {}
	inline ~session() { setopt(NULL); }
	inline wbuf &wbf() { return m_wbuf; }
	inline wbuf *pwbf() { return &m_wbuf; }
	inline const wbuf &wbf() const { return m_wbuf; }
	inline pbuf &pbf() { return m_pbuf; }
	inline serial serial_id() const { return wbf().serial_id(); }
	static inline handshake &handshakers() { return m_hs; }
	static inline int init(int maxfd) {
		if (!m_msgpool.init(maxfd, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (m_hs.init(maxfd) < 0) { return NBR_EMALLOC; }
		return monitor::static_init(maxfd);
	}
	static inline void fin() {
		m_msgpool.fin();
		monitor::static_fin();
		m_hs.fin();
	}
	inline void state_change(int st) {
		TRACE("state_change: %p: %d -> %d\n", this, m_state, st);
		m_state = st;
		m_mon.notice(this, st);
	}
	inline int add_watcher(watcher &w) { return m_mon.add_watcher(w); }
	int operator () (DSCRPTR fd, bool success) {
		switch(m_state) {
		case HANDSHAKE:
			this->operator()(fd, success ? S_ESTABLISH : S_EST_FAIL); break;
		case SVHANDSHAKE:
			this->operator()(fd, success ? S_SVESTABLISH : S_SVEST_FAIL); break;
		default: ASSERT(false); return NBR_EINVAL;
		}
		return NBR_OK;
	}
	int operator () (DSCRPTR fd, message event) {
		switch(event) {
		case S_ESTABLISH:
		case S_SVESTABLISH:
			if (m_fd == fd) {
				ASSERT( (event == S_ESTABLISH && m_state == HANDSHAKE) ||
					(event == S_SVESTABLISH && m_state == SVHANDSHAKE) );
				m_failure = 0;	/* reset failure times */
				wbf().update_serial();
				wbf().write_detach();
				if (loop::wp().set_wbuf(fd, pwbf()) < 0) {
					close();/* close second connection and continue */
					ASSERT(false);
					break;
				}
				/* notice connection est to watchers only first time. */
				state_change(event == S_ESTABLISH ? ESTABLISH : SVESTABLISH);
			}
			else {
				LOG("%p:this session call connect 2times %d %d\n", this, m_fd, fd);
				close();/* close second connection and continue */
				ASSERT(false);
			}
			break;
		case S_SVEST_FAIL:
		case S_SVCLOSE:
			m_failure = MAX_CONN_RETRY;	/* server connection never try reconnect. fall through */
		case S_EST_FAIL:
		case S_CLOSE:
			/* invalidate m_fd */
			if (__sync_bool_compare_and_swap(&m_fd, fd, INVALID_FD)) {
				ASSERT(m_state == ESTABLISH || m_state == SVESTABLISH ||
					m_state == HANDSHAKE || m_state == SVHANDSHAKE);
				/* invalidate all writer which relate with current fd */
				wbf().invalidate();
				/* change state and notice */
				state_change(DISABLED);
				/* detach this wbuf from write poller if still attached
				 * (another thread may already initialize wbuf) */
				loop::wp().reset_wbuf(fd, pwbf());
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
				ASSERT(m_state == HANDSHAKE || m_state == ESTABLISH);
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
			m_mon.notice(this, RECVDATA);
			break;
		}
		return NBR_OK;
	}
	INTERFACE result on_read(loop &l, poller::event &ev) {
		int r; handshake::handshaker hs;
		DSCRPTR fd = poller::from(ev);
		switch(m_state) {
		case HANDSHAKE:
		case SVHANDSHAKE:
			TRACE("operator () (stream_handler)");
			if (poller::closed(ev)) {
				TRACE("closed detected: %d\n", fd);
				return destroy;
			}
			/* do SSL negotiation or something like that. */
			else if ((r = l.handshake(ev)) <= 0) {
				TRACE("handshake called (%d)\n",r );
				/* back to poller or close fd */
				/* EV_WRITE for knowing connection establishment */
				if (r == NBR_OK) { return again_rw; }
				else {
					if (handshakers().find_and_erase(fd, hs)) {
						hs.m_h(fd, false);
					}
					return destroy;
				}
			}
			if (m_state == HANDSHAKE) {
				//TRACE("fd=%d, handler=%p\n", fd, handshakers().find(fd));
				//handshakers().find(fd)->m_ch.dump();
				if (handshakers().find_and_erase(fd, hs)) {
					ASSERT(fd == hs.m_fd);
					TRACE("fd=%d, connect_handler success\n", fd);
					hs.m_h(fd, true);
					if (!poller::readable(ev)) { return again; }
					TRACE("fd %d already readable, proceed to establish\n", fd);
				}
				/* already timed out. this fd will close soon. ignore. *OR*
				 * already closed. in that case, fd is closed. */
				else {
					return nop;
				}
			}
			else {
				this->operator()(fd, true);
			}
		case ESTABLISH:
		case SVESTABLISH:
			//TRACE("stream_read: %p, fd = %d,", this, fd);
			if (poller::closed(ev)) {
				TRACE("closed detected: %d\n", fd);
				/* remote peer closed, close immediately this side connection.
				 * (if server closed connection before FIN from client,
				 * connection will be TIME_WAIT status and remains long time,
				 * but if not closed immediately, then CLOSE_WAIT and also remains long time...) */
				return destroy;
				//return handler_result::nop;
			}
			ASSERT(m_fd == fd);
			return read(l);
			//TRACE("result: %d\n", r);
		case DISABLED:
			return nop;
		case CLOSED:
			return destroy;
		default:
			ASSERT(false);
			return destroy;
		}
	}
	static inline session_event_message *alloc_event_message(session *s, int state) {
		return m_msgpool.alloc(s, state);
	}
	static inline void free_event_message(session_event_message *m) {
		m_msgpool.free(m);
	}
	inline int attach() {
		TRACE("session::write attach %d\n", m_fd);
		return loop::wp().attach(m_fd, pwbf());
	}
	inline void reuse() {
		if (m_state == CLOSED) { m_state = INVALID; }
	}
	inline int connect() {
		handshake::handler ch(*this);
		m_state = HANDSHAKE;
		if ((m_fd = session::connect(m_addr, m_t, ch, CONN_TIMEOUT, m_opt, raw())) < 0) {
			ASSERT(false); return m_fd;
		}
		if (loop::open(*this) < 0) { return NBR_ESYSCALL; }
		return m_fd;
	}
	inline int reconnect(watcher &wh) { 
		ASSERT(net::parking::valid(m_t)); 
		return connect(NULL, wh); 
	}
	inline int reconnect() {
		monitor::watcher dummy(monitor::nop);
		return connect(NULL, dummy);
	}
	static void close(DSCRPTR fd);
	void shutdown() { if (valid()) { loop::close(m_fd); } }
	const char *addr(char *b, size_t bl) const { return m_addr.get(b, bl, m_t); }
	const char *uri(char *b, size_t bl) const {
		return address::to_uri(b, bl, m_addr, m_t);
	}
	inline int setaddr(const char *addr) {
		if (!net::parking::valid(
			m_t = loop::pk().divide_addr_and_transport(addr, m_addr))) {
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
	inline void set_type() {
		m_type = (m_raw ? RAW : (net::parking::stream(m_t) ? STREAM : DGRAM));
	}
	inline void set_kind(U8 k) { m_kind = k; }
	inline U8 kind() const { return m_kind; }
public:	/* APIs */
	inline DSCRPTR fd() const { return m_fd; }
	inline DSCRPTR afd() const { return m_afd; }
	inline bool valid() const { return m_fd != INVALID_FD; }
	INTERFACE DSCRPTR on_open(U32 &flag, transport **ppt) {
		ASSERT(m_fd != INVALID_FD);
		*ppt = m_t;
		flag = poller::EV_READ | poller::EV_WRITE;
		set_type();
		return m_fd;
	}
	INTERFACE void on_close() {
		handshake::handshaker hs; DSCRPTR fd = m_fd;
		if (handshakers().find_and_erase(fd, hs)) {
			/* closed during handshaking */
			TRACE("fd = %d, execute closed event\n", fd);
		}
		/* after operator () called, m_fd will be -1 */
		switch(m_state) {
		case HANDSHAKE: case ESTABLISH: 
			this->operator () (m_fd, S_CLOSE); break;
		case SVHANDSHAKE: case SVESTABLISH: 
			this->operator() (m_fd, S_SVCLOSE); break;
		default: ASSERT(false); break;
		}
		m_afd = INVALID_FD;
		net::syscall::close(fd, m_t);
	}
	inline result read_and_parse(loop &l, int &parse_result);
	inline result read_stream(loop &l);
	inline result read_dgram(loop &l);
	inline result read_raw(loop &l);
	result read(loop &l);
	void close();
	inline int connect(const char *addr, monitor::watcher &wh) {
		int r;
		if (!wbf().initialized()) {
			if (m_mon.init() < 0) { ASSERT(false); return NBR_EPTHREAD; }
		}
		if (valid()) { /* call handler now
			(and it still wanna notice, add_hander) */
			TRACE("%p: already established\n", this);
			ASSERT(m_state == ESTABLISH);
			return wh(this, ESTABLISH) ? add_watcher(wh) : NBR_OK;
		}
		reuse();	//try reuse
		if (!__sync_bool_compare_and_swap(&m_state, INVALID, HANDSHAKE)) {
			TRACE("%p: already start connect\n", this);
			/* add to notification list. handler called later */
			return add_watcher(wh);
		}
		if (addr) {
			if ((r = setaddr(addr)) < 0) { TRACE("setaddr = err(%d)\n", r); return r; }
		}
		if ((r = add_watcher(wh)) < 0) { TRACE("add_watcher = err(%d)\n", r);return r; }
		if ((r = wbf().init()) < 0) { TRACE("wbf().init = err(%d)\n", r);return r; }
		m_failure = 0;
		return ((r = connect()) >= 0) ? NBR_OK : r;
	}
	inline int accept(DSCRPTR fd, DSCRPTR afd, address &raddr) {
		int r;
		if (!wbf().initialized()) {
			if (m_mon.init() < 0) { ASSERT(false); return NBR_EPTHREAD; }
		}
		if ((r = wbf().init()) < 0) { return r; }
		m_failure = 0;
		m_state = SVHANDSHAKE;
		m_fd = fd;
		m_afd = afd;
		m_t = loop::tl()[afd];	/* inherit from listener */
		m_addr = raddr;
		return NBR_OK;
	}
	inline int listen(const char *addr, object *opt) {
		int r;
		if ((r = wbf().init()) < 0) { return r; }
		m_failure = 0;
		m_state = SVHANDSHAKE;
		if ((r = setaddr(addr)) < 0) { return r; }
		SKCONF skc = { 120, 65536, 65536, opt };
		if (opt) {
			skc.rblen = (*opt)("rblen",65536);
			skc.wblen = (*opt)("wblen",65536);
			skc.timeout = (*opt)("timeout",120);
		}
		char a[256];
		if ((m_fd = net::syscall::socket(
			m_addr.get(a, sizeof(a), m_t), &skc, m_t)) < 0) {
			return m_fd;
		}
		/* datagram listner: m_fd == accepted fd also */
		m_afd = m_fd;
		/* make it established */
		this->operator () (m_fd, S_SVESTABLISH);
		return m_fd;
	}
	INTERFACE result on_write(poller &, DSCRPTR fd) {
		ASSERT(fd == m_fd);
		return wbf().write(m_fd, m_t);
	}
	inline int write(char *p, size_t l) {
		return wbf().send<wbuf::raw>(wbuf::raw::arg(reinterpret_cast<U8 *>(p), l), *this);
	}
	inline int writev(struct iovec *v, U32 s) {
		return wbf().send<wbuf::iov>(wbuf::iov::arg(v, s), *this);
	}
	inline int writef(DSCRPTR s, size_t ofs, size_t sz) {
		return wbf().send<wbuf::file>(wbuf::file::arg(s, ofs, sz), *this);
	}
	template <class SR, class OBJ>
	inline int writeo(SR &sr, OBJ &o) {
		if (m_type == DGRAM) {
			return wbf().send<wbuf::template obj2<OBJ, SR, wbuf::dgram> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::dgram>::arg_dgram(o, sr, m_addr), *this);
		}
		else if (m_type == STREAM) {
			return wbf().send<wbuf::template obj2<OBJ, SR, wbuf::raw> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::raw>::arg(o, sr), *this);
		}
		else {
			ASSERT(false);
			return NBR_ENOTFOUND;
		}
	}
public:
	inline int read(char *p, size_t l) {
		return net::syscall::read(m_fd, p, l, m_t);
	}
	inline int sys_write(char *p, size_t l) {
		return net::syscall::write(m_fd, p, l, m_t);
	}
	inline int sys_writev(struct iovec *v, U32 s) {
		return net::syscall::writev(m_fd, v, s, m_t);
	}
	inline int sys_writef(DSCRPTR s, off_t *ofs, size_t sz) {
		return net::syscall::sendfile(m_fd, s, ofs, sz, m_t);
	}
public: /* synchronized socket APIs */
	int sync_connect(loop &l, int timeout = CONN_TIMEOUT_US);
	int sync_read(loop &l, object &o, int timeout = CONN_TIMEOUT_US);
	int sync_write(loop &l, int timeout = CONN_TIMEOUT_US);
public:
	static int connect(address &a, transport *t, handshake::handler &ch, 
		double t_o, object *o, bool raw);
	FORBID_COPY(session);
};

}
}

extern "C" {
extern int socket_read(void *s, char *p, size_t sz);
extern int socket_write(void *s, char *p, size_t sz);
extern int socket_writev(void *s, struct iovec *p, size_t sz);
extern int socket_writef(void *s, DSCRPTR fd, size_t ofs, size_t sz);
extern int socket_sys_write(void *s, char *p, size_t sz);
extern int socket_sys_writev(void *s, struct iovec *p, size_t sz);
extern int socket_sys_writef(void *s, DSCRPTR fd, off_t *ofs, size_t sz);
}

#endif
