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
		SOCKET			= constant::emittable::SOCKET,
		LISTENER		= constant::emittable::LISTENER,
		FSWATCHER		= constant::emittable::FSWATCHER,
		FILESYSTEM  	= constant::emittable::FILESYSTEM,
		TIMER			= constant::emittable::TIMER,
		BASE 			= constant::emittable::BASE,
		SIGNALHANDLER	= constant::emittable::SIGNALHANDLER,
		THREAD			= constant::emittable::THREAD,
		TASK			= constant::emittable::TASK,
	};
	static const int MAX_LISTENER_HINT = 32;
	static const int MAX_TIMER_HINT = 16;
	static const int MAX_TASK_HINT = 10000;
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
	class task : public emittable {
		loop::timer_handle m_t;
		handler::timerfd::taskgrp *m_tg;
	public:
		inline task(handler::timerfd::taskgrp *tg) : emittable(TASK), m_t(NULL), m_tg(tg) {}
		inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
		inline int open(double start_sec, double intval_sec) {
			return m_tg->add_timer(*this, start_sec, intval_sec, &m_t) ? NBR_OK : NBR_ESHORT;
		}
		inline void close() {
			m_tg->remove_timer_reserve(m_t);
			emittable::remove_all_watcher();
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
		peer() {}
		peer(handler::socket *s, const net::address &addr) : m_w(s), m_addr(addr) {}
		~peer() {}
		void set(handler::socket *s, const net::address &addr) { m_w.set(s), m_addr = addr; }
		const net::address &addr() const { return m_addr; }
		handler::socket *s() { return m_w.unwrap<handler::socket>(); }
	};
	class sig : public emittable {
		static void nop(int) { return; }
		volatile int m_signo;
	public:
		inline sig() : emittable(SIGNALHANDLER), m_signo(0) {}
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
				void (*tmp)(int) = nop;
				handler::signalfd::hook(m_signo, tmp);
				emittable::remove_all_watcher();
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
		char *m_name, *m_code, *m_pgname;
	public:
		inline thread() : emittable(THREAD), m_server(NULL),
			m_alive(1), m_timeout_sec(1), m_name(NULL), m_code(NULL), m_pgname(NULL) {}
		inline ~thread() {
			if (m_name) { util::mem::free(m_name); }
			if (m_code) { util::mem::free(m_code); }
			if (m_pgname) { util::mem::free(m_pgname); }
		}
		inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
		inline bool alive() const { return m_alive; }
		inline const char *code() const { return m_code; }
		inline const char *name() const { return m_name; }
		inline const char *pgname() const { return m_pgname; }
	public:
		inline void set(const char *name, const char *code, int timeout_sec = 1, const char *pgname = loop::NO_EVENT_LOOP) {
			m_name = util::str::dup(name);
			m_code = util::str::dup(code);
			if (pgname) { m_pgname = util::str::dup(pgname); }
			ASSERT(timeout_sec > 0 && timeout_sec <= 0xFFFF);
			m_timeout_sec = timeout_sec;
		}
		inline void kill() { m_alive = 0; }
		inline bool enable_event_loop() { return !m_pgname || (0 != util::str::cmp(m_pgname, loop::NO_EVENT_LOOP)); }
		inline void set_server(server *sv) { m_server = sv; }
		inline server *svr() { ASSERT(m_server); return (server *)m_server; }
		void *operator () ();
		int start();
		int wait();
	};
private: /* emittable object memory pool */
	static util::array<handler::listener> m_stream_listener_pool;
	static util::array<handler::socket> m_socket_pool;
	static util::array<handler::timerfd> m_timer_pool;
	static util::array<task> m_task_pool;
	static sig m_signal_pool[handler::signalfd::SIGMAX];
	static util::map<thread, const char *> m_thread_pool;
		/* initialize, finalize */
	static int init_emitters(
		int max_listener, int max_socket, int max_task, int max_timer, int max_thread) {
		int r, flags = util::opt_expandable | util::opt_threadsafe;
		if (!m_stream_listener_pool.init(max_listener, -1, flags)) { return NBR_EMALLOC; }
		if (!m_socket_pool.init(max_socket, -1, flags)) { return NBR_EMALLOC; }
		if (!m_timer_pool.init(max_timer, -1, flags)) { return NBR_EMALLOC; }
		if (!m_task_pool.init(max_task, -1, flags)) { return NBR_EMALLOC; }
		if (!m_thread_pool.init(max_thread, max_thread, -1, flags)) { return NBR_EMALLOC; }
		if ((r = handler::socket::static_init(loop::maxfd())) < 0) { return r; }
		return NBR_OK;
	}
	template <class V> static inline int sweeper(V *v, int &) {
		v->clear_commands_and_watchers();
		return NBR_OK;
	}
	template <class V> static void cleanup(util::array<V> &a) {
		int n;
		a.iterate(server::template sweeper<V>, n);
		a.fin();
	}
	template <class V, class K> static void cleanup(util::map<V, K> &m) {
		int n;
		m.iterate(server::template sweeper<V>, n);
		m.fin();
	}
	static void fin_emitters() {
		emittable::start_finalize();
		cleanup<handler::socket>(m_socket_pool);
		cleanup<handler::timerfd>(m_timer_pool);
		cleanup<task>(m_task_pool);
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
			TRACE("remove socket\n");
			handler::socket *s = reinterpret_cast<handler::socket *>(p);
			if (s->has_flag(handler::socket::F_CACHED)) {
				char b[256];
			TRACE("cached socket %s\n", s->resolved_uri(b, sizeof(b)));
				if (!tlsv()->resource()->socket_pool().erase(*s)) {
					ASSERT(false);
				}
			}
			else {
			TRACE("non cached socket\n");
				m_socket_pool.free(reinterpret_cast<handler::socket *>(p));
			}
		} return;
		case LISTENER:
			m_stream_listener_pool.free(reinterpret_cast<handler::listener *>(p));
			return;
		case BASE:
			delete reinterpret_cast<base *>(p); return;
		case TIMER: {
			handler::timerfd *tfd = reinterpret_cast<handler::timerfd *>(p);
			if (tfd != &(loop::timer())) {
				const char *name = tfd->name();
				if (name) {
					tlsv()->resource()->timer_pool().free(tfd);
				}
				else {
					m_timer_pool.free(tfd);
				}
			}
			return;
		}
		case SIGNALHANDLER:
			return;
		case FILESYSTEM:
			return;
		case FSWATCHER:
			loop::filesystem().pool().free(reinterpret_cast<handler::fs::watcher *>(p)); 
			return;
		case THREAD:	//TODO: is there anything todo ?
			m_thread_pool.erase(reinterpret_cast<thread *>(p)->name());
			return;
		case TASK:
			m_task_pool.free(reinterpret_cast<task *>(p));
			return;
		default:
			switch (p->type()) {
			case constant::emittable::WPOLLER:
			case constant::emittable::SIGNAL:
				return;
			default:
				ASSERT(false);
				break;
			}
			return;
		}	
	}
