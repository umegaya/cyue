/***************************************************************
 * server.h : main loop of worker thread
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SERVER_H__)
#define __SERVER_H__

#include "loop.h"
#include "listener.h"
#include "timerfd.h"
#include "signalfd.h"
#include "fs.h"
#include "fabric.h"
#if defined(__OLD_FIBER__)
#include "rpc.h"
#endif

namespace yue {
struct config {
	int max_object;
	int max_fiber;
	int timeout_check_intv_us;
	int fiber_timeout_us;
	int configure(const char *k, const char *v) {
		CONFIG_START()
			CONFIG_INT(max_object, k, v)
			CONFIG_INT(max_fiber, k, v)
			CONFIG_INT(timeout_check_intv_us, k, v)
			CONFIG_INT(fiber_timeout_us, k, v)
		CONFIG_END()
		return NBR_OK;
	}
};
class server : public loop {
public:
	enum {
		SOCKET		= handler::base::SOCKET,
		LISTENER	= handler::base::LISTENER,
		FSWATCHER	= handler::base::FSWATCHER,
		FILESYSTEM  = handler::base::FILESYSTEM,
		BASE 		= handler::base::HANDLER_TYPE_MAX,
		TIMER,
		SIGNAL,
		THREAD,
	};
	class base : public emittable {
	public:
		base() : emittable(BASE) {}
	};
	class listener {
		emittable::wrap m_w;
		bool m_error;
	public:
		listener() : m_w(NULL), m_error(false) {}
		~listener() {}
		inline bool fd() { return m_w.unwrap<handler::base>()->fd(); }
		inline bool error() { return m_error; }
		inline void set(emittable *p) { m_w.set(p); }
		inline emittable *get() { return m_w.unwrap<emittable>(); }
		inline void set_error(bool on) { m_error = on; }
	};
	class timer : public emittable {
		loop::timer_handle m_t;
	public:
		inline timer() : emittable(TIMER), m_t(NULL) {}
		inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
		inline U32 tick() { return m_t->tick(); }
		inline void close() { loop::timer().remove_timer_reserve(m_t); }
		inline int open(double start_sec, double intval_sec) {
			return ((m_t = loop::timer().add_timer(*this, start_sec, intval_sec))) ? NBR_OK : NBR_EEXPIRE;
		}
		int operator () (loop::timer_handle t) {
			event::timer ev(this);
			emittable::emit(event::ID_TIMER, ev);
			return NBR_OK;
		}
	};
	class peer {
		emittable::wrap m_w;
		net::address m_addr;
	public:
		peer(handler::socket *s, const net::address &addr) : m_w(s), m_addr(addr) {}
		~peer() {}
		const net::address addr() const { return m_addr; }
		handler::socket *s() { return m_w.unwrap<handler::socket>(); }
	};
	class sig : public emittable {
		static void nop(int) { return; }
		volatile int m_signo;
	public:
		inline sig() : emittable(SIGNAL), m_signo(0) {}
		inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
		inline sig *set_signo(int sno) {
			if (__sync_bool_compare_and_swap(&m_signo,0,sno)) {
				handler::signalfd::hook(m_signo, *this);
			}
			return this;
		}
		inline void close() {
			if (m_signo == 0) { return; }
			if (__sync_bool_compare_and_swap(&m_signo, m_signo, 0)) {
				handler::signalfd::hook(m_signo, nop);
			}
		}
		void operator() (int signo) {
			event::signal ev(signo, this);
			emittable::emit(event::ID_SIGNAL, ev);
		}
	};
	class thread : public emittable {
		volatile server *m_server;
		volatile U8 m_alive, padd;
		U16 m_timeout_sec;
		char *m_name, *m_code;
	public:
		inline thread() : emittable(THREAD), m_server(NULL),
			m_alive(1), m_timeout_sec(1), m_name(NULL), m_code(NULL) {}
		inline ~thread() {
			if (m_name) { util::mem::free(m_name); }
			if (m_code) { util::mem::free(m_code); }
		}
		inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
		inline bool alive() const { return m_alive; }
		inline const char *code() const { return m_code; }
		inline const char *name() const { return m_name; }
	public:
		inline void set(const char *name, const char *code, int timeout_sec = 1) {
			m_name = util::str::dup(name);
			m_code = util::str::dup(code);
			ASSERT(timeout_sec > 0 && timeout_sec <= 0xFFFF);
			m_timeout_sec = timeout_sec;
		}
		inline void kill() { m_alive = 0; }
		inline void set_server(server *sv) { m_server = sv; }
		inline server *svr() { ASSERT(m_server); return (server *)m_server; }
		void *operator () ();
		volatile server *start();
	};
private: /* emittable object memory pool */
	static util::array<handler::listener> m_stream_listener_pool;
	static util::map<listener, const char *> m_listener_pool;
	static util::array<handler::socket> m_socket_pool;
	static util::map<handler::socket, const char*> m_cached_socket_pool;
	static util::array<timer> m_timer_pool;
	static sig m_signal_pool[handler::signalfd::SIGMAX];
	static util::map<thread, const char *> m_thread_pool;
	static util::array<peer> m_peer_pool;
		/* initialize, finalize */
	static int init_emitters(
		int max_listener, int max_socket, int max_timer, int max_thread) {
		int r, flags = util::opt_expandable | util::opt_threadsafe;
		if (!m_stream_listener_pool.init(max_listener, -1, flags)) { return NBR_EMALLOC; }
		if (!m_listener_pool.init(max_listener, max_listener, -1, flags)) { return NBR_EMALLOC; }
		if (!m_socket_pool.init(max_socket, -1, flags)) { return NBR_EMALLOC; }
		if (!m_cached_socket_pool.init(max_socket, max_socket, -1, flags)) { return NBR_EMALLOC; }
		if (!m_timer_pool.init(max_timer, -1, flags)) { return NBR_EMALLOC; }
		if (!m_thread_pool.init(max_thread, max_thread, -1, flags)) { return NBR_EMALLOC; }
		if (!m_peer_pool.init(max_socket, -1, flags)) { return NBR_EMALLOC; }
		if ((r = handler::socket::static_init(loop::maxfd())) < 0) { return r; }
		return NBR_OK;
	}
	template <class V> static inline int sweeper(V *v, util::array<V> &) {
		TRACE("sweep %p\n", v);
		v->clear_commands_and_watchers();
		return NBR_OK;
	}
	template <class V, class K> static inline int sweeper(V *v, util::map<V, K> &) {
		TRACE("sweep %p\n", v);
		v->clear_commands_and_watchers();
		return NBR_OK;
	}
	template <class V> static void cleanup(util::array<V> &a) {
		a.iterate(server::template sweeper<V>, a);
		a.fin();
	}
	template <class V, class K> static void cleanup(util::map<V, K> &m) {
		m.iterate(server::template sweeper<V, K>, m);
		m.fin();
	}
	static void fin_emitters() {
		emittable::start_finalize();
		m_peer_pool.fin();
		m_listener_pool.fin();
		cleanup<handler::socket>(m_socket_pool);
		cleanup<handler::socket, const char *>(m_cached_socket_pool);
		cleanup<timer>(m_timer_pool);
		cleanup<handler::listener>(m_stream_listener_pool);
		for (int i = 0; i < (int)countof(m_signal_pool); i++) {
			m_signal_pool[i].clear_commands_and_watchers();
		}
		handler::socket::static_fin();
		cleanup<thread, const char *>(m_thread_pool);
	}
		/* memory finalizer */
	static void finalize(emittable *p) {
		switch (p->type()) {
		case SOCKET: {
			handler::socket *s = reinterpret_cast<handler::socket *>(p);
			if (s->has_flag(handler::socket::F_CACHED)) {
				char b[256];
				m_cached_socket_pool.erase(s->addr().get(b, sizeof(b)));
			}
			else {
				m_socket_pool.free(reinterpret_cast<handler::socket *>(p));
			}
		} return;
		case LISTENER:
			m_stream_listener_pool.free(reinterpret_cast<handler::listener *>(p));
			return;
		case BASE:
			delete reinterpret_cast<base *>(p); return;
		case TIMER:
			m_timer_pool.free(reinterpret_cast<timer *>(p)); return;
		case SIGNAL:
			return;
		case FILESYSTEM:
			return;
		case FSWATCHER:
			loop::filesystem().pool().free(reinterpret_cast<handler::fs::watcher *>(p)); 
			return;
		case THREAD:	//TODO: is there anything todo ?
			m_thread_pool.erase(reinterpret_cast<thread *>(p)->name());
			return;
		default:
			switch (p->type()) {
			case handler::base::WPOLLER:
			case handler::base::TIMER:
			case handler::base::SIGNAL:
				return;
			default:
				ASSERT(false);
				return;
			}
			break;
		}	
	}
