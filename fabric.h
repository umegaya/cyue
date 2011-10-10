/***************************************************************
 * fabric.h : fiber factory (also tls for each worker thread)
 * (create fiber from serializer::object: stream_dispatcher recipient)
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
#if !defined(__FABRIC_H__)
#define __FABRIC_H__

#include "fiber.h"
#include "map.h"
#include "ll.h"
#include "dbm.h"
#include "serializer.h"

namespace yue {
class fabric {
public:
	struct error {
		enum {
			ERROBJ_NIL,
			ERROBJ_STRING,
			ERROBJ_COROUTINE,
		};
		S32 m_errno;
		U32 m_msgid;
		U8 m_type, padd[3];
		union {
			char m_msg[256];
			coroutine *m_co;
		};
		inline error() { set(0, serializer::INVALID_MSGID, c_nil()); }
		template <typename EOBJ>
		inline error(int err, MSGID msgid, EOBJ eo) { set(err, msgid, eo); }
		inline void set_errno(int err, MSGID msgid) {
			m_errno = err; m_msgid = msgid;
		}
		inline const error &set(int err, MSGID msgid, const char *msg) {
			m_type = ERROBJ_STRING;
			set_errno(err, msgid);
			util::str::copy(m_msg, msg, sizeof(m_msg));
			return *this;
		}
		inline const error &set(int err, MSGID msgid, const type::nil &n) {
			m_type = ERROBJ_NIL;
			set_errno(err, msgid);
			return *this;
		}
		inline const error &set(int err, MSGID msgid, coroutine *co) {
			m_type = ERROBJ_COROUTINE;
			set_errno(err, msgid);
			m_co = co;
			return *this;
		}
		inline int pack(serializer &sr) const {
			return sr.pack_error(m_msgid, *this);
		}
		inline int pack_as_object(serializer &sr) const {
			return fabric::pack_as_object(*this, sr);
		}
		inline int operator () (serializer &sr) {
			verify_success(sr.push_array_len(2));
			verify_success(sr << m_errno);
			switch(m_type) {
			case ERROBJ_NIL: verify_success(sr.pushnil()); break;
			case ERROBJ_STRING: verify_success(sr << m_msg); break;
			case ERROBJ_COROUTINE: verify_success(m_co->pack_error(sr)); break;
			default: ASSERT(false); break;
			}
			return sr.len();
		}
	};
	struct yielded {
		enum {
			type_fiber,
			type_handler,
			type_chandler,
		};
		struct context {
			U8 m_type, padd[3];
			union {
				fiber *m_f;
				U8 m_h[sizeof(fiber::handler)];
				fiber::chandler m_cf;
			};
			inline context() {}
			inline context(fiber *f) { set(f); }
			inline context(fiber::handler &h) { set(h); }
			inline context(fiber::chandler f) { set(f); }
			inline void set(fiber *f) { m_type = type_fiber; m_f = f; }
			inline void set(fiber::handler &h) { m_type = type_handler; hd() = h; }
			inline void set(fiber::chandler f) { m_type = type_chandler; m_cf = f; }
			inline ~context() {}
			inline fiber::handler &hd() {
				return *reinterpret_cast<fiber::handler *>(m_h);
			}
			inline const fiber::handler &hd() const {
				return *reinterpret_cast<const fiber::handler *>(m_h);
			}
			inline void fin(bool error) {
				switch(m_type) {
				case type_fiber: m_f->fin(error); break;
				case type_handler: break;
				case type_chandler: break;
				default: ASSERT(false); break;
				}
			}
			inline int resume(fabric &fbr, object &o) {
				switch(m_type) {
				case type_fiber:
					return m_f->resume(fbr, o);
				case type_handler:
					return hd()(fbr, o);
				case type_chandler:
					return m_cf(&o);
				default: ASSERT(false); return NBR_EINVAL;
				}
			}
			inline int raise(fabric &fbr, const error &e) {
				switch(m_type) {
				case yielded::type_fiber:
					return m_f->respond(fbr.packer(), e);
				case yielded::type_handler:
				case yielded::type_chandler: {
					if (fabric::pack_as_object(e, fbr.packer()) < 0) {
						ASSERT(false);
						return NBR_EFORMAT;
					}
					object &o = fbr.packer().result();
					int r = resume(fbr, o);
					o.fin();
					return r;
				} break;
				default: ASSERT(false); return NBR_EINVAL;
				}
			}
		};
		UTIME m_start;
		MSGID m_msgid;
		context m_ctx;
		inline yielded() : m_ctx() {}
		inline yielded(U32 t_o, fiber *f, MSGID msgid) :
			m_start(util::time::now() + t_o), m_msgid(msgid), m_ctx(f) {}
		inline yielded(U32 t_o, fiber::handler &h, MSGID msgid) :
			m_start(util::time::now() + t_o), m_msgid(msgid), m_ctx(h) {}
		inline yielded(U32 t_o, fiber::chandler cf, MSGID msgid) :
			m_start(util::time::now() + t_o), m_msgid(msgid), m_ctx(cf) {}
		inline yielded(U32 t_o, const context &c, MSGID msgid) :
			m_start(util::time::now() + t_o), m_msgid(msgid) {
				util::mem::copy(&m_ctx, &c, sizeof(m_ctx));
		}
		inline ~yielded() {}
		inline bool timeout(UTIME now) const { return ((m_start) < now); }
		inline void fin(bool e) { m_ctx.fin(e); }
		inline int resume(fabric &fbr, object &o) { return m_ctx.resume(fbr, o); }
		inline int raise(fabric &fbr, const class error &e) { return m_ctx.raise(fbr, e); }
		inline MSGID msgid() const { return m_msgid; }
	};
protected:
	static util::map<yielded, MSGID> m_yielded_fibers;
	static util::map<local_actor, UUID> m_object_assign_table;
	static int m_max_fiber, m_max_object, m_fiber_timeout_us;
	static class server *m_server;
	ll m_lang;
	dbm m_storage;
	serializer m_packer;
	local_actor m_la;
	error m_error;
public:
	template <class DATA>
	static inline int pack_as_object(DATA &d, serializer &sr) {
		int r; pbuf pbf;
		if ((r = pbf.reserve(sizeof(DATA) * 2)) < 0) { return r; }
		sr.start_pack(pbf.p(), pbf.available());
		if ((r = d.pack(sr)) < 0) { return r; }
		pbf.commit(r);
		r = sr.unpack(pbf);
		ASSERT(r == serializer::UNPACK_SUCCESS);
		return r == serializer::UNPACK_SUCCESS ? NBR_OK : NBR_EINVAL;
	}
	template <typename EOBJ>
	const error &set_last_error(int err, MSGID msgid, EOBJ eo) {
		return m_error.set(err, msgid, eo);
	}
	const error &last_error() { return m_error; }
public:
	fabric() : m_lang(), m_storage(), m_packer(), m_la()  {}
	static void configure(class server *s,
		int max_fiber, int max_object, int fiber_timeout_us) {
		m_max_fiber = max_fiber;
		m_max_object = max_object;
		m_fiber_timeout_us = fiber_timeout_us;
		m_server = s;
	}
	static int init() {
		if (!m_yielded_fibers.init(
			m_max_fiber, m_max_fiber, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (!m_object_assign_table.init(
			m_max_object, m_max_object, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		return NBR_OK;
	}
	static void fin() {
		m_yielded_fibers.fin();
		m_object_assign_table.fin();
	}
	static int timeout_iterator(yielded *py, UTIME now) {
		TRACE("check_timeout: %p thrs=%u\n", py, m_fiber_timeout_us);
		if (py->timeout(now)) {
			TRACE("check_timeout: erased %p, %u\n", py, py->msgid());
			yielded y;
			if (yielded_fibers().find_and_erase(py->msgid(), y)) {
				fabric &f = tlf();
				y.raise(f, f.set_last_error(NBR_ETIMEOUT, y.msgid(), c_nil()));
				y.fin(true);
			}
		}
		return NBR_OK;
	}
	static int check_timeout(U64 c) {
		UTIME now = util::time::now();
		yielded_fibers().iterate(timeout_iterator, now);
		return NBR_OK;
	}
	int tls_init(local_actor &la);
	void tls_fin() {}
	static inline fabric &tlf() { return *reinterpret_cast<fabric *>(net::tls()); }
	inline local_actor &tla() { return m_la; }
	static inline util::map<yielded, MSGID> &yielded_fibers() {
		return m_yielded_fibers;
	}
	static inline util::map<local_actor, UUID> &object_assign_table() {
		return m_object_assign_table;
	}
	inline ll &lang() { return m_lang; }
	inline dbm &storage() { return m_storage; }
	inline serializer &packer() { return m_packer; }
	static inline class server *served() { return m_server; }
	template <class ACTOR>
	inline int recv(ACTOR &a, object &o) {
		fiber *f;
		if (o.is_request()) {
			if (!(f = fiber::create(a, o))) {
				a.send(packer(), m_error.set(NBR_EMALLOC, o.msgid(), c_nil()));
				goto end;
			}
			TRACE("recv create fiber %p %u\n", f, o.msgid());
			return resume(*f, o);/* if error is happen, then send error */
		}
		else if (o.is_response()) {
			TRACE("recv resp: msgid = %u\n", o.msgid());
			yielded y;
			if (!yielded_fibers().find_and_erase(o.msgid(), y)) {
				TRACE("recv resp: fb for msgid = %u not found\n", o.msgid());
				goto end;
			}
			y.resume(*this, o);
			goto end;
		}
		else if (o.is_notify() || true/* and others */) {
			ASSERT(false);
			goto end;
		}
	end:
		o.fin();
		return NBR_OK;
	}
	static inline void close(remote_actor &) { return; }
	inline int resume(fiber &f, object &o) { return f.resume(*this, o); }
	inline int resume(fiber &f) { return f.resume(*this, f.obj()); }
	template <class FIBER>
	static inline int suspend(FIBER f, MSGID msgid) {
		yielded y(m_fiber_timeout_us, f, msgid);
		return yielded_fibers().insert(y, msgid) ?
			fiber::exec_yield : fiber::exec_error;
	}
	inline int delegate(fiber *f) {
		return m_la.delegate(f) < 0 ?
			fiber::exec_error : fiber::exec_delegate;
	}
	inline int delegate(fiber_handler &fh, object &o) {
		return m_la.delegate(fh, o) < 0 ?
			fiber::exec_error : fiber::exec_delegate;
	}
};
struct yielded_context : public fabric::yielded::context {
	inline yielded_context() : fabric::yielded::context() {}
	inline yielded_context(fiber::handler &h) : fabric::yielded::context(h) {}
	inline yielded_context(fiber::handler h) : fabric::yielded::context(h) {}
};

template <>
inline int fabric::suspend<yielded_context &>(yielded_context &c, MSGID msgid) {
	yielded y(m_fiber_timeout_us, c, msgid);
	return yielded_fibers().insert(y, msgid) ?
		fiber::exec_yield : fiber::exec_error;
}
}

#endif
