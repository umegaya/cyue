/***************************************************************
 * eio.h : event IO loop
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
#if !defined(__EIO_H__)
#define __EIO_H__

#include "thread.h"
#include "queue.h"
#include "functional.h"
#include "sbuf.h"
#include "parking.h"
#include "wbuf.h"
#include "syscall.h"
#include "address.h"
#include "signalfd.h"
#include "timerfd.h"
#include "hresult.h"
#include "actor.h"

namespace yue {
namespace module {
namespace net {
namespace eio {

#define __ENABLE_RECONNECTION__

#if defined(__NOUSE_PROTOCOL_PLUGIN__)
	#define GET_TRANSPORT(_fd) NULL
#else
	#define GET_TRANSPORT(_fd) basic_processor::from(_fd)
#endif

#define EIO_TRACE(...)
#define WP_TRACE(...)

class loop {
	FORBID_COPY(loop);
public:
	typedef selector::method poller;
public:
	struct thread_args {
		loop *m_e; int m_idx, m_standalone;
		thread_args() {}
		inline void set(loop *e, int idx, int sa = 0) {
			m_e = e; m_idx = idx;
			m_standalone = sa;
		}
	};
	template <class PROCESSOR>
	class em {
		FORBID_COPY(em);
	public:
		/* constraint for class PROCESSOR */
		class processor : public PROCESSOR {
		public:
			typedef typename PROCESSOR::task proc_task;
			class task : public proc_task {
			public:
				void operator () (em<PROCESSOR> &em, poller &p) {
					proc_task::operator () (em, p);
				}
			};
		public:
			processor() : PROCESSOR() {}
			typedef typename PROCESSOR::handler proc_handler;
			class handler : public proc_handler {
			public:
				inline int operator () (em<PROCESSOR> &em, poller::event &e) {
					return proc_handler::operator () (em, e, p);
				}
			};
			static int init(loop &e, poller &p, int maxfd) { 
				return PROCESSOR::init(e, p, maxfd);
			}
			static void fin() { PROCESSOR::fin(); }
			int tls_init(loop &e, poller &p, int maxfd, local_actor &la) {
				return PROCESSOR::tls_init(e, p, maxfd, la);
			}
			void tls_fin() { PROCESSOR::tls_fin(); }
			void process(em<PROCESSOR> &em, poller::event &e) {
				return PROCESSOR::process(em, e);
			}
			int attach(DSCRPTR fd, int type, 
				const proc_handler &h, 
				transport *p = NULL, void *param = NULL) {
				return PROCESSOR::attach(fd, type, h, p, param);
			}
		};
		typedef typename PROCESSOR::task task;
		static const U32 TASK_EXPAND_UNIT_SIZE = 4096;
		typedef queue<task, TASK_EXPAND_UNIT_SIZE> taskqueue;
	protected:
		processor m_proc;
		taskqueue m_queue;
		poller &m_p;
		int m_maxfd, m_timeout;
		void *m_tls_p;
	public:
		em(poller &p, int maxfd, int timeout) : m_proc(), m_queue(), m_p(p),
			m_maxfd(maxfd), m_timeout(timeout), m_tls_p(NULL) {}
		~em() { tls_fin(); }
		inline taskqueue &que() { return m_queue; }
		inline processor &proc() { return m_proc; }
		inline poller &p() { return m_p; }
		inline int maxfd() const { return m_maxfd; }
		inline int timeout() const { return m_timeout; }
	public:
		static em<PROCESSOR> *tlem() {
			return thread::current_tls<em<PROCESSOR> >(); }
		void set_tls_p(void *p) { m_tls_p = p; }
		void *tls_p() { return m_tls_p; }
	public:
		static inline int init(loop &e, poller &p, int maxfd) {
			return processor::init(e, p, maxfd); }
		inline int tls_init(loop &e) {
			if (que().init() < 0) { return NBR_EMALLOC; }
			local_actor la(this);
			return proc().tls_init(e, p(), maxfd(), la);
		}
		static inline void fin() { processor::fin(); }
		inline void tls_fin() { proc().tls_fin(); }
		inline void poll() {
			int n_ev;
			task t;
			while (que().pop(t)) { t(*this, p()); }
			poller::event occur[maxfd()];
			if ((n_ev = p().wait(occur, maxfd(), timeout())) < 0) {
				if (p().error_again()) { return; }
				TRACE("poller::wait: %d", m_p.error_no());
				return;
			}
			for (int i = 0; i < n_ev; i++) {
				proc().process(*this, occur[i]);
			}
		}
		static inline em<PROCESSOR> *start(thread_args &ta) {
			loop &e = *(ta.m_e);
			poller &p = e.poller_at(ta.m_idx);
			poller_group &pg = e.pg_at(ta.m_idx);
			em<PROCESSOR> *pem = new em<PROCESSOR>(p, e.maxfd(), e.timeout());
			if (!pem) { return NULL; }
			if (!ta.m_standalone) {
				thread::init_tls(pem);
			}
			pg.push_em(pem);
			if (pem->tls_init(e) < 0) { return NULL; }
			return pem;
		}
		static inline void stop(em<PROCESSOR> *pem, thread_args &ta) {
			delete pem;
			if (!ta.m_standalone) {
				thread::try_cancel();
			}
		}
		static void *run(void *ptr) {
			em<PROCESSOR> *pem;
			thread_args &ta = *reinterpret_cast<thread_args *>(ptr);
			if (!(pem  = start(ta))) { return NULL; }
			while (ta.m_e->alive()) { pem->poll(); }
			stop(pem, ta);
			return NULL;
		}
		static inline int again(DSCRPTR) { return handler_result::again; }
		static inline int again_rw(DSCRPTR) { return handler_result::again_rw; }
		static inline int destroy(DSCRPTR) { return handler_result::destroy; }
		static inline int keep(DSCRPTR) { return handler_result::keep; }
	};