public:
	typedef handler::accept_handler accept_handler;
	typedef util::queue<fabric::task, TASK_EXPAND_UNIT_SIZE> fabric_taskqueue;
	typedef struct {
		thread *m_thread;
	} launch_args;
private:
	static config m_cfg;
	static ll m_config_ll;
	fabric m_fabric;
	fabric_taskqueue m_fque;
	thread *m_thread;
public:
	server() : loop(), m_thread(NULL) {}
	~server() {}
	static inline int configure(const util::app &a) {
		int r = m_config_ll.init(a);
		if (r < 0) { return r; }
		if ((r = m_config_ll.eval(ll::bootstrap())) < 0) {
			return r;
		}
		return server::thread_count();
	}
	static int static_init(util::app &a) {
		int r;
		if ((r = loop::static_init(a)) < 0) { return r; }
		if ((r = emittable::static_init(loop::maxfd(), loop::maxfd(),
			sizeof(fabric::task), finalize)) < 0) { return r; }
		/* initialize emitter memory */
		if ((r = init_emitters(32,
			loop::maxfd(),
			loop::timer().max_task(),
			util::hash::DEFAULT_HASHMAP_CONCURRENCY)) < 0) {
			return r;
		}
		/* initialize fabric engine */
		if ((r = fabric::static_init(m_cfg)) < 0) { return r; }
		/* read command line configuration */
		return configure(a);
	}
	static void static_fin() {
		m_config_ll.fin();
		fabric::static_fin();
		loop::fin_handlers();
		fin_emitters();
		ASSERT(fiber::watcher_pool().use() <= 0);
		fiber::watcher_pool().fin();
		loop::static_fin();
	}
	inline int init(launch_args &a) {
		int r;
		m_thread = a.m_thread;
		if ((r = loop::init(loop::app())) < 0) { return r; }
		if ((r = m_fque.init()) < 0) { return r; }
		if ((r = m_fabric.init(loop::app(), this)) < 0) { return r; }
		return m_fabric.execute(m_thread->code());
	}
	inline void fin() {
		m_fque.fin();
		loop::fin();
		m_fabric.fin();
		if (m_thread) {
			event::thread ev(m_thread);
			m_thread->immediate_emit(event::ID_THREAD, reinterpret_cast<void *>(&ev));
			m_thread = NULL;
		}
	}
	inline void poll() {
		fabric::task t;
		while (m_fque.pop(t)) { TRACE("fabric::task processed %u\n", t.type()); t(*this); }
		loop::poll();
	}
	void run(launch_args &a);
	template <class SR, class OBJ> int send(SR &sr, OBJ &obj);
	static inline config &cfg() { return m_cfg; }
	static inline server *tlsv() { return reinterpret_cast<server *>(loop::tls()); }
	inline fabric &fbr() { return m_fabric; }
	inline fabric_taskqueue &fque() { return m_fque; }
	inline thread *thrd() { return m_thread; }
