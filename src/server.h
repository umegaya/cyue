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
#include "session.h"
#include "fabric.h"
#include "rpc.h"

namespace yue {
class server : public loop {
	typedef yue::handler::session session;
	typedef yue::handler::listener stream_listener;
	typedef yue::handler::session datagram_listener;
	typedef yue::handler::accept_handler accept_handler;
	typedef yue::util::handshake::handler connect_handler;
	typedef queue<fabric::task, TASK_EXPAND_UNIT_SIZE> fabric_taskqueue;
	struct config {
		int max_object;
		int max_fiber;
		int worker_count;
		double timeout_check_sec;
		int fiber_timeout_us;
		int configure(const char *k, const char *v) {
			CONFIG_START()
				CONFIG_INT(max_object, k, v)
				CONFIG_INT(max_fiber, k, v)
				CONFIG_INT(worker_count, k, v)
				CONFIG_INT(fiber_timeout_us, k, v)
			CONFIG_END()
			return NBR_OK;
		}
	};
	struct session_pool {
		/* session kind */
		enum {
			NONE = session::DEFAULT_KIND,
			SERV,	/* for server_session */
			POOL,	/* for client_session */
			MESH,	/* for client_session */
		};
		class client_session : public session {
		public:
			client_session() : session () {}
		};
		class server_session : public session {
		public:
			server_session() : session() { set_kind(SERV); }
		};
		/* connected session */
		map<client_session, const char*> m_mesh;
		array<client_session> m_pool;
		/* accepted session */
#if !defined(__OLD_SERVER_SESSION)
		map<server_session, char[32]> m_as;
#else
		server_session *m_as;
#endif
		int m_maxfd;
		map<server_session *, UUID> m_sm;
		struct listener {
			U8 is_stream, is_error, padd[2];
			union {
				stream_listener *m_stream;
				datagram_listener *m_datagram;
			};
			listener() : is_error(0) {}
			~listener() {
				if (is_stream) {
					if (m_stream) { delete m_stream; m_stream = NULL; }
				}
				else {
					if (m_datagram) { delete m_datagram; m_datagram = NULL; }
				}
			}
			inline bool error() const { return is_error; }
			inline DSCRPTR fd() const {
				return is_stream ?
					(m_stream ? m_stream->fd() : INVALID_FD) :
					(m_datagram ? m_datagram->fd() : INVALID_FD);
			}
			inline DSCRPTR open(const char *addr, accept_handler &ah, object *opt) {
				net::address a; int r;
				transport *t = loop::pk().divide_addr_and_transport(addr, a);
				if (!parking::valid(t)) { return NBR_EINVAL; }
				is_stream = parking::stream(t);
				if (is_stream) {
					if (!(m_stream = new stream_listener)) { return NBR_EMALLOC; }
					m_stream->configure(addr, ah, opt);
					if ((r = loop::open(*m_stream)) < 0) { is_error = 1; return r; }
				}
				else {
					if (!(m_datagram = new datagram_listener)) { return NBR_EMALLOC; }
					m_datagram->listen(addr, opt);
					if ((r = loop::open(*m_stream)) < 0) { is_error = 1; return r; }
				}
				return r;
			}
		};
		map<listener, const char *> m_lctx;
	public:
		session_pool() : m_mesh(), m_pool(), 
#if !defined(__OLD_SERVER_SESSION)
			m_as(),
#else
			m_as(NULL), 
#endif
		m_maxfd(0), m_sm() {}
		int init(int maxfd) {
			m_maxfd = maxfd;
			if (!m_mesh.init(maxfd, maxfd, -1, opt_threadsafe | opt_expandable)) {
				return NBR_EMALLOC;
			}
			if (!m_pool.init(maxfd, -1, opt_threadsafe | opt_expandable)) {
				return NBR_EMALLOC;
			}
			if (!m_sm.init(maxfd, maxfd, -1, opt_threadsafe | opt_expandable)) {
				return NBR_EMALLOC;
			}
			if (!m_lctx.init(8, 8, -1, opt_threadsafe | opt_expandable)) {
				return NBR_EMALLOC;
			}
#if !defined(__OLD_SERVER_SESSION)
			if (!m_as.init(maxfd, maxfd, -1, opt_threadsafe | opt_expandable)) {
				return NBR_EMALLOC;
			}
#else
			if (!(m_as = new server_session[maxfd])) { return NBR_EMALLOC; }
#endif
			yue::handler::monitor::watcher wh(*this);
			if (yue::handler::monitor::add_static_watcher(wh) < 0) { return NBR_EEXPIRE; }
			return NBR_OK;
		}
		void fin() {
			m_mesh.fin();
			m_pool.fin();
			m_sm.fin();
			m_lctx.fin();
#if !defined(__OLD_SERVER_SESSION)
			m_as.fin();
#else
			if (m_as) {
				delete []m_as;
				m_as = NULL;
			}
#endif
			m_maxfd = 0;
		}
		map<listener, const char *> &lctx() { return m_lctx; }
	public:
		int operator () (DSCRPTR fd, DSCRPTR afd, net::address &a, handler::base **ch) {
			TRACE("accept: %d (by %d)\n", fd, afd);
			session *s;
#if !defined(__OLD_SERVER_SESSION)
			char uri[32]; bool exist;
			if (!net::address::to_uri(uri, sizeof(uri), a, loop::tl()[afd])) {
				ASSERT(false); return NBR_ESYSCALL;
			}
			TRACE("uri from : %s\n", uri);
			s = m_as.alloc(uri, &exist);
			if (!s || exist) { return NBR_EEXPIRE; }
#else
			s = (m_as + fd);
#endif
			*ch = s;
			return s->accept(fd, afd, a);
		}
		bool operator () (session *s, int st) {
			if (st == session::CLOSED) {
				char b[256];
				switch(s->kind()) {
#if !defined(__OLD_SERVER_SESSION)
				case SERV:  {
					char uri[32];
					if (!s->uri(uri, sizeof(uri))) {
						ASSERT(false); break;
					}
					m_as.erase(uri);
				} break;
#else
				case SERV: break;
#endif
				case POOL: m_pool.free(static_cast<client_session *>(s));
					break;
				case MESH: m_mesh.erase(s->addr(b, sizeof(b))); break;
				}
			}
			return yue::handler::monitor::KEEP;
		}
	public:
		inline session *served_for(net::address &a, DSCRPTR fd) {
#if !defined(__OLD_SERVER_SESSION)
			char uri[32];
			if (!net::address::to_uri(uri, sizeof(uri), a, loop::tl()[fd])) {
				ASSERT(false); return NULL;
			}
			return m_as.find(uri);
#else
			ASSERT(fd < m_maxfd && (m_as[fd].fd() == INVALID_FD || m_as[fd].fd() == fd));
			return m_as[fd].valid() ? &(m_as[fd]) : NULL;
#endif
		}
		inline session *served_for(const char *a) {
#if !defined(__OLD_SERVER_SESSION)
			return m_as.find(a);
#else
			return NULL;
#endif
		}
		session *add_to_mesh(const char *addr, object *opt, bool raw = false) {
			session *s = m_mesh.alloc(addr);
			if (!s) { return NULL; }
			s->set_kind(MESH);
			s->setopt(opt);
			s->setraw(raw ? 1 : 0);
			if (s->setaddr(addr) < 0) {
				m_mesh.erase(addr);
				return NULL;
			}
			return s;
		}
		session *open(const char *addr, object *opt, bool raw = false) {
			session *s = m_pool.alloc();
			if (!s) { return NULL; }
			s->set_kind(POOL);
			s->setopt(opt);
			s->setraw(raw ? 1 : 0);
			if (s->setaddr(addr) < 0) {
				m_pool.free(static_cast<client_session *>(s));
				return NULL;
			}
			return s;
		}
	};
	static yue::handler::accept_handler m_ah;
	static session_pool m_sp;	/* server connections */
	static const char *m_bootstrap;
	static config m_cfg;
	static server **m_sl, **m_slp;
	static int m_thn;
	static int m_argc; static char **m_argv;
	fabric m_fabric;
	fabric_taskqueue m_fque;
public:
	server() : loop() {}
	~server() {}
	static inline int configure(int thn, int argc, char *argv[]) {
		if (!argv) { return thn; }
		if (argc < 2) { return NBR_EINVAL; }
		m_bootstrap = argv[1];
		if (argc >= 3) {
			util::str::atoi(argv[2], thn);
			TRACE("thread num => %d\n", thn);
		}
		return thn;
	}
	static int static_init(class app &a, int thn, int argc, char *argv[]) {
		int r;
		m_ah.set(m_sp);
		m_argc = argc;
		m_argv = argv;
		if ((r = loop::static_init(a, thn, argc, argv)) < 0) { return r; }
		/* read command line configuration */
		if ((m_thn = configure(thn, argc, argv)) < 0) { return m_thn; }
		if (!(m_slp = (m_sl = new server*[m_thn]))) { return NBR_EMALLOC; }
		/* configure fabric system */
		fabric::configure(m_cfg.max_fiber,
			m_cfg.max_object, m_cfg.fiber_timeout_us);
		/* initialize net engine */
		if ((r = session::init(loop::maxfd())) < 0) { return r; }
		if ((r = m_sp.init(loop::maxfd())) < 0) { return r; }
		if ((r = fabric::init()) < 0) { return r; }
		/* enable fiber timeout checker */
		util::functional<int (timer_handle)> h(fabric::check_timeout);
		if (!set_timer(0.0f, m_cfg.timeout_check_sec, h) < 0) { return NBR_EEXPIRE; }
		return m_thn;
	}
	static void static_fin() {
		fabric::fin();
		m_sp.fin();
		session::fin();
		if (m_sl) {
			delete []m_sl;
			m_slp = m_sl = NULL;
		}
		loop::static_fin();
	}
	inline int init(class app &a) {
		int r;
		server **ppsv = __sync_fetch_and_add(&m_slp, sizeof(server*));
		*ppsv = this;
		if ((r = loop::init(a)) < 0) { return r; }
		if ((r = m_fabric.tls_init(this)) < 0) { return r; }
		return m_fque.init();
	}
	inline void fin() {
		m_fque.fin();
		loop::fin();
		m_fabric.tls_fin();
	}
	inline void poll() {
		fabric::task t;
		while (m_fque.pop(t)) { t(*this); }
		loop::poll();
	}
	void run(class app &a);
	static inline config &cfg() { return m_cfg; }
	static inline session_pool &spool() { return m_sp; }
	static inline const char *bootstrap_source() { return m_bootstrap; }
	static inline server *tlsv() { return reinterpret_cast<server *>(loop::tls()); }
	static inline int argc() { return m_argc; }
	static inline char **argv() { return m_argv; }
	inline fabric &fbr() { return m_fabric; }
	inline fabric_taskqueue &fque() { return m_fque; }
public:
	static inline int curse() { return util::syscall::daemonize(); }
	static inline int fork(char *cmd, char *argv[], char *envp[] = NULL) {
		return util::syscall::forkexec(cmd, argv, envp);
	}
	static inline session *served_for(net::address &a, DSCRPTR fd) { return m_sp.served_for(a, fd); }
	static inline session *served_for(const char *a) { return m_sp.served_for(a); }
	static inline server *get_thread(int idx) { return (idx < m_thn) ? m_sl[idx] : NULL; }
	static inline int thread_count() { return m_thn; }
	static inline int get_thread_idx(server *p) {
		for (server **s = m_sl; s < m_slp; s++) {
			TRACE("%p %p %p\n", s, m_sl, m_slp);
			if ((*s) == p) { return (s - m_sl); }
		}
		ASSERT(false);
		return -1;
	}
	static inline DSCRPTR get_listener_from(const char *addr) {
		session_pool::listener *l = m_sp.lctx().find(addr);
		return l ? l->fd() : INVALID_FD;
	}
public:
	static inline int listen(const char *addr, object *opt = NULL) {
		return listen(addr, m_ah, opt);
	}
	static inline int listen(const char *addr, accept_handler &ah, object *opt = NULL) {
		bool exist; session_pool::listener *l = m_sp.lctx().alloc(addr, &exist);
		if (!l) { return NBR_EEXPIRE; }
		if (!exist) {
			if (l->open(addr, ah, opt) < 0) { return NBR_ESYSCALL; }
			return l->fd();
		}
		while (!l->error() && l->fd() == INVALID_FD) {
			util::time::sleep(10 * 1000 * 1000/* 10ms */);
		}
		return l->error() ? NBR_ESYSCALL : l->fd();
	}
	static inline int signal(int signo, functional<void (int)> &sh) {
		return handler::signalfd::hook(signo, sh);
	}
	static inline timer_handle set_timer(
		double start, double intval, functional<int (timer_handle)> &sh) {
		return loop::timer().add_timer(sh, start, intval);
	}
	static inline void stop_timer(timer_handle t) {
		loop::timer().remove_timer_reserve(t);
	}
};
}
/* write poller functions */
#include "wpoller.hpp"
/* server related fabric/fiber inline functions */
#include "fiber.hpp"
/* session read handler implementation */
#include "datagram.hpp"
#include "raw.hpp"
#include "stream.hpp"
#if defined(NON_VIRTUAL)
/* handler implementation */
#include "handler.hpp"
#endif
#endif