public:
	class basic_processor {
		FORBID_COPY(basic_processor);
	public:
		typedef functional<int (em<basic_processor>&, poller::event &)> handler;
		struct task {
			unsigned char type, padd[3];
			union {
				poller::event m_ev;
				DSCRPTR m_fd;
			};
			enum {
				WRITE_AGAIN,
				READ_AGAIN,
				CLOSE,
				TYPE_MAX,
			};
			task() {}
			task(DSCRPTR fd) : type(CLOSE), m_fd(fd) {}
			task(poller::event &ev, U8 t) : type(t), m_ev(ev) {}
			template <class PROCESSOR> void operator () (em<PROCESSOR> &em, poller &p) {
				switch(type) {
				case WRITE_AGAIN:em.proc().wp().write(em, m_ev); break;
				case READ_AGAIN:em.proc().process(em, m_ev); break;
				case CLOSE:em.proc().close(m_fd); break;
				}
			}
		};
		class write_poller : public poller {
			FORBID_COPY(write_poller);
			struct wfd {
				wbuf *m_wp;
				wfd() : m_wp(NULL) {}
			} *m_wb;
		public:
			write_poller() : m_wb(NULL) {}
			~write_poller() { fin(); }
			int init(int maxfd) {
				writer::init(this);
				if (!(m_wb = new wfd[maxfd])) { return NBR_EMALLOC; }
				return poller::open(maxfd);
			}
			void fin() { 
				poller::close();
				if (m_wb) { 
					delete []m_wb; 
					m_wb = NULL;
				}
			}
			template <class PROCESSOR>
			int operator() (em<PROCESSOR> &em, poller::event &e) {
				poller::event occur[em.maxfd()]; int n_ev;
				if ((n_ev = wait(occur, em.maxfd(), em.timeout())) < 0) {
					if (error_again()) { return em.again(fd()); }
					WP_TRACE("poller::wait: %d", error_no());
					return em.destroy(fd());
				}
				for (int i = 0; i < n_ev; i++) {
					write(em, occur[i]);
				}
				/* all event fully read? */
				return n_ev < em.maxfd() ? em.again(fd()): em.keep(fd());
			}
			template <class PROCESSOR>
			void write(em<PROCESSOR> &em, poller::event &e) {
				int r; DSCRPTR fd = poller::from(e);
				WP_TRACE("write: fd = %d\n", fd);
				wbuf *wbf = get_wbuf(fd);
				if (!wbf) {
					WP_TRACE("write: wbuf detached %d\n", fd);
					return;
				}
				switch((r = wbf->write(fd, GET_TRANSPORT(fd)))) {
				case handler_result::keep: {
					WP_TRACE("write: %d: process again\n", fd);
					typename PROCESSOR::task t(e, task::WRITE_AGAIN);
					em.que().mpush(t);
				} break;
				case handler_result::again: {
					WP_TRACE("write: %d: back to poller\n", fd);
					poller::attach(fd, poller::EV_WRITE);
				} break;
				case handler_result::nop: {
					WP_TRACE("write: %d: wait next retach\n", fd);
				} break;
				case handler_result::destroy:
				default: {
					ASSERT(r == handler_result::destroy);
					WP_TRACE("write: %d: close %d\n", fd, r);
					typename PROCESSOR::task t(fd);
					em.que().mpush(t); 
				} break;
				}
			}
			inline void reset_wbuf(DSCRPTR fd, wbuf *wbf) {
				ASSERT(m_wb[fd].m_wp == wbf);
				if (m_wb[fd].m_wp == wbf) {
					m_wb[fd].m_wp = NULL;
				}
			}
			inline void set_wbuf(DSCRPTR fd, wbuf *wbf) {
				ASSERT(!m_wb[fd].m_wp || m_wb[fd].m_wp == wbf);
				m_wb[fd].m_wp = wbf;
			}
			inline int init_wbuf(DSCRPTR fd, wbuf *wbf) {
				TRACE("init_wbuf: %d %p\n", fd, wbf);
				set_wbuf(fd, wbf);
				return wbf ? poller::retach(fd, poller::EV_WRITE) :
					poller::attach(fd, poller::EV_WRITE);
			}
			inline void detach(DSCRPTR fd) { poller::detach(fd); }
			inline wbuf *get_wbuf(DSCRPTR fd) {
				return m_wb[fd].m_wp;
			}
			inline writer get(DSCRPTR fd) { 
				return writer::create(get_wbuf(fd), fd);
			}
		};
	protected:
		static transport **m_transport;
		static write_poller m_wp;
		static poller *m_rp;
		static signalfd m_sig;
		static timerfd m_timer;
		static loop *m_e;
		basic_processor() {}
	public:
		~basic_processor() {}
		static inline poller &rp() { ASSERT(m_rp); return *m_rp; }
		static inline write_poller &wp() { return m_wp; }
		static inline signalfd &sig() { return m_sig; }
		static inline timerfd &timer() { return m_timer; }
		static int init(loop &e, poller &p, int maxfd) {
			int r;
			m_rp = &p;
			m_e = &e;
			if ((r = m_wp.init(maxfd)) < 0) { return r; }
			if ((r = m_timer.init()) < 0) { return r; }
			if ((r = m_sig.init()) < 0) { return r; }
			if ((r = m_sig.hook(SIGINT, signalfd::handler(process_signal))) < 0) {
				return r;
			}
#if !defined(__ENABLE_TIMER_FD__)
			if ((r = m_sig.hook(SIGALRM, signalfd::handler(m_timer))) < 0) {
				return r;
			}
#endif
			if ((r = m_sig.ignore(SIGPIPE))) { return r; }
			if ((r = m_sig.ignore(SIGHUP))) { return r; }
			if (!(m_transport = new transport*[maxfd])) { return NBR_EMALLOC; }
			util::mem::fill(m_transport, 0, sizeof(transport *) * maxfd);
			return NBR_OK;
		}
		inline int tls_init(loop &, poller &, int, local_actor &) {
			return NBR_OK;
		}
		static inline void fin() {
			m_wp.fin();
			m_sig.fin();
			m_timer.fin();
			if (m_transport) {
				delete []m_transport;
				m_transport = NULL;
			}
			m_rp = NULL;
			m_e = NULL;
		}
		inline void tls_fin() {}
		static void process_signal(int sig) {
			switch (sig) {
			case SIGINT: m_e->die(); break;
			default: ASSERT(false); break;
			}
		}
		static inline transport *from(DSCRPTR fd) { return m_transport[fd]; }
		struct fd_type {
			enum {
				CONNECTION,
				LISTENER,
				SIGNAL,
				TIMER,
				POLLER,
				SERVERCONN,
				DGLISTENER,
				DGCONN,
				RAWSOCKET,
			};
		};
		static inline int attach(DSCRPTR fd, int type, transport *p = NULL) {
			if (util::syscall::fcntl_set_nonblock(fd) != 0) {
				ASSERT(false);
				return NBR_ESYSCALL;
			}
			m_transport[fd] = p;
			switch(type) {
			case fd_type::CONNECTION:
			case fd_type::DGCONN:
			case fd_type::RAWSOCKET:
				if (wp().init_wbuf(fd, NULL) < 0) { return NBR_EMALLOC; }
				/* EV_WRITE for knowing connection establishment */
				return rp().attach(fd, poller::EV_READ | poller::EV_WRITE);
			case fd_type::SERVERCONN:
				if (wp().init_wbuf(fd, NULL) < 0) { return NBR_EMALLOC; }
				return rp().attach(fd, poller::EV_READ | poller::EV_WRITE);
			case fd_type::DGLISTENER:
				if (wp().attach(fd, poller::EV_WRITE) < 0) { return NBR_ESYSCALL; }
			default:
				break;
			}
			return rp().attach(fd, poller::EV_READ);
		}
		inline void process(em<basic_processor> &em, poller::event &e) {
			DSCRPTR fd = poller::from(e);
			TRACE("read: fd = %d\n", fd);
			ASSERT(false);
		}
		static inline void close(DSCRPTR fd) {
			TRACE("basic_processor::close %d\n", fd);
			rp().detach(fd);
			wp().detach(fd);
			yue::util::syscall::shutdown(fd);
			syscall::close(fd, GET_TRANSPORT(fd));
			/* GET_TRANSPORT will be overwritten at next attach */
			/* after that, value of fd may reused by kernel,
			 * so we should not do anything based on fd value. */
		}
		static inline int handshake(DSCRPTR fd, poller::event &e) {
			return syscall::handshake(fd,
				poller::readable(e), poller::writable(e),
				GET_TRANSPORT(fd));
		}
	};
	template <class DISPATCHER>
	class processor : public basic_processor {
		DISPATCHER m_dp;
	public:
		typedef basic_processor super;
		typedef typename DISPATCHER::handler handler;
		typedef typename DISPATCHER::task task;
		processor() : super() {}
		~processor() {}
		DISPATCHER &dispatcher() { return m_dp; }
		static int init(loop &e, poller &p, int maxfd) {
			int r;
			if ((r = super::init(e, p, maxfd)) < 0) { return r; }
			if ((r = DISPATCHER::init(maxfd)) < 0) { return r; }
			return NBR_OK;
		}
		int tls_init(loop &e, poller &p, int maxfd, local_actor &la) {
			return m_dp.tls_init(e, p, maxfd, la);
		}
		static void fin() {
			DISPATCHER::fin();
			super::fin();
		}
		void tls_fin() {
			m_dp.tls_fin();
		}
		static inline int attach(DSCRPTR fd, int type,
			const handler &h, transport *p = NULL, void *param = NULL) {
			if (!DISPATCHER::attach(fd, type, h, param)) { return NBR_EINVAL; }
			return super::attach(fd, type, p);
		}
		inline void process(em<processor<DISPATCHER> > &em, poller::event &e) {
			int r; DSCRPTR fd = poller::from(e);
			EIO_TRACE("read: fd = %d\n", fd);
			switch((r = m_dp.read(fd, em, e))) {
			case handler_result::keep: {
				EIO_TRACE("read: %d: process again\n", fd);
				task t(e, task::READ_AGAIN);
				em.que().mpush(t);
			} break;
			case handler_result::again: {
				EIO_TRACE("read: %d: back to poller\n", fd);
				r = rp().retach(fd, poller::EV_READ);
				ASSERT(r >= 0);
			} break;
			case handler_result::again_rw: {
				EIO_TRACE("read: %d: back to poller\n", fd);
				rp().retach(fd, poller::EV_READ | poller::EV_WRITE);
			} break;
			case handler_result::nop: {
				EIO_TRACE("read: %d: wait next retach\n", fd);
			} break;
			case handler_result::destroy:
			default: {
				ASSERT(r == handler_result::destroy);
				TRACE("read: %d: close %d\n", fd, r);
				task t(fd);
				em.que().mpush(t);
			} break;
			}
		}
		static inline void close(DSCRPTR fd) {
			DISPATCHER::close(fd);
			super::close(fd);
		}
	};
	class generic_dispatcher {
	public:
		typedef processor<generic_dispatcher> proc;
		typedef basic_processor::task task;
		typedef basic_processor::fd_type fd_type;
		typedef functional<int (em<proc>&, poller::event &)> handler;
	protected:
		static handler *m_list;
	public:
		static int init(int maxfd) {
			int r;
			if (!(m_list = new handler[maxfd])) { return NBR_EMALLOC; }
			handler hw(proc::wp()), hs(proc::sig()), ht(proc::timer());
			if ((r = proc::attach(proc::wp().fd(), fd_type::POLLER, hw, NULL)) < 0) {
				return r;
			}
			if ((r = proc::attach(proc::sig().fd(), fd_type::SIGNAL, hs, NULL)) < 0) {
				return r;
			}
#if defined(__ENABLE_TIMER_FD__)
			if ((r = proc::attach(proc::timer().fd(), fd_type::TIMER, hs, NULL)) < 0) {
				return r;
			}
#endif
			return NBR_OK;
		}
		static inline void fin() {
			if (m_list) {
				delete []m_list;
				m_list = NULL;
			}
		}
		int tls_init(loop &, poller &, int, local_actor &la) { return NBR_OK; }
		void tls_fin() {};
		inline handler &operator [] (DSCRPTR fd) {return m_list[fd];}
		static inline bool attach(DSCRPTR fd, int type, const handler &h, void *p) {
			m_list[fd] = h;
			return true;
		}
		inline int read(DSCRPTR fd, 
			em<processor<generic_dispatcher> > &em, poller::event &e) {
			TRACE("read: fd = %d\n", fd);
			return m_list[fd](em, e);
		}
		static inline void close(DSCRPTR) {}
	};
	class sync_poller : public poller {
	public:
		sync_poller() : poller() {}
		inline int wait_event(DSCRPTR wfd, U32 event, int timeout, poller::event &ev) {
			int r = NBR_OK;
			if (fd() == INVALID_FD) {
				if ((r = open(1)) < 0) { ASSERT(false); goto end; }
			}
			if ((r = attach(wfd, event)) < 0) {
				TRACE("errno = %d\n", error_no());ASSERT(false);
				goto end;
			}
		retry:
			if ((r = wait(&ev, 1, timeout)) < 0) {
				if (error_again()) { goto retry; }
				TRACE("errno = %d\n", error_no());ASSERT(false);
				goto end;
			}
			ASSERT(loop::poller::from(ev) == wfd);
		end:
			detach(wfd);
			return r;
		}
	};