public:
	static inline int curse() { return util::syscall::daemonize(); }
	static inline int fork(char *cmd, char *argv[], char *envp[] = NULL) {
		return util::syscall::forkexec(cmd, argv, envp);
	}
	static inline thread *get_thread(const char *name) { return m_thread_pool.find(name); }
	static inline int thread_count() { return m_thread_pool.use(); }
public: /* open, close */
	static inline int open(emittable *p) {
		switch (p->type()) {
		case SOCKET:
			return reinterpret_cast<handler::socket *>(p)->open();
		case LISTENER:
			return reinterpret_cast<handler::listener *>(p)->open();
		case BASE:
		case TIMER:
		case SIGNAL:
		case FSWATCHER:
		case THREAD:
			return NBR_OK;
		default:
			ASSERT(false); return NBR_OK;
		}
	}
	static inline void close(emittable *p) {
		switch (p->type()) {
		case SOCKET:
			reinterpret_cast<handler::socket *>(p)->close(); return;
		case LISTENER://listener cannot close on runtime
			ASSERT(false); return;
		case BASE:
			return;
		case TIMER:
			reinterpret_cast<timer *>(p)->close(); return;
		case SIGNAL:
			reinterpret_cast<sig *>(p)->close(); return;
		case FSWATCHER:
			reinterpret_cast<handler::fs::watcher *>(p)->close(); return;
		case THREAD:	//TODO: how stop this thread?
			reinterpret_cast<thread *>(p)->kill(); return;
		default:
			ASSERT(false); return;
		}
	}
public: /* create emittable */
	static inline emittable *emitter() {
		emittable *p = new base();
		TRACE("new emitter %p\n", p);
		return p;
	}
public:	/* create thread */
	static inline emittable *launch(const char *name, const char *code_or_file, int timeout_sec) {
		thread *w = m_thread_pool.alloc(name);
		if (!w) { return NULL; }
		w->set(name, code_or_file, timeout_sec);
		if (!w->start()) {
			m_thread_pool.erase(name);
			return NULL;
		}
		return w;
	}
