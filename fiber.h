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

class fiber : public constant::fiber {
public:
	enum {
		from_remote,
		from_local,
		from_handler,
		from_nop,
	};
	typedef util::functional<int (fabric &, object &)> handler;
	typedef int (*chandler)(object_struct *);
protected:
	/* channel */
	U8 m_type, padd[3];
	union {
		U8 m_lact[sizeof(local_actor)];
		U8 m_ract[sizeof(remote_actor)];
		U8 m_hact[sizeof(handler)];
	};
	object m_obj;
	fiber(local_actor &from, object &o) : 
		m_type(from_local), m_obj(o) { l_act() = from; }
	fiber(remote_actor &from, object &o) :
		m_type(from_remote), m_obj(o) { r_act() = from; ASSERT(from.valid()); ASSERT(r_act().valid()); }
	fiber(handler &h, object &o) : 
		m_type(from_handler), m_obj(o) { h_act() = h; }
	fiber(const type::nil &n, object &o) :
		m_type(from_nop), m_obj(o) { }
	local_actor &l_act() { return *reinterpret_cast<local_actor *>(m_lact); }
	remote_actor &r_act() { return *reinterpret_cast<remote_actor *>(m_ract); }
	handler &h_act() { return *reinterpret_cast<handler *>(m_hact); }
public:
	U8 type() const { return m_type; }
	object &obj() { return m_obj; }
	const object &obj() const { return m_obj; }
public:
	template <class ACTOR>
	static inline fiber *create(ACTOR &a, object &o);
	inline int resume(fabric &fbr, object &o);
	inline int yield(fabric &fbr, MSGID msgid);
	inline void fin(bool);
	template <class RESP>
	int respond(serializer &sr, RESP &resp) {
		TRACE("fiber::respond: type=%u\n", m_type);
		switch(m_type) {
		case from_remote: return r_act().send(sr, resp);
		case from_local: return l_act().send(sr, resp);
		case from_handler: /* TODO: call resume of handler */ASSERT(false);
		case from_nop: return NBR_OK;
		default: ASSERT(false); return NBR_EINVAL;
		}
	}
	void cleanup() {
		switch(m_type) {
		case from_remote: obj().fin(); break;
		case from_local: obj().fin(); break;
		case from_handler: obj().fin(); break;
		case from_nop: obj().fin(); break;
		default: ASSERT(false); return;
		}
	}
};

struct fiber_handler : public fiber::handler {
	template <class FUNCTION> fiber_handler(FUNCTION &f) : fiber::handler(f) {}
};

}

#endif
