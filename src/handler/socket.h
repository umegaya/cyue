/***************************************************************
 * socket.h : base class for describing normal socket (file, stream, datagram)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SOCKET_H__)
#define __SOCKET_H__

#include "loop.h"
#include "thread.h"
#include "serializer.h"
#include "wbuf.h"
#include "sbuf.h"
#include "handshake.h"
#include "listener.h"

#define SOCKET_TRACE(...) //printf(__VA_ARGS__)

namespace yue {
namespace handler {
class socket : public base {
public:
	typedef net::wbuf wbuf;
	typedef util::pbuf pbuf;
	typedef net::address address;
	typedef util::handshake handshake;
public:
	static const int CONN_TIMEOUT_SEC = 5;
	static const double CONN_TIMEOUT = 1.f * CONN_TIMEOUT_SEC;
	static const int CONN_TIMEOUT_US = (1000 * 1000 * CONN_TIMEOUT_SEC);
	static const size_t MINIMUM_FREE_PBUF = 1024;
	enum {
		HANDSHAKE,
		WAITACCEPT,
		ESTABLISH,
		RECVDATA,
		CLOSED,
		MAX_STATE,
	};
	enum {
		NONE,
		STREAM,
		DGRAM,
		RAW,
	};
	enum {
		F_FINALIZED = 1 << 0,
		F_INITIALIZED = 1 << 1,
		F_CACHED = 1 << 2,
		F_CLOSED = 1 << 3,
	};
protected:
	DSCRPTR m_fd;
	transport *m_t;
	emittable *m_listener;
	wbuf m_wbuf;
	pbuf m_pbuf;
	serializer m_sr;
	address m_addr;
	U8 m_state, m_socket_type, m_flags, padd;
	object *m_opt;
	static handshake m_hs;
public:
	inline socket() : base(SOCKET),
		m_fd(INVALID_FD), m_t(NULL), m_listener(NULL),
		m_wbuf(), m_pbuf(), m_sr(), m_addr(),
		m_state(CLOSED), m_socket_type(NONE), m_flags(0), m_opt(NULL) {}
	inline ~socket() { fin(); }
	inline wbuf &wbf() { return m_wbuf; }
	inline const wbuf &wbf() const { return m_wbuf; }
	inline pbuf &pbf() { return m_pbuf; }
	inline address &addr() { return m_addr; }
	inline const emittable *ns_key() const { return m_listener ? m_listener : this; }
	inline emittable *ns_key() { return m_listener ? m_listener : this; }
	inline bool is_server_conn() const { return m_listener; }
	inline emittable *accepter() { return m_listener; }
	INTERFACE DSCRPTR fd() { return m_fd; }
	INTERFACE transport *t() { return m_t; }
	inline bool has_flag(U8 f) const { return m_flags & f; }
	inline void set_flag(U8 f, bool on) { 
		if (on) { m_flags |= f; } else { m_flags &= ~(f); } 
	}
	inline bool valid() const { return (m_state > HANDSHAKE && m_state < CLOSED); }
	inline int state() const { return m_state; }
	static inline handshake &handshakers() { return m_hs; }
	static inline int static_init(int maxfd) { return m_hs.init(maxfd); }
	static inline void static_fin() { m_hs.fin(); }
	static inline bool check(const emittable *p) { return p->type() == SOCKET; }
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
	inline void fin() { 
		setopt(NULL); 
	}
	inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
	inline const char *resolved_uri(char *b, size_t l) {
		return address::to_uri(b, l, m_addr, m_t);
	}
protected: //util
	inline int setopt(object *o) {
		if (o) {
			if (m_opt) { m_opt->fin(); }
			else { m_opt = new object; }
			if (!m_opt) { ASSERT(false); return NBR_EMALLOC; }
			*m_opt = *o;
		}
		else if (m_opt) {
			m_opt->fin();
			delete m_opt;
			m_opt = NULL;
		}
		return NBR_OK;
	}
	inline int setaddr(const char *addr) {
		if (!net::parking::valid(
			m_t = loop::pk().divide_addr_and_transport(addr, m_addr))) {
			return NBR_EINVAL;
		}
		return NBR_OK;
	}
	inline bool skip_server_accept() const {
		/* TODO: now no 'server accept' concept for udp connection */
		return (m_t && m_t->dgram) ||
			(m_opt && ((*m_opt)("raw", 0) || (*m_opt)("skip_server_accept", 0)));
	}
	inline void set_kind() {
		m_socket_type = ((m_opt && (*m_opt)("raw", 0)) ? RAW :
			(net::parking::stream(m_t) ? STREAM : DGRAM));
	}
	inline void emit(int state);
