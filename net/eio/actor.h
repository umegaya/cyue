/***************************************************************
 * actor.h : programming unit which has its own loop, and
 * can send/recv message.
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
#if !defined(__ACTOR_H__)
#define __ACTOR_H__

#include "sbuf.h"
#include "wbuf.h"
#include "serializer.h"

namespace yue {
class fiber;
class fiber_handler;
namespace module {
namespace net {
namespace eio {
struct remote_actor : public writer {
	typedef writer super;
	inline remote_actor(const super &w) : super(w) {}
	inline remote_actor() : super() {}
	template <class SR, class O>
	inline int send(SR &sr, O &o) {
		ASSERT(super::valid());
		return super::writeo<SR, O>(sr, o);
	}
	void close();
};
struct dgram_actor : public writer {
	typedef writer super;
	address m_addr;
	inline dgram_actor(const super &w, const address &a) : super(w), m_addr(a) {}
	inline dgram_actor() : super() {}
	template <class SR, class O>
	inline int send(SR &sr, O &o) {
		ASSERT(super::valid());
		return super::writedg<SR, O>(sr, o, m_addr);
	}
	void close();
};
struct local_actor {
	void *m_em;
	local_actor(void *em) : m_em(em) {}
	local_actor() : m_em(NULL) {}
	void set(void *em) { m_em = em; }
	template <class SR, class O> int send(SR &sr, O &o);
	bool feed(object &o);
	bool delegate(fiber *f, object &o);
	bool delegate(fiber_handler &fh, object &o);
};
}
}
}
}

#endif
