/***************************************************************
 * net.h : event driven network IO (java NIO, libev, ...)
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
#if !defined(__NET_H__)
#define __NET_H__

#include "impl.h"

namespace yue {
/* specialization for each module */
template <class LOOP> struct loop_traits;
typedef util::functional<int (DSCRPTR, int)> connect_handler;
typedef util::functional<int (DSCRPTR, connect_handler&)> accept_handler;
}
/* customize by traits */
#if _NET == eio
#include "eio/traits.h"
#else
#error no valid module
#endif

namespace yue {
/* basic typedef (specialized) */
typedef module::net::_NET::loop loop;
typedef loop_traits<loop>::address address;
/* represents worker thread on same node */
typedef loop_traits<loop>::local_actor local_actor;
/* represents yue process on remote node */
typedef loop_traits<loop>::remote_actor remote_actor;
/* represents message stream which transfered by TCP/UDP */
typedef loop_traits<loop>::session session;
typedef loop_traits<loop>::timer timer;
enum {
	S_ESTABLISH = loop_traits<loop>::S_ESTABLISH,
	S_SVESTABLISH = loop_traits<loop>::S_SVESTABLISH,
	S_EST_FAIL = loop_traits<loop>::S_EST_FAIL,
	S_SVEST_FAIL = loop_traits<loop>::S_SVEST_FAIL,
	S_CLOSE = loop_traits<loop>::S_CLOSE,
	S_SVCLOSE = loop_traits<loop>::S_SVCLOSE,
};
class net : public loop {
public:
	inline int maxfd() { return loop_traits<loop>::maxfd(*this); }
	inline int init() {
		return loop_traits<loop>::init(*this);
	}
	inline int start() {
		return loop_traits<loop>::start(*this);
	}
	inline void fin() {
		return loop_traits<loop>::fin(*this);
	}
	inline void die() {
		loop_traits<loop>::die(*this);
	}
	inline void poll() {
		loop_traits<loop>::poll(*this);
	}
	inline int run(int num) {
		return loop_traits<loop>::run(*this, num);
	}
	inline int listen(const char *addr, accept_handler &ah) {
		return loop_traits<loop>::listen(*this, addr, ah);
	}
	inline int connect(const char *addr, connect_handler &ch, double timeout, 
		object *opt) {
		return loop_traits<loop>::connect(*this, addr, ch, timeout, opt);
	}
	inline int signal(int signo, functional<void (int)> &sh) {
		return loop_traits<loop>::signal(*this, signo, sh);
	}
	inline timer set_timer(double start, double intval,
		functional<int (U64)> &sh) {
		return loop_traits<loop>::set_timer(*this, start, intval, sh);
	}
	inline void stop_timer(timer t) {
		return loop_traits<loop>::stop_timer(*this, t);
	}
	inline local_actor *get_thread(int idx) {
		return loop_traits<loop>::get_thread(*this, idx);
	}
	static inline void set_tls(void *tls) {
		return loop_traits<loop>::set_tls(tls);
	}
	static inline void *tls() {
		return loop_traits<loop>::tls();
	}
};
}

#endif
