/***************************************************************
 * traits.h : specialization of class net when using eio.
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
#if !defined(__NET_TRAITS_H__)
#define __NET_TRAITS_H__

#include "transport.h"
#include "address.h"
#include "actor.h"
#include "wbuf.h"
#include "address.h"
#include "session.h"

namespace yue {
typedef module::net::eio::loop loop;
template <> struct loop_traits<loop> {
public:
	typedef module::net::eio::address address;
	typedef module::net::eio::timerfd::task *timer;
	typedef module::net::eio::loop::basic_processor::fd_type fd_type;
	typedef module::net::eio::remote_actor remote_actor;
	typedef module::net::eio::local_actor local_actor;
	typedef module::net::eio::session session;
protected:
	static int m_worker_num;
	static void *m_tls;
	static local_actor *m_la;
	static void *m_em_handle;
public:
	static int worker_num() { return m_worker_num; }
	static void *tls_p() { return m_tls; }
	enum {
		S_ESTABLISH = module::net::eio::S_ESTABLISH,
		S_SVESTABLISH = module::net::eio::S_SVESTABLISH,
		S_EST_FAIL = module::net::eio::S_EST_FAIL,
		S_SVEST_FAIL = module::net::eio::S_SVEST_FAIL,
		S_CLOSE = module::net::eio::S_CLOSE,
		S_SVCLOSE = module::net::eio::S_SVCLOSE,
	};
public:
	static int maxfd(loop &l);
	static int init(loop &l);
	static void fin(loop &l);
	static void die(loop &l);
	static int run(loop &l, int num);
	static int listen(loop &l, const char *addr, accept_handler &ah);
	static inline int connect(loop &l, const char *addr, 
		connect_handler &ch, double t_o);
	static local_actor *get_thread(loop &l, int idx);
	static int signal(loop &l, int signo, functional<void (int)> &sh);
	static timer set_timer(loop &l, double start, double intval, 
		functional<int (U64)> &sh);
	static void stop_timer(loop &l, timer t);
	static void set_tls(void *tls);
	static void *tls();
	/* for using server inside lua program */
	static int start(loop &l);
	static void stop(loop &l);
	static void poll(loop &l);
};

inline int loop_traits<loop>::connect(loop &l, const char *addr,
	connect_handler &ch, double t_o)  {
	address to;
	transport *t = l.divide_addr_and_transport(addr, to);
	return session::connect(to, t, ch, t_o);
}

template <class SR, class O>
int module::net::eio::local_actor::send(SR &sr, O &o) {
	int r;
	if ((r = o.pack_as_object(sr)) < 0) { return r; }
	return feed(sr.result());
}

}

#endif