protected:
	struct poller_group {
		poller m_p;
		thread_pool m_thp;
		local_actor *m_aem;
		volatile int m_nem, m_emmax;
	public:
		poller_group() : m_aem(NULL) {}
		~poller_group() { free_em(); }
		void free_em() { if (m_aem) { delete []m_aem; m_aem = NULL; } }
		void push_em(void *em) {
			if (m_nem < m_emmax) {
				m_aem[__sync_fetch_and_add(&m_nem, 1)].set(em);
			}
		}
		bool alloc_em(int num) {
			free_em();
			m_emmax = num; m_nem = 0;
			return (m_aem = new local_actor[num]);
		}
		local_actor *em(int idx) { ASSERT(idx < m_nem); return (m_aem + idx); }
	};
	poller_group *m_pg;
	int m_pgn, m_maxfd, m_timeout;
	bool m_alive;
	parking m_parking;
	static sync_poller m_sync;
public:
	loop(int maxfd = -1, int timeout = 50) :
		m_pg(NULL), m_pgn(0),  m_maxfd(maxfd), m_timeout(timeout),
		m_alive(true) {}
	~loop() { fin(); }
	inline bool alive() const { return m_alive; }
	inline bool die() { return __sync_val_compare_and_swap(&m_alive, true, false); }
	inline int maxfd() const { return m_maxfd; }
	inline int timeout() const { return m_timeout; }
	inline int pgn() const { return m_pgn; }
	inline poller &poller_at(int idx) { 
		ASSERT(idx < pgn()); return m_pg[idx].m_p; }
	inline thread_pool &pool_at(int idx) { 
		ASSERT(idx < pgn()); return m_pg[idx].m_thp; }
	inline poller_group &pg_at(int idx) {
		ASSERT(idx < pgn()); return m_pg[idx]; }
