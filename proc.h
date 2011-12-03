/***************************************************************
 * proc.h : rpc-detail implementation
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
#if !defined(__PROC_H__)
#define __PROC_H__
namespace yue {
namespace rpc {
namespace proc {
enum {
	keepalive,
	callproc,
	callmethod,
};
}

/* keep alive */
namespace keepalive {
#define KEEPALIVE_TRACE(...)
struct args {
	UTIME m_tstamp;
	static inline U8 cmd() { return proc::keepalive; }
	inline int operator() (serializer &sr, MSGID) const {
		verify_success(sr.push_array_len(1));
		verify_success(sr << m_tstamp);
		return sr.len(); 
	}
	struct accessor {
		object &m_o;
		inline accessor(object &o) : m_o(o) {}
		inline UTIME tstamp() const { return m_o.arg(0); }
	};
};
struct rval {
	object *m_o;
	rval(object &o) : m_o(&o) {}
	rval(const type::nil &n) : m_o(NULL) {}
	inline int operator() (serializer &sr) const { 
		KEEPALIVE_TRACE("keepalive: respond: ts=%llu\n", a.tstamp());
		verify_success(sr.push_array_len(1));
		ASSERT(m_o);
		args::accessor a(*m_o);
		verify_success(sr << a.tstamp());
		return sr.len(); 
	}
	struct accessor {
		object &m_o;
		inline accessor(object &o) : m_o(o) {}
		inline UTIME tstamp() const { return m_o.resp().elem(0); }
	};
};
}

/* call normal procedure */
namespace callproc {
struct args {
	coroutine *m_co;
	args() : m_co(NULL) {}
	args(coroutine *co) : m_co(co) {}
	static inline U8 cmd() { return proc::callproc; }
	const coroutine *co() const { return m_co; }
	inline int operator() (serializer &sr, MSGID assigned) const {
		/* array len also packed in following method
		 * (eg. lua can return multiple value) */
		verify_success(m_co->pack_stack(sr));
		return sr.len(); 
	}
	struct accessor {
		object &m_o;
		inline accessor(object &o) : m_o(o) {}
	};
};
struct rval {
	coroutine *m_co;
	yielded_context m_y;
	rval(object &o) : m_co(NULL), m_y() {}
	rval(const type::nil &n) : m_co(NULL), m_y() {}
	~rval() { if (m_co) { m_co->free(); m_co = NULL; } }
	coroutine *co() { return m_co; }
	inline int operator() (serializer &sr) const {
		/* array len also packed in following method
		 * (eg. lua can return multiple value) */
		verify_success(m_co->pack_response(sr));
		return sr.len();
	}
	inline bool initialized() const { return m_co != NULL; }
	inline int resume(object &o) { return m_co->resume(o); }
	inline int start(object &o, fiber_context c) { return m_co->resume(o, c); }
	inline int init(fabric &fbr, fiber *fb) {
		m_y.set(fb);
		return (m_co = fbr.lang().create(&m_y)) ? NBR_OK : NBR_EEXPIRE;
	}
};
}

/* call object method */
namespace callmethod {
struct args {
	static inline U8 cmd() { return proc::callmethod; }
	inline int operator() (serializer &sr, MSGID assigned) const {
		return sr.len(); 
	}
	struct accessor {
		object &m_o;
		inline accessor(object &o) : m_o(o) {}
	};
};
struct rval {
	rval(object &o) {}
	rval(const type::nil &n) {}
	inline int operator() (serializer &sr) const { 
		return sr.len(); 
	}
};
}


/* handlers */
/* keepalive */
template <>
inline int procedure<keepalive::rval, keepalive::args>
	::operator () (fabric &, object &) {
	args::accessor a(*(m_rval.m_o));
	KEEPALIVE_TRACE("keepalive: handler: ts=%llu\n", a.tstamp());
	return fiber::exec_finish;
}

/* callproc */
template <>
inline int procedure<callproc::rval, callproc::args>
	::operator () (fabric &f, object &o) {
	int r;
	if (!m_rval.initialized()) {
		if ((r = m_rval.init(f, this)) < 0) {
			f.set_last_error(r, obj().msgid(), "m_rval.init fails\n");
			return fiber::exec_error;
		}
		ASSERT(m_rval.initialized());
		if ((r = m_rval.start(o, fiber::context())) == fiber::exec_error) {
			f.set_last_error(NBR_EINTERNAL, obj().msgid(), m_rval.co());
		}
		return r;
	}
	/* because initialized means 'decide which thread process this coroutine', so else if */
	else if (m_rval.co()->ll().attached() != &f) {
		/* now 2 case to reach here.
		 * 1. server mode and thread which receives this object o,
		 * does not have coroutine assigned to it. (VM is different)
		 * 2. client library mode and this fabric does not have a responsibility
		 * to execute logic (because we assume 1 server instance only used from 1 client VM,
		 * another thread should pass object to it) */
		/* not always o == this->obj(), so pass o to delegate */
		return m_rval.co()->ll().attached()->delegate(this, o);
	}
	if ((r = m_rval.resume(o)) == fiber::exec_error) {
		f.set_last_error(NBR_EINTERNAL, obj().msgid(), m_rval.co());
	}
	return r;
}

/* callmethod */
template <>
inline int procedure<callmethod::rval, callmethod::args>
	::operator () (fabric &f, object &o) {
	return fiber::exec_finish;
}

}
}
#endif
