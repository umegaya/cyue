/***************************************************************
 * rpc.h : yue-rpc command invokation system.
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
#if !defined(__RPC_H__)
#define __RPC_H__

#include "fabric.h"
#include "serializer.h"
#include "util.h"

namespace yue {
namespace rpc {
template <class R, class A>
class procedure : public fiber {
	struct rval : public R  {
		/* when invoked by other rpc */
		inline rval(object &o) : R(o) {}
		/* when start up by itself */
		inline rval(const type::nil &n) : R(n) {}
		inline int operator() (serializer &sr) const {
			return R::operator () (sr);
		}
	} m_rval;
public:
	struct args : public A {
		MSGID m_msgid;
		inline args() : A() {}
		inline args(const A &a) : A(a) {}
		/* for remote_actor::send (it requires method pack) */
		inline int pack(serializer &sr) const {
			args *a = const_cast<args *>(this);
			return sr.pack_request(a->m_msgid, A::cmd(), *this);
		}
		/* for local_actor::send (it requires method pack) */
		inline int pack_as_object(serializer &sr) const {
			return fabric::pack_as_object(*this, sr);
		}
		inline int operator() (serializer &sr, MSGID assigned) const {
			return A::operator() (sr, assigned);
		}
	};
	template <class CALLBACK>
	struct args_and_cb : public args {
		typedef typename util::mpg::template ref_traits<CALLBACK>::type cb_type;
		cb_type m_cb;
		U32 m_timeout;
		inline args_and_cb(cb_type c) : args(), m_cb(c), m_timeout(0) {}
		inline args_and_cb(cb_type c, const A &a) : args(a), m_cb(c) {}
		/* for remote_actor::send (it requires method pack) */
		inline int pack(serializer &sr) const {
			args_and_cb *a = const_cast<args_and_cb *>(this);
			int r = sr.pack_request(a->m_msgid, A::cmd(), *this);
			if (r >= 0) {
				/* now, packet is under packed, not sent. */
				/* should yield here because if once packet is sent, it is possible that
				 * reply is received before suspend is finished. */
				if (fabric::suspend<cb_type>(m_cb, args::m_msgid, m_timeout)
					== fiber::exec_error) {
					return NBR_ESHORT;
				}
			}
			return r;
		}
		/* for local_actor::send (it requires method pack) */
		inline int pack_as_object(serializer &sr) const {
			return fabric::pack_as_object(*this, sr);
		}
	};
public:
	template <class ACTOR>
	inline procedure(ACTOR &a, object &o) : fiber(a, o), m_rval(o) { m_cmd = A::cmd(); }
	template <class ACTOR>
	inline procedure(ACTOR &a, const fiber::rpcdata &d) :
		fiber(a, c_nil()), m_rval(c_nil()) {
		m_cmd = A::cmd(); fiber::sbf().m_rpcdata = d;
	}
	inline R &rval() { return m_rval; }
	template <class ALLOCATOR>
	inline void *operator new (size_t s, ALLOCATOR &o) { return o.malloc(s); }
	/* specialized operator new */
	inline void *operator new (size_t s, const type::nil &) { return util::mem::alloc(s); }
	/* procedure memory cannot freed by operator delete. */
	inline void operator delete (void *p) {}
	inline int resume(fabric &fbr, object &o) {
		int r;
		switch(operator () (fbr, o)) {
		case exec_error: {	/* unrecoverable error happen */
			r = fiber::respond(fbr.packer(), fbr.last_error());
			fiber::fin(true);	/* instead of delete */
		} break;
		case exec_finish: {	/* procedure finish (it should reply to caller actor) */
			r = fiber::respond(fbr.packer(), *this);
			fiber::fin(false);	/* instead of delete */
		} break;
		case exec_yield: 	/* procedure yields. (will invoke again) */
		case exec_delegate:	/* fiber send to another native thread. */
			return NBR_OK;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
		return r;
	}
public:
	/* for remote_actor::send (it requires method pack) */
	inline int pack(serializer &sr) const {
		return sr.pack_response(m_rval, fiber::msgid());
	}
	/* for local_actor::send (it requires method pack) */
	inline int pack_as_object(serializer &sr) const {
		return fabric::pack_as_object(*this, sr);
	}
public:	/* actual handler (must be specialized for each rpc command) */
	inline int operator () (fabric &fbr, object &o);
	inline void cleanup_onerror() {}
};