protected:
	void fin() {
		if (m_pg) {
			join();
			for (int i = 0; i < m_pgn; i++) {
				m_pg[i].m_thp.fin();
				m_pg[i].m_p.close();
			}
			delete []m_pg;
			m_pg = NULL;
			m_pgn = 0;
		}
		m_parking.fin();
		nbr_fin();	/* TODO: remove nbr_*** */
	}
	int join() {
		for (int i = 0; i < m_pgn; i++) {
			if (m_pg[i].m_thp.started()) {
				m_pg[i].m_thp.join();
			}
		}
		return NBR_OK;
	}
	int open(int n_pg) {
		if (m_maxfd < 0) {
			util::syscall::rlimit rl;
			if(util::syscall::getrlimit(RLIMIT_NOFILE, &rl) < 0) {
				return NBR_ESYSCALL;
			}
			m_maxfd = rl.rlim_cur;
		}
		if (n_pg <= 0) { return NBR_EINVAL; }
		m_pgn = n_pg;
		if (!(m_pg = new poller_group[m_pgn])) { return NBR_EMALLOC; }
		for (int i = 0; i < m_pgn; i++) {
			if (m_pg[i].m_p.open(m_maxfd) < 0) { return NBR_ESYSCALL; }
		}
		return NBR_OK;
	}
	int init(int size, int (*ini[])(loop &, poller &, int)) {
		int r;
		/* TODO: remove nbr_*** */
		if ((r = nbr_init(NULL)) < 0) { return r; }
		if ((r = m_parking.init()) < 0) { return r; }
		/* default: use TCP */
		if ((r = m_parking.add("tcp", NULL/* default */)) < 0) { return r; }
		if ((r = m_parking.add("udp", udp_transport())) < 0) { return r; }
		if ((r = m_parking.add("mcast", mcast_transport())) < 0) { return r; }
		if ((r = m_parking.add("popen", popen_transport())) < 0) { return r; }
		if ((r = open(size)) < 0) { return NBR_ESYSCALL; }
		for (int i = 0; i < size; i++) {
			if ((r = ini[i](*this, m_pg[i].m_p, m_maxfd)) < 0) { return r; }
		}
		return NBR_OK;
	}
	int run(void *(*worker[])(void *), void (*fzr[])(), int num[], int n_pg) {
		int r, i;
		if (n_pg != m_pgn) { return NBR_EINVAL; }
		thread_args args[m_pgn];
		if (n_pg == 1 && num[0] == 1) {
			/* hey! you don't need thread pool if only one process? */
			m_pg[0].alloc_em(num[0]);
			args[0].set(this, 0, 1);
			worker[0](&(args[0]));
			fzr[0]();
			return NBR_OK;
		}
		for (i = 0; i < m_pgn; i++) {
			m_pg[i].alloc_em(num[i]);
			args[i].set(this, i);
			if ((r = m_pg[i].m_thp.start(num[i], worker[i], &args[i])) < 0) {
				return r;
			}
		}
		r = join();
		for (i = 0; i < m_pgn; i++) { fzr[i](); }
		return r;
	}