public://state change
	int operator () (DSCRPTR fd, bool success) {
		if (m_fd != fd) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		switch(m_state) {
		case HANDSHAKE:
			state_change(success ? WAITACCEPT : CLOSED, HANDSHAKE); break;
		/* it is possible when fiber call socket::close 
		(because fiber execution run concurrently with handler processing */
		case CLOSED: TRACE("already closed %p\n", this); break;
		default: ASSERT(false); return NBR_EINVAL;
		}
		return NBR_OK;
	}
	inline bool grant() { return state_change(ESTABLISH, WAITACCEPT); }
	inline bool authorized() const { return (m_state == ESTABLISH); }
	bool state_change(U8 new_state, U8 old_state) {
		if (!__sync_bool_compare_and_swap(&m_state, old_state, new_state)) {
			TRACE("fail to change state: %u expected, but %u\n", old_state, m_state);
			//multiple thread call yue_emitter_open simultaneously, its normal.
			return false;
		}
		TRACE("state_change %u -> %u\n", old_state, new_state);
		switch (new_state) {
		case HANDSHAKE:
			if (has_flag(F_FINALIZED)) {
				return false;
			}
			set_flag(F_INITIALIZED, false);
			break;
		case WAITACCEPT:
			ASSERT(old_state == HANDSHAKE);
			wbf().update_serial();
			wbf().write_detach();
			if (loop::wp().set_wbuf(fd(), &(wbf())) < 0) {
				close();/* close connection */
				ASSERT(false);
				break;
			}
			emit(WAITACCEPT);
			if (skip_server_accept()) { grant(); }
			/* otherwise, user program manually call grant() to permit access from peer */
			break;
		case ESTABLISH:
			emit(ESTABLISH);
			break;
		case CLOSED:
			ASSERT(old_state == ESTABLISH || old_state == WAITACCEPT || old_state == HANDSHAKE);
			/* invalidate all writer which relate with current fd */
			wbf().invalidate();
			/* detach this wbuf from write poller if still attached
			 * (another thread may already initialize wbuf) */
			loop::wp().reset_wbuf(fd(), &(wbf()));
			/* if server connection, add finalized flag to notice this session no more can be used. */
			/* for client session, user call close and it set below flag at inside. */
			if (is_server_conn()) { set_flag(F_FINALIZED, true); }
			/* notice connection close to watchers (if F_FINALIZED is on, each watcher have to do finalize) */
			emit(CLOSED);
			break;
		default:
			ASSERT(false);
			break;
		}
		return true;
	}
public://open
	//for client connection
	inline int configure(const char *addr, object *opt, emittable *listener = NULL) {
		int r;
		if ((r = setopt(opt)) < 0) { return r; }
		m_listener = listener;
		return setaddr(addr);
	}
	inline int configure(DSCRPTR fd, listener *l, address &raddr) {
		m_fd = fd;
		m_listener = l;
		m_t = l->t();	/* inherit from listener */
		m_addr = raddr;
		return NBR_OK;
	}
	int open_client_conn(double timeout) {
		if (!state_change(HANDSHAKE, CLOSED)) {
			return NBR_EALREADY;
		}
		SKCONF skc = skconf();
		if ((m_fd = net::syscall::socket(NULL, &skc, m_t)) < 0) { goto end; }
		if (net::syscall::connect(m_fd, m_addr.addr_p(), m_addr.len(), m_t) < 0) {
			goto end;
		}
		if (wbf().init() < 0) { goto end; }
		if (m_hs.start_handshake(m_fd, *this, timeout) < 0) {
			goto end;
		}
		if (loop::open(*this) < 0) { goto end; }
		TRACE("session : connect success\n");
		return ((int)m_fd);
	end:
		if (m_fd >= 0) {
			net::syscall::close(m_fd, m_t);
			m_fd = INVALID_FD;
		}
		return NBR_ESYSCALL;
	}
	//for server stream connection
	int open_server_conn(double timeout) {
		int r;
		if (!state_change(HANDSHAKE, CLOSED)) {
			return NBR_EALREADY;
		}
		if ((r = wbf().init()) < 0) { return r; }
		if ((r = setopt(NULL)) < 0) { return r; }
		if ((r = m_hs.start_handshake(m_fd, *this, timeout)) < 0) {
			return r;
		}
		return loop::open(*this);
	}
	//for datagram listener
	int open_datagram_server() {
		int r;
		if (!state_change(ESTABLISH, CLOSED)) {
			return NBR_EALREADY;
		}
		if ((r = wbf().init()) < 0) { return r; }
		SKCONF skc = skconf();
		char a[256];
		ASSERT(m_t->dgram);
		if ((m_fd = net::syscall::socket(
			m_addr.get(a, sizeof(a), m_t), &skc, m_t)) < 0) {
			r = m_fd;
			m_fd = INVALID_FD;
			return r;
		}
		if (loop::open(*this) < 0) {
			if (m_fd >= 0) { net::syscall::close(m_fd, m_t); }
			m_fd = INVALID_FD;
			return NBR_ESYSCALL;
		}
		return m_fd;
	}
	int open() {
		if (is_server_conn()) {
			int r = ((m_socket_type != DGRAM) ? 
				open_server_conn(skconf().timeout) : open_datagram_server());
			if (r < 0 && r != NBR_EALREADY) {
				return r;
			}
		}
		return NBR_OK;	/* client connection start is more lazy (when try to invoke some RPC) */
	}
	INTERFACE DSCRPTR on_open(U32 &flag) {
		ASSERT(m_fd != INVALID_FD);
		flag = 0;//poller::EV_WRITE;
		set_kind();
		base::sched_read(m_fd);
		return m_fd;
	}