/* senders */
typedef fiber::session session;
typedef session::loop_handle loop_handle;
template <class ARGS>
static inline MSGID call(session &ss, fabric &fbr, ARGS &a) {
	if (ss.writeo(fbr.packer(), a) < 0) { return serializer::INVALID_MSGID; }
	return a.m_msgid;
}
template <class ARGS>
static inline MSGID call(loop_handle &la, fabric &fbr, ARGS &a) {
	if (la.send(fbr.packer(), a) < 0) { return serializer::INVALID_MSGID; }
	return a.m_msgid;
}
template <class ARGS>
static inline MSGID call(session &ss, ARGS &a) {
	return call(ss, fabric::tlf(), a);
}
template <class ARGS>
static inline MSGID call(loop_handle &la, ARGS &a) {
	return call(la, fabric::tlf(), a);
}
}
}

#include "proc.h"

namespace yue {
namespace rpc {
template <class PROC>
static inline int resume(fiber *f, fabric &fbr, object &o) {
	PROC *p = reinterpret_cast<PROC *>(f);
	return p->resume(fbr, o);
}
template <class PROC>
static inline int respond_as(fiber *f, fabric &fbr) {
	PROC *p = reinterpret_cast<PROC *>(f);
	return p->respond(fbr.packer(), *p);
}
}
#define PROCEDURE(name) 								\
	yue::rpc::procedure<yue::rpc::name::rval, yue::rpc::name::args>

#define RESUME(name, f, fbr, o)	case rpc::proc::name: {	\
	return rpc::resume<PROCEDURE(name)>(f, fbr, o);		\
} break;

#define WEAVE(name, a, o) case rpc::proc::name: {		\
	return new(o) PROCEDURE(name)(a, o); 				\
} break;

#define DESTROY(name, f, onerror) case rpc::proc::name: {		\
	PROCEDURE(name)* fb = reinterpret_cast<PROCEDURE(name)*>(f);\
	delete fb; 											\
	f->cleanup();										\
} break;

#define RESPOND(name, f, fbr, r) case rpc::proc::name: {	\
	r = rpc::respond_as<PROCEDURE(name)>(f, fbr);		\
} break;


template <class ACTOR, class ALLOCATOR>
inline fiber *fiber::create(ACTOR &a, ALLOCATOR &o) {
	switch(o.cmd()) {
	WEAVE(keepalive, a, o)
	WEAVE(callproc, a, o)
	WEAVE(callmethod, a, o)
	default: ASSERT(false); return NULL;
	}
}

inline int fiber::resume(fabric &fbr, object &o) {
	switch(cmd()) {
	RESUME(keepalive, this, fbr, o)
	RESUME(callproc, this, fbr, o)
	RESUME(callmethod, this, fbr, o)
	default: ASSERT(false); break;
	}
	return NBR_ENOTFOUND;
}

inline void fiber::fin(bool error) {
	switch(cmd()) {
	DESTROY(keepalive, this, error)
	DESTROY(callproc, this, error)
	DESTROY(callmethod, this, error)
	default: ASSERT(false); break;
	}
}

inline int fiber::on_respond(int result, fabric &fbr) {
	int r;
	switch(result) {
	case exec_error: {	/* unrecoverable error happen */
		r = respond(fbr.packer(), fbr.last_error());
	} break;
	case exec_finish: {	/* procedure finish (it should reply to caller actor) */
		switch(cmd()) {
		RESPOND(keepalive, this, fbr, r)
		RESPOND(callproc, this, fbr, r)
		RESPOND(callmethod, this, fbr, r)
		}
	} break;
	case exec_yield: 	/* procedure yields. (will invoke again) */
	case exec_delegate:	/* fiber send to another native thread. */
		return NBR_OK;
	default:
		ASSERT(false);
		return NBR_EINVAL;
	}
	return r;
}


}

#endif