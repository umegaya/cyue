/***************************************************************
 * fiber.h : execute unit for asynchronous network programming
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
#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "net.h"
#include "serializer.h"
#include "constant.h"

namespace yue {

class fabric;

struct fiber_context {
	union {
		DSCRPTR m_fd;	/* if remote node, listener/receiver fd, otherwise INVALID_FD */
	};
};

class fiber : public constant::fiber {
public:
	enum {
		from_remote,
		from_local,
		from_handler,
		from_dgram,
		from_nop,
	};
	enum {
		allocator_invalid,
		allocator_object,
		allocator_sbuf,
	};
	/* default allocator */
	struct allocator {
		struct rpcdata {
			MSGID m_msgid;
			inline rpcdata(MSGID msgid = serializer::INVALID_MSGID)
				: m_msgid(msgid) {}
		} m_rpcdata;
		sbuf m_sbf;
		inline allocator() : m_sbf() {}
		inline void fin() { m_sbf.fin(); }
		inline void *malloc(size_t s) { return m_sbf.malloc(s); }
		inline MSGID msgid() const { return m_rpcdata.m_msgid; }
		inline void *operator new (size_t s, U8 *p) { 
			ASSERT(s == sizeof(allocator)); return p; 
		}
	};
	typedef allocator::rpcdata rpcdata;
	typedef util::functional<int (fabric &, object &)> handler;
	typedef int (*chandler)(object_struct *);
protected:
	/* channel */
	U8 m_type, m_allocator_type, m_cmd, padd;
	union {
		U8 m_lact[sizeof(local_actor)];
		U8 m_ract[sizeof(remote_actor)];
		U8 m_dact[sizeof(dgram_actor)];
		U8 m_hact[sizeof(handler)];
	};
	union {
		U8 m_obj[sizeof(object)];
		U8 m_sbf[sizeof(allocator)];
	};
	template <class ALLOCATOR>
	fiber(local_actor &from, ALLOCATOR &o) :
		m_type(from_local) { l_act() = from; set_allocator(o); }
	template <class ALLOCATOR>
	fiber(remote_actor &from, ALLOCATOR &o) :
		m_type(from_remote) {
		r_act() = from; set_allocator(o); ASSERT(from.valid()); ASSERT(r_act().valid());
	}
	template <class ALLOCATOR>
	fiber(dgram_actor &from, ALLOCATOR &o) :
		m_type(from_dgram) {
		d_act() = from;  set_allocator(o); ASSERT(from.valid()); ASSERT(d_act().valid());
	}
	template <class ALLOCATOR>
	fiber(handler &h, ALLOCATOR &o) :
		m_type(from_handler) { h_act() = h; set_allocator(o); }
	template <class ALLOCATOR>
	fiber(const type::nil &n, ALLOCATOR &o) :
		m_type(from_nop) { set_allocator(o); }
protected:
	local_actor &l_act() { return *reinterpret_cast<local_actor *>(m_lact); }
	remote_actor &r_act() { return *reinterpret_cast<remote_actor *>(m_ract); }
	dgram_actor &d_act() { return *reinterpret_cast<dgram_actor *>(m_ract); }
	handler &h_act() { return *reinterpret_cast<handler *>(m_hact); }
	U8 type() const { return m_type; }
	object &obj() { return *reinterpret_cast<object *>(m_obj); }
	const object &obj() const { return *reinterpret_cast<const object *>(m_obj); }
	allocator &sbf() { return *reinterpret_cast<allocator *>(m_sbf); }
	const allocator &sbf() const { return *reinterpret_cast<const allocator *>(m_sbf); }
	inline void set_allocator(object &o) {
		m_allocator_type = allocator_object;
		obj() = o;
	}
	inline void set_allocator(const type::nil &n) {
		m_allocator_type = allocator_sbuf;
		new(m_sbf) allocator();
	}
	inline MSGID msgid() const {
		switch(m_allocator_type) {
		case allocator_object: return obj().msgid();
		case allocator_sbuf: return sbf().msgid();
		default: ASSERT(false); return serializer::INVALID_MSGID;
		}
	}
	inline U8 cmd() const { return m_cmd; }
	inline void memfree() {
		switch(m_allocator_type) {
		case allocator_object: obj().fin(); break;
		case allocator_sbuf: sbf().fin(); delete this; break;
		default: ASSERT(false); break;
		}
	}
public:
	template <class ACTOR, class ALLOCATOR>
	static inline fiber *create(ACTOR &a, ALLOCATOR &o);
	/* can only invoked by fiber created with object */
	inline int resume(fabric &fbr) {
		ASSERT(m_allocator_type == allocator_object);
		return resume(fbr, obj());
	}
	inline int resume(fabric &fbr, object &o);
	inline void fin(bool);
	template <class RESP>
	int send_handler(serializer &sr, RESP &resp);
	template <class RESP>
	int respond(serializer &sr, RESP &resp) {
		TRACE("fiber::respond: type=%u\n", m_type);
		switch(m_type) {
		case from_remote: return r_act().send(sr, resp);
		case from_local: return l_act().send(sr, resp);
		case from_dgram: return d_act().send(sr, resp);
		case from_handler: return fiber::send_handler(sr, resp);
		case from_nop: return NBR_OK;
		default: ASSERT(false); return NBR_EINVAL;
		}
	}
	void cleanup() {
		switch(m_type) {
		case from_remote: memfree(); break;
		case from_local: memfree(); break;
		case from_handler: memfree(); break;
		case from_dgram: memfree(); break;
		case from_nop: memfree(); break;
		default: ASSERT(false); return;
		}
	}
	inline fiber_context context() {
		fiber_context c;
		switch(m_type) {
		case from_remote: c.m_fd = r_act().parent_fd(); break;
		case from_dgram: c.m_fd = d_act().parent_fd(); break;
		case from_local:
		case from_handler:
		case from_nop: 	c.m_fd = INVALID_FD; break;
		default: ASSERT(false); c.m_fd = INVALID_FD; break;
		}
		return c;
	}
};

struct fiber_handler : public fiber::handler {
	template <class FUNCTION> fiber_handler(FUNCTION &f) : fiber::handler(f) {}
};

}

#endif
