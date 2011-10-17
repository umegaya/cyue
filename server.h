/***************************************************************
 * server.h : yue server instance.
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
#if !defined(__SERVER_H__)
#define __SERVER_H__

#include "rpc.h"
#include "macro.h"

namespace yue {
class server : public net {
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
		}
	};
	struct session_pool {
		/* session kind */
		enum {
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
		struct listen_context {
			int dummy;
		};
		/* connected session */
		map<client_session, const char*> m_mesh;
		array<client_session> m_pool;
		/* accepted session */
		server_session *m_as;
		int m_maxfd;
		map<server_session *, UUID> m_sm;
		map<listen_context, const char *> m_lctx;
	public:
		session_pool() : m_mesh(), m_pool(), m_as(NULL), m_maxfd(0), m_sm() {}
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
			if (!(m_as = new server_session[maxfd])) { return NBR_EMALLOC; }
			session::watcher wh(*this);
			if (session::add_static_watcher(wh) < 0) { return NBR_EEXPIRE; }
			return NBR_OK;
		}
		void fin() {
			m_mesh.fin();
			m_pool.fin();
			m_sm.fin();
			m_lctx.fin();
			if (m_as) {
				delete []m_as;
				m_as = NULL;
			}
			m_maxfd = 0;
		}
		map<listen_context, const char *> &lctx() { return m_lctx; }
	public:
		int operator () (DSCRPTR fd, connect_handler &ch) {
			TRACE("accept: %d\n", fd);
			ch = connect_handler(m_as[fd]);
			return m_as[fd].accept(fd);
		}
		bool operator () (session *s, int st) {
			if (st == session::CLOSED) {
				char b[256];
				switch(s->kind()) {
				case SERV: break;
				case POOL: m_pool.free(static_cast<client_session *>(s)); 
					break;
				case MESH: m_mesh.erase(s->addr(b, sizeof(b))); break;
				}
			}
			return session::KEEP_WATCH;
		}
	public:
		inline session *served_for(DSCRPTR fd) {
			ASSERT(fd < m_maxfd && (m_as[fd].fd() == INVALID_FD || m_as[fd].fd() == fd));
			return m_as[fd].valid() ? &(m_as[fd]) : NULL;
		}
		session *add_to_mesh(const char *addr) {
			session *s = m_mesh.alloc(addr);
			if (!s) { return NULL; }
			s->set_kind(MESH);
			if (s->setaddr(addr) < 0) {
				m_mesh.erase(addr);
				return NULL;
			}
			return s;
		}
		session *open(const char *addr) {
			session *s = m_pool.alloc();
			if (!s) { return NULL; }
			s->set_kind(POOL);
			if (s->setaddr(addr) < 0) {
				m_pool.free(static_cast<client_session *>(s));
				return NULL;
			}
			return s;
		}
	};
	accept_handler m_ah;
	session_pool m_sp;	/* server connections */
	const char *m_bootstrap;
	config m_cfg;
public:
	server() : net(), m_ah(m_sp), m_sp(), m_bootstrap(NULL) {}
	~server() { fin(); }
	int init(const char *bootstrap = NULL, config *c = NULL) {
		int r;
		config dc = { 1000000, 100000, 2, 1.0f, 50000000 };/* default */
		if (!c) { c = &dc; }
		m_cfg = *c;
		m_bootstrap = bootstrap;
		/* configure fabric system */
		fabric::configure(this, c->max_fiber, c->max_object, c->fiber_timeout_us);
		/* initialize net engine */
		if ((r = net::init()) < 0) { return r; }
		if ((r = m_sp.init(net::maxfd())) < 0) { return r; }
		/* enable fiber timeout checker */
		util::functional<int (U64)> h(fabric::check_timeout);
		return set_timer(0.0f, c->timeout_check_sec, h) ? NBR_OK : NBR_EMALLOC;
	}
	void fin() {
		m_sp.fin();
		net::fin();
	}
	inline config &cfg() { return m_cfg; }
	inline session_pool &spool() { return m_sp; }
	inline const char *bootstrap_source() { return m_bootstrap; }
public:
	inline int run(int n = -1) { return net::run(n < 0 ? cfg().worker_count : n); }
	inline int curse() { return util::syscall::daemonize(); }
	inline int fork(char *cmd, char *argv[], char *envp[] = NULL) {
		return util::syscall::fork(cmd, argv, envp); }
	inline int listen(const char *addr) { return listen(addr, m_ah); }
	inline int listen(const char *addr, accept_handler &ah) {
		session_pool::listen_context c, *pc = m_sp.lctx().insert(c, addr);
		return pc ? NBR_OK : net::listen(addr, ah);  }
	inline session *served_for(DSCRPTR fd) { return m_sp.served_for(fd); }
	inline local_actor *get_thread(int idx) { return net::get_thread(idx); }
};
}

#endif