public:
	int init() {
		int (*ini[])(loop &, poller &, int) = { 
			em<processor<generic_dispatcher> >::init };
		return init(1, ini);
	}
	int run(int num) {
		void *(*r[])(void *) = {
			em<processor<generic_dispatcher> >::run };
		void (*f[])() = {
			em<processor<generic_dispatcher> >::fin };
		return run(r, f, &num, 1);
	}
	transport *from(const char *ctx) {
		return m_parking.find_ptr(ctx);
	}
	inline local_actor *from(int pg_idx, int th_idx) {
		ASSERT(pg_idx < m_pgn);
		return m_pg[pg_idx].em(th_idx);
	}
	template <class PROCESSOR> int init_with() {
		int (*ini[])(loop &, poller &, int) = { em<PROCESSOR>::init };
		return init(1, ini);
	}
	template <class PROCESSOR> int run_with(int num) {
		void *(*r[])(void *) = { em<PROCESSOR>::run };
		void (*f[])() = { em<PROCESSOR>::fin };
		return run(r, f, &num, 1);
	}
	template <class PROCESSOR> inline void *start_with() {
		thread_args ta;
		ta.set(this, 0, 1);
		return reinterpret_cast<void *>(em<PROCESSOR>::start(ta));
	}
	template <class PROCESSOR> inline void stop_with(void *pem) {
		thread_args ta;
		ta.set(this, 0, 1);
		em<PROCESSOR>::stop(reinterpret_cast<em<PROCESSOR> *>(pem),ta);
	}
	template <class PROCESSOR> static inline void poll_with(void *pem) {
		reinterpret_cast<em<PROCESSOR> *>(pem)->poll();
	}
	static inline sync_poller &synchronizer() { return m_sync; }
public:
	transport *divide_addr_and_transport(const char *addr, address &a);
	transport *divide_addr_and_transport(const char *addr, char *out, int len);
};

}
}
}
}

#endif