public:	/* create peer */
	static inline peer *open_peer(handler::socket *s, const net::address &a) {
		return m_peer_pool.alloc(s, a);
	}
	static inline void close_peer(peer *p) {
		m_peer_pool.free(p);
	}
public: /* create filesystem */
	static inline emittable *fs_watch(const char *path, const char *events) {
		handler::fs::event_flags flags = handler::fs::event_flags_from(events);
		return loop::filesystem().watch(path, flags);
	}
public: /* create timer */
	static inline emittable *set_timer(double start_sec, double intval_sec) {
		timer *t = m_timer_pool.alloc();
		if (!t) { return NULL; }
		if (t->open(start_sec, intval_sec) < 0) {
			m_timer_pool.free(t);
			return NULL;
		}
		return t;
	}
public: /* create signal */
	static emittable *signal(int signo) {
		if (signo < 0 || signo >= handler::signalfd::SIGMAX) { ASSERT(false); return NULL; }
		return m_signal_pool[signo].set_signo(signo);
	}
public: /* crate socket */
	static emittable *open(const char *addr, object *opt = NULL) {
		handler::socket *s;
		bool cached = false;
		if (!opt || !((*opt)("no_cache", false))) {
			bool exist;
			cached = true;
			if (!(s = m_cached_socket_pool.alloc(addr, &exist))) { goto error; }
			if (exist) {
				if (opt) { opt->fin(); }
				while (
					(!s->has_flag(handler::socket::F_INITIALIZED)) &&
					(!s->has_flag(handler::socket::F_ERROR))
				){
					util::time::sleep(1 * 1000 * 1000/* 1ms */);
				}
				return s->has_flag(handler::socket::F_INITIALIZED) ? s : NULL;
			}
		}
		else {
			if (!(s = m_socket_pool.alloc())) { return NULL; }
		}
		s->configure(addr, opt);
		s->set_flag(handler::socket::F_CACHED, cached);
		return s;
	error:
		if (s) {
			if (cached) { 
				util::time::sleep(100 * 1000 * 1000); /* 100ms */
				m_cached_socket_pool.erase(addr); 
			}
			else { m_socket_pool.free(s); }
		}
		return NULL;
	}
public:	/* create listener */
	int operator () (DSCRPTR fd, handler::listener *l, net::address &raddr, handler::base **ppb) {
		handler::socket *s = m_socket_pool.alloc();
		if (!s) { return NBR_EMALLOC; }
		s->configure(fd, l, raddr);
		/* if ((r = s->open_server_conn(fd, l, raddr, l->config().timeout)) < 0) {
			m_socket_pool.free(s); 
			return r; 
		} */
		*ppb = s;
		return NBR_OK;
	}
	emittable *listen(const char *addr, object *opt = NULL) {
		net::address a; transport *t;
		handler::listener *l; handler::socket *s;
		bool exist; listener *c = m_listener_pool.alloc(addr, &exist);
		if (!c) { goto error; }
		if (!exist) {
			t = loop::pk().divide_addr_and_transport(addr, a);
			if (!parking::valid(t)) { goto error; }
			if (parking::stream(t)) {
				if (!(l = m_stream_listener_pool.alloc())) { goto error; }
				if (!l->configure(addr, *this, opt)) { goto error; }
				//if ((r = loop::open(*l)) < 0) { goto error; }
				c->set(l);
			}
			else {
				if (!(s = m_socket_pool.alloc())) { goto error; }
				if (!s->configure(addr, opt, s)) { goto error; }
				//if ((r = s->open_datagram_server(addr, *opt)) < 0) { goto error; }
				c->set(s);
			}
			return c->get();
		}
		else if (opt) { opt->fin(); }
		while (!c->error() && c->fd() == INVALID_FD) {
			util::time::sleep(10 * 1000 * 1000/* 10ms */);
		}
		return c->error() ? NULL : c->get();
	error:
		if (c) { c->set_error(true); }
		/* wait another thread finish this function
		 * TODO: more safe way to propagate error to another thread (pthread_cond?) */
		util::time::sleep(100 * 1000 * 1000/* 100ms */);
		m_listener_pool.erase(addr);	//handler::socket or listener freed from emittable->unref
		return NULL;
	}
};
}
/* write poller functions */
#include "wpoller.hpp"

/* server related fabric/fiber inline functions */
#include "fiber.hpp"

/* handler implementation */
#if defined(NON_VIRTUAL)
#include "handler.hpp"
#endif

/* emittable implementation */
#include "emittable.hpp"

/* socket implement */
#include "socket.hpp"

/* fs implement */
#include "fs.hpp"

/* pluggable modules implementation */
#include "impl.hpp"
#endif