public://close
	INTERFACE void on_close() {
		TRACE("on_close %p\n", this);
		handshake::handshaker hs;
		if (handshakers().find_and_erase(m_fd, hs)) {
			/* closed during handshaking */
			TRACE("fd = %d, execute closed event %p\n", m_fd, this);
		}
		switch(m_state) {
		case HANDSHAKE: case ESTABLISH: case WAITACCEPT:
			state_change(CLOSED, m_state); break;
		/* when establish timeout => connection closed, this happen.
		 * in that case, m_state should be CLOSED. */
		default:
			TRACE("already closed: state = %u\n", m_state);
			ASSERT(m_fd == INVALID_FD);
			ASSERT(m_state == CLOSED);
			return;
		}
		net::syscall::close(m_fd, m_t);
		m_fd = INVALID_FD;
		if (has_flag(F_FINALIZED)) {
			emittable::remove_all_watcher();
		}
	}
	inline void close();
public://read
	inline result read_and_parse(loop &l, int &parse_result);
	inline result read_stream(loop &l);
	inline result read_dgram(loop &l);
	inline result read_raw(loop &l);
	inline result read(loop &l);
	INTERFACE result on_read(loop &l, poller::event &ev) {
		result r = on_read_impl(l, ev);
		/* if finalized & initialized, first handshake process comming after this socket closed */
		/* F_CLOSED flag for epoll system. not 100% but cover most case */
		return has_flag(F_FINALIZED) ? ((poller::initialized(ev) || has_flag(F_CLOSED)) ? nop : destroy) : r;
	}
	inline result on_read_impl(loop &l, poller::event &ev) {
		int r; handshake::handshaker hs;
		ASSERT(m_state == CLOSED || m_fd == poller::from(ev) || (!poller::readable(ev) && !poller::writable(ev)));
		switch(m_state) {
		case HANDSHAKE:
			TRACE("%p:%s: operator () (stream_handler)", this, poller::initialized(ev) ? "first" : "next");
			if (poller::closed(ev)) {
				SOCKET_TRACE("closed detected: %d %04x\n", m_fd, ev.flags);
				return destroy;
			}
			/* do SSL negotiation or something like that. */
			else if ((r = l.handshake(ev, m_t)) < 0) {
				SOCKET_TRACE("handshake called (%d)\n",r );
				/* back to poller or close m_fd */
				/* EV_WRITE for knowing connection establishment */
				if (r == NBR_ESEND) { 
					return write_again; 
				}
				else if (r == NBR_EAGAIN) {
					return read_again;
				}
				else {
					if (handshakers().find_and_erase(m_fd, hs)) {
						hs.m_h(m_fd, false);
					}
					return destroy;
				}
			}
			//TRACE("m_fd=%d, handler=%p\n", m_fd, handshakers().find(m_fd));
			//handshakers().find(m_fd)->m_ch.dump();
			if (handshakers().find_and_erase(m_fd, hs)) {
				ASSERT(m_fd == hs.m_fd);
				TRACE("m_fd=%d, connect_handler success %p\n", m_fd, this);
				hs.m_h(m_fd, true);
				if (!poller::readable(ev)) { return read_again; }
				TRACE("m_fd %d already readable, proceed to establish %p\n", m_fd, this);
			}
			/* already timed out. this m_fd will close soon. ignore. *OR*
			 * already closed. in that case, m_fd is closed. */
			else {
				return nop;
			}
		case WAITACCEPT:
		case ESTABLISH: {
			//TRACE("stream_read: %p, m_fd = %d,", this, m_fd);
			if (poller::closed(ev)) {
				SOCKET_TRACE("closed detected: %d %04x\n", m_fd, ev.flags);
				/* remote peer closed, close immediately this side connection.
				 * (if server closed connection before FIN from client,
				 * connection will be TIME_WAIT status and remains long time,
				 * but if not closed immediately, then CLOSE_WAIT and also remains long time...) */
				return destroy;
			}
			return read(l);
		}
		case CLOSED:
			return nop;
		default:
			ASSERT(false);
			SOCKET_TRACE("invalid socket status: %d\n", m_state);
			return destroy;
		}
	}
public: //write
	inline int attach() {
		TRACE("attached for write %d\n", fd());
		return loop::wp().attach(m_fd, &wbf());
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
	inline int writeo(SR &sr, OBJ &o, const address *a = NULL) {
		if (m_socket_type == DGRAM) {
			return wbf().send<wbuf::template obj2<OBJ, SR, wbuf::dgram> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::dgram>::arg_dgram(o, sr, a ? *a : m_addr), *this);
		}
		else if (m_socket_type == STREAM) {
			return wbf().send<wbuf::template obj2<OBJ, SR, wbuf::raw> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::raw>::arg(o, sr), *this);
		}
		else {
			ASSERT(false);
			return NBR_ENOTFOUND;
		}
	}
	INTERFACE result on_write(poller &) {
		if (has_flag(F_FINALIZED)) { return nop; }
		return wbf().write(m_fd, m_t);
	}
};
}
}

#endif