public:
	typedef handler::accept_handler accept_handler;
	typedef util::queue<fabric::task, TASK_EXPAND_UNIT_SIZE> fabric_taskqueue;
	typedef struct {
		thread *m_thread;
	} launch_args;
	struct poller_local_resource {
		util::map<handler::socket, const char*> m_cached_socket_pool;
		util::map<listener, const char *> m_listener_pool;
		util::map<peer, net::address> m_peer_pool;
		typedef util::pattern::shared_allocator<handler::timerfd, const char *> timer_allocator;
		timer_allocator m_named_timer_pool;
		int m_result;
	protected:
		poller_local_resource() : m_cached_socket_pool(), m_listener_pool(),
			m_peer_pool(), m_named_timer_pool(), m_result(0) {}
		int init() { return (m_result = init_pools()); }
		void fin() {
			m_result = 0;
			server::cleanup<handler::socket, const char *>(m_cached_socket_pool);
			server::cleanup<timer_allocator::element, const char *>(m_named_timer_pool.pool());
			m_listener_pool.fin();
			m_peer_pool.fin();
		}
		int check(int) { return m_result; }
		int init_pools() {
			int r, flags = util::opt_expandable | util::opt_threadsafe;
			if (!m_listener_pool.init(MAX_LISTENER_HINT, MAX_LISTENER_HINT, -1, flags)) { return NBR_EMALLOC; }
			if (!m_peer_pool.init(loop::maxfd(), loop::maxfd(), -1, flags)) { return NBR_EMALLOC; }
			if ((r = m_named_timer_pool.init(MAX_TIMER_HINT)) < 0) { return r; }
			return m_cached_socket_pool.init(loop::maxfd(), loop::maxfd(), -1, flags) ? NBR_SUCCESS : NBR_EMALLOC;
		}
	public:
		util::map<handler::socket, const char*> &socket_pool() { return m_cached_socket_pool; }
		util::map<listener, const char*> &listener_pool() { return m_listener_pool; }
		util::map<peer, net::address> &peer_pool() { return m_peer_pool; }
		util::pattern::shared_allocator<handler::timerfd, const char *> &timer_pool() { return m_named_timer_pool; }
	};
	typedef util::pattern::shared_allocator<poller_local_resource, const char *> poller_local_resource_pool;
