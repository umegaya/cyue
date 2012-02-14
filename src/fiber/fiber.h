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

#include "loop.h"
#include "session.h"
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
	typedef yue::handler::session session;
	typedef session::loop_handle thread;
	typedef session::stream_handle stream;
	typedef session::datagram_handle datagram;
	enum {
		from_stream,
		from_datagram,
		from_thread,
		from_handler,
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
	typedef util::functional<int (fabric &, void *)> phandler;
	typedef int (*chandler)(object *);
protected:
	/* channel */
	U8 m_type, m_allocator_type, m_cmd, padd;
	union {
		U8 m_thread[sizeof(thread)];
		U8 m_stream[sizeof(stream)];
		U8 m_datagram[sizeof(datagram)];
		U8 m_hact[sizeof(handler)];
	};
	union {
		U8 m_obj[sizeof(object)];
		U8 m_sbf[sizeof(allocator)];
	};
	template <class ALLOCATOR> fiber(thread &t, ALLOCATOR &o) :
		m_type(from_thread) { thread_ref() = t; set_allocator(o); }
	template <class ALLOCATOR> fiber(stream &s, ALLOCATOR &o) :
		m_type(from_stream) { stream_ref() = s; set_allocator(o); 
		ASSERT(stream_ref().valid()); }
	template <class ALLOCATOR> fiber(datagram &s, ALLOCATOR &o) :
		m_type(from_datagram) { datagram_ref() = s; set_allocator(o); 
		ASSERT(datagram_ref().valid()); }
	template <class ALLOCATOR> fiber(handler &h, ALLOCATOR &o) :
		m_type(from_handler) { handler_ref() = h; set_allocator(o); }
	template <class ALLOCATOR> fiber(const type::nil &n, ALLOCATOR &o) :
		m_type(from_nop) { set_allocator(o); }
protected:
	inline thread &thread_ref() { return *reinterpret_cast<thread *>(m_thread); }
	inline stream &stream_ref() { return *reinterpret_cast<stream *>(m_stream); }
	inline datagram &datagram_ref() { return *reinterpret_cast<datagram *>(m_datagram); }
	inline handler &handler_ref() { return *reinterpret_cast<handler *>(m_hact); }
	inline U8 type() const { return m_type; }
	inline object &obj() { return *reinterpret_cast<object *>(m_obj); }
	inline const object &obj() const { return *reinterpret_cast<const object *>(m_obj); }
	inline allocator &sbf() { return *reinterpret_cast<allocator *>(m_sbf); }
	inline const allocator &sbf() const { 
		return *reinterpret_cast<const allocator *>(m_sbf); 
	}
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
	inline int send_handler(serializer &sr, RESP &resp);
	template <class RESP>
	inline int send_loop(serializer &sr, RESP &resp);
	template <class RESP>
	int respond(serializer &sr, RESP &resp) {
		TRACE("fiber::respond: type=%u\n", m_type);
		switch(m_type) {
		case from_stream: return stream_ref().send(sr, resp);
		case from_datagram: return datagram_ref().send(sr, resp);
		case from_thread: return fiber::send_loop(sr, resp);
		case from_handler: return fiber::send_handler(sr, resp);
		case from_nop: return NBR_OK;
		default: ASSERT(false); return NBR_EINVAL;
		}
	}
	inline int on_respond(int result, fabric &fbr);
	inline void cleanup() { memfree(); }
	inline fiber_context context() {
		fiber_context c;
		switch(m_type) {
		case from_stream: c.m_fd = stream_ref().parent_fd(); break;
		case from_datagram: c.m_fd = datagram_ref().parent_fd(); break;
		case from_thread:
		case from_handler:
		case from_nop: 	c.m_fd = INVALID_FD; break;
		default: ASSERT(false); c.m_fd = INVALID_FD; break;
		}
		return c;
	}
};
}

#endif
