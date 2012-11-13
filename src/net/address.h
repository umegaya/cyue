/***************************************************************
 * address.h : abstruction of network address expression
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
#if !defined(__ADDRESS_H__)
#define __ADDRESS_H__

#include "parking.h"

namespace yue {
namespace net {
class address {
protected:
	union {
		char *m_p;
		struct sockaddr m_sa;
	};
	socklen_t m_al;
public:
	address() : m_al(sizeof(m_sa)) {}
	~address() { if (m_al == 0) { util::mem::free(m_p); } }
	address(const address &a) { *this = a; }
	socklen_t *len_p() { return &m_al; }
	char *addr_p() { return reinterpret_cast<char *>(&m_sa); }
	const char *addr_p() const { return reinterpret_cast<const char*>(&m_sa); }
	const struct sockaddr &addr() const { return m_sa; }
	socklen_t len() const { return m_al; }
	inline const address &operator = (const address &a) {
		m_al = a.m_al;
		if (m_al == 0) {
			m_p = util::str::dup(a.m_p);
			if (!m_p) { ASSERT(false); return *this; }
		}
		else {
			util::mem::copy(&m_sa, &(a.m_sa), m_al);
		}
		return *this;
	}
	inline bool operator == (const address &a) const {
		if (m_al == 0) { return m_p == a.m_p; }
		return m_al == a.m_al && util::mem::cmp(addr_p(), a.addr_p(), m_al) == 0;
	}
public:
	inline int set(const char *a, transport *p = NULL) {
		return syscall::s2a(a, &m_sa, &m_al, p);
	}
	inline const char *get(char *b, size_t l, transport *p = NULL) const {
		if (syscall::a2s((void *)&m_sa, m_al, b, l, p) < 0) {
			return "(invalid)"; 
		}
		return b;
	}
	inline int set(DSCRPTR fd) {
		return util::syscall::get_sock_addr(fd, addr_p(), len_p());
	}
	static inline const char *to_uri(char *p, size_t l, DSCRPTR fd, transport *t = NULL) {
		address a;
		if (a.set(fd) < 0) { return NULL; }
		return to_uri(p, l, a, t);
	}
	static inline const char *to_uri(char *p, size_t l, const address &a, transport *t = NULL) {
		char b[256];
		util::str::printf(p, l, "%s://%s", t ? t->name : "tcp", a.get(b, sizeof(b), t));
		return p;
	}
};
}
}

#endif