private:
	static config m_cfg;
	static ll m_config_ll;
	static poller_local_resource_pool m_resource_pool;
	static poller_local_resource *m_main_resource;
	fabric m_fabric;
	fabric_taskqueue m_fque;
	thread *m_thread;
	poller_local_resource *m_resource;
public:
	server() : loop(), m_thread(NULL), m_resource(NULL) {}
	~server() {}
	static inline int configure(const util::app &a) {
		int r = m_config_ll.init(a, NULL);
		if (r < 0) { return r; }
		if ((r = m_config_ll.eval(ll::bootstrap())) < 0) {
			return r;
		}
		return server::thread_count();
	}
	static void output_logo(FILE *f, const char *ll_version) {
		fprintf(f, "__  ____ __ __    ____  \n");
		fprintf(f, "\\ \\ \\  // / \\ \\  / ___\\ \n");
		fprintf(f, " \\ \\/ / | | | | / /     \n");
		fprintf(f, "  \\  /  | | | | ~~~~~~~~    version %s(%s)\n", "0.3.5", ll_version);
		fprintf(f, " _/ /   \\ \\_/ / \\ \\___  \n");
		fprintf(f, " \\_/     \\___/   \\____/  \n");
		fprintf(f, "it's brilliant on the cloud\n\n");
		fprintf(f, "(c)2011 - 2012 Takehiro Iyatomi(iyatomi@gmail.com)\n");
	}
	static int static_init(util::app &a, bool is_server = true) {
		int r;
		if (is_server) {
			output_logo(stdout, ll::version());
		}
		if ((r = loop::static_init(a)) < 0) { return r; }
		if ((r = emittable::static_init(loop::maxfd(), loop::maxfd(),
			sizeof(fabric::task), finalize)) < 0) { return r; }
		/* initialize emitter memory */
		if ((r = init_emitters(MAX_LISTENER_HINT,
			loop::maxfd(),
			MAX_TASK_HINT,
			MAX_TIMER_HINT,
			util::hash::DEFAULT_HASHMAP_CONCURRENCY)) < 0) {
			return r;
		}
		if ((r = m_resource_pool.init(loop::DEFAULT_POLLER_MAP_SIZE_HINT)) < 0) {
			return r;
		}
		/* initialize fabric engine */
		if ((r = fabric::static_init(m_cfg)) < 0) { return r; }
		/* read command line configuration */
		return is_server ? configure(a) : NBR_OK;
	}
	static void static_fin() {
		m_config_ll.fin();
		fabric::static_fin();
		loop::fin_handlers();
		fin_emitters();
		m_resource_pool.fin();
		ASSERT(fiber::watcher_pool().use() <= 0);
		fiber::watcher_pool().fin();
		loop::static_fin();
	}
	inline int init(launch_args &a) {
		int r;
		m_thread = a.m_thread;
		TRACE("server init: %p %p\n", this, m_thread);
		loop::launch_args la = { &(loop::app()), m_thread->pgname() };
		if ((r = loop::init(la)) < 0) { return r; }
		if ((r = m_fque.init()) < 0) { return r; }
		if ((r = m_fabric.init(loop::app(), this)) < 0) { return r; }
		if (!(m_resource = m_resource_pool.alloc(m_thread->pgname()))) {
			return NBR_ESHORT;
		}
		return m_fabric.execute(m_thread->code());
	}
	inline void fin() {
		if (m_thread) {		//process all thread message
			event::thread ev(m_thread);
			m_thread->emit(event::ID_THREAD, ev);
			fabric::task t;
			while (m_fque.pop(t)) { TRACE("fabric::task processed2 %u\n", t.type()); t(*this); }
			m_thread->remove_all_watcher(true);
			m_thread = NULL;//unref thread emitter. it will be freed after all reference released.
		}
		m_fabric.fin_lang();//unref all emitter which referred in lang VM
		loop::fin();		//stop event IO
		m_fque.fin();
		if (m_resource) {	//poller local resource should be removed after poller & lang VM is removed.
							//because these are using memory which allocated for resource.
							//but during resource removal, it access to fiber (to remove them from emitter watcher list)
							//so only lang VM in fabric remove first,
			m_resource_pool.free(m_resource);
			m_resource = NULL;
		}
		m_fabric.fin();		//then finally memory for fibers are removed here
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
	inline poller_local_resource *resource() { return m_resource; }
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
		case LISTENER:
			return reinterpret_cast<handler::listener *>(p)->open();
		case TIMER:
			return loop::open(*reinterpret_cast<handler::timerfd *>(p));
		case SOCKET:
			return reinterpret_cast<handler::socket *>(p)->open();
		case BASE:
		case SIGNALHANDLER:
		case FSWATCHER:
		case THREAD:
		case TASK:
			return NBR_OK;
		default:
			ASSERT(false); return NBR_OK;
		}
	}
	static inline void close(emittable *p) {
		switch (p->type()) {
		case LISTENER://TODO: timer, listener cannot close on runtime
			ASSERT(false); return;
		case TIMER:
			reinterpret_cast<handler::timerfd *>(p)->close(); return;
		case SOCKET:
			reinterpret_cast<handler::socket *>(p)->close(); return;
		case BASE:
			p->remove_all_watcher(); return;
		case SIGNALHANDLER:
			reinterpret_cast<sig *>(p)->close(); return;
		case FSWATCHER:
			reinterpret_cast<handler::fs::watcher *>(p)->close(); return;
		case THREAD:	//TODO: how stop this thread?
			reinterpret_cast<thread *>(p)->kill(); return;
		case TASK:
			reinterpret_cast<task *>(p)->close(); return;
		default:
			ASSERT(false); return;
		}
	}
public: /* create emittable */
	static inline emittable *emitter() {
		emittable *p = new base();
		return p;
	}
public:	/* create thread */
	static inline emittable *launch(const char *name,
		const char *code_or_file, int timeout_sec, const char *pgname) {
		bool exist; thread *w = m_thread_pool.alloc(name, &exist);
		if (!w) { return NULL; }
		if (!exist) {
			w->set(name, code_or_file, timeout_sec, pgname);
			if (w->start() < 0) { goto error; }
		}
		if (w->wait() < 0) { goto error; }
		return w;
	error:
		m_thread_pool.erase(name);
		return NULL;
	}
public:	/* create peer */
	inline peer *open_peer(handler::socket *s, const net::address &a) {
		bool exists; peer *p = m_resource->peer_pool().alloc(a, &exists);
		if (!p) { return NULL; }
		if (!exists) { p->set(s, a); }
		return p;
	}
	inline void close_peer(peer *p) {
		m_resource->peer_pool().erase(p->addr());
	}
public: /* create filesystem */
	static inline emittable *fs_watch(const char *path, const char *events) {
		handler::fs::event_flags flags = handler::fs::event_flags_from(events);
		return loop::filesystem().watch(path, flags);
	}
public: /* create timer */
	/* create normal timer */
	struct timer_initializer {
		const char *name;
		double start_sec, intval_sec;
		int operator () (timerfd *tfd) {
			if (!tfd->set_name(name)) { return NBR_EMALLOC; }
			return tfd->init(start_sec, intval_sec);
		}
	};
	struct taskgrp_initializer {
		const char *name;
		int max_task, max_intv_sec, resolution_us;
		int operator () (timerfd *tfd) {
			if (!tfd->set_name(name)) { return NBR_EMALLOC; }
			return tfd->init_taskgrp(max_task, max_intv_sec, resolution_us);
		}
	};
	static int timer_initialize_checker(timerfd *tfd, int n_chk) {
		if (tfd->fd() != INVALID_FD) { return NBR_SUCCESS; }
		if (n_chk >= 5000) { return NBR_ETIMEOUT; }
		util::time::sleep(1 * 1000 * 1000);
		return NBR_OK;
	}
	template <class H>
	static inline bool create_sys_timer(H &h, double start_sec, double intval_sec) {
#if defined(__ENABLE_TIMER_FD__) || defined(USE_KQUEUE_TIMER)
		//kqueue timer is nat suitable for native timer because its start_sec ignored, but for some system timeout checker (open_now = true),
		//it is good to use kqueue timer because its independently invoked so timeout check (important!) interval be more stable.
		timerfd *t = m_timer_pool.alloc(h);
		if (!t) { return NULL; }
		if (t->init(start_sec, intval_sec) < 0) {
			m_timer_pool.free(t);
			return false;
		}
		//
		if (loop::open(*t, m_mainp) < 0) {
			m_timer_pool.free(t);
			return false;
		}
		return true;
#else
		return loop::timer().tg()->add_timer(h, start_sec, intval_sec);
#endif
	}
	template <class H>
	static inline bool create_timer(H &h, double start_sec, double intval_sec) {
#if defined(__ENABLE_TIMER_FD__)
		timerfd *t = m_timer_pool.alloc(h);
		if (!t) { return NULL; }
		if (t->init(start_sec, intval_sec) < 0) {
			m_timer_pool.free(t);
			return false;
		}
		return true;
#else
		return loop::timer().tg()->add_timer(h, start_sec, intval_sec);
#endif
	}
	static inline emittable *create_timer(double start_sec, double intval_sec) {
#if defined(__ENABLE_TIMER_FD__)
		timerfd *t = m_timer_pool.alloc();
		if (!t) { return NULL; }
		if (t->init(start_sec, intval_sec) < 0) {
			m_timer_pool.free(t);
			return NULL;
		}
		return t;
#else
		/* if timerfd not supported, named timer not supported. */
		return create_task(&(loop::timer()), start_sec, intval_sec);
#endif
	}
	inline emittable *create_timer(const char *name, double start_sec, double intval_sec) {
#if defined(__ENABLE_TIMER_FD__)
		timer_initializer izr = { name, start_sec, intval_sec };
		return m_resource->timer_pool().alloc(name, izr, timer_initialize_checker);
#else
		/* if timerfd not supported, named timer not supported. */
		return NULL;
#endif
	}
	/* create task group */
	inline emittable *create_taskgrp(const char *name, int max_task, int max_intv_sec, int resolution_us) {
#if defined(__ENABLE_TIMER_FD__)
		taskgrp_initializer izr = { name, max_task, max_intv_sec, resolution_us };
		return m_resource->timer_pool().alloc(name, izr, timer_initialize_checker);
#else
		return &(loop::timer());
#endif
	}
	inline emittable *find_timer(const char *name) {
#if defined(__ENABLE_TIMER_FD__)
		return m_resource->timer_pool().pool().find(name);
#else
		return &(loop::timer());
#endif
	}
	/* create task */
	static inline emittable *create_task(timerfd *tfd, double start_sec, double intval_sec) {
		handler::timerfd::taskgrp *tg = tfd->tg();
		if (!tg) { ASSERT(false); return NULL; }
		task *t = m_task_pool.alloc(tg);
		if (t->open(start_sec, intval_sec) < 0) {
			m_task_pool.free(t);
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
	emittable *open(const char *addr, object *opt = NULL) {
		handler::socket *s;
		bool cached = false;
		if (!opt || !((*opt)("no_cache", false))) {
			TRACE("allocate cached socket: %s\n", addr);
			bool exist;
			cached = true;
			if (!(s = m_resource->socket_pool().alloc(addr, &exist))) { goto error; }
			if (exist) {
				if (opt) { opt->fin(); }
				int cnt = 0;
				/* wait initialization or 5sec timeout */
				while ((++cnt < 5000) && !s->has_flag(handler::socket::F_CACHED)){
					util::time::sleep(1 * 1000 * 1000/* 1ms */);
				}
				return cnt >= 5000 ? NULL : s;
			}
		}
		else {
			TRACE("allocate non cached socket: %s\n", addr);
			if (!(s = m_socket_pool.alloc())) { return NULL; }
		}
		s->configure(addr, opt);
		s->set_flag(handler::socket::F_CACHED, cached);
		s->set_flag(handler::socket::F_INITIALIZED, true);
		return s;
	error:
		if (s) {
			if (cached) { 
				util::time::sleep(100 * 1000 * 1000); /* 100ms */
				m_resource->socket_pool().erase(addr);
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
		*ppb = s;
		return NBR_OK;
	}
	emittable *listen(const char *addr, object *opt = NULL) {
		net::address a; transport *t;
		handler::listener *l; handler::socket *s;
		bool exist; listener *c = m_resource->listener_pool().alloc(addr, &exist);
		if (!c) { goto error; }
		if (!exist) {
			t = loop::pk().divide_addr_and_transport(addr, a);
			if (!parking::valid(t)) { goto error; }
			if (parking::stream(t)) {
				if (!(l = m_stream_listener_pool.alloc())) { goto error; }
				if (l->configure(addr, *this, opt) < 0) { goto error; }
				c->set(l);
			}
			else {
				if (!(s = m_socket_pool.alloc())) { goto error; }
				if (s->configure(addr, opt, s) < 0) { goto error; }
				if (s->init_datagram_server() < 0) { goto error; }
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
		m_resource->listener_pool().erase(addr);	//handler::socket or listener freed from emittable->unref
		return NULL;
	}
};
}
/* write poller functions */
#include "wpoller.hpp"

/* timerfd functions */
#include "timerfd.hpp"

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
