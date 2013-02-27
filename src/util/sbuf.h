/***************************************************************
 * sbuf.h : socket data buffer
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
#if !defined(__SBUF_H__)
#define __SBUF_H__

#include "types.h"
#include "util.h"
#include "thread.h"
#include "msgid.h"

namespace yue {
namespace util {

#define SBUF_TRACE(__FMT,...) \
		//TRACE("sbuf:" __FMT, __VA_ARGS__)
class pbuf {
	FORBID_COPY(pbuf);
	friend class basic_socket;
	friend class sbuf;
public:
	typedef int refcnt;
	static const size_t INITIAL_BUFFER_SIZE = 256;
	struct ptr {
		refcnt m_cnt;
		char m_p[0];
		inline void unref() {
			SBUF_TRACE("unref: %p cnt:%u\n", this, m_cnt);
			ASSERT(referred());
			if (__sync_add_and_fetch(&m_cnt, -1) == 0) {
				SBUF_TRACE("unref: free this mem %p\n", this);
				util::mem::free(this);
			}
		}
		inline ptr *refer() { SBUF_TRACE("refer: %p cnt:%u\n", this, m_cnt);__sync_add_and_fetch(&m_cnt, 1); return this; }
		inline bool referred() const { return m_cnt > 0; }
	protected:
		~ptr() {}
	};
protected:
	ptr *m_ptr;
	size_t m_len, m_limit, m_ofs;
	inline void fin() { unref(); }
public:
	pbuf() : m_ptr(NULL), m_len(0), m_limit(0), m_ofs(0) {}
	~pbuf() { fin(); }
	inline ptr *refer() { return m_ptr->refer(); }
	inline void unref() { if (m_ptr) { m_ptr->unref(); } }
	inline void copy(pbuf &pbf) {
		m_len = pbf.m_len;
		m_limit = pbf.m_limit;
		m_ofs = pbf.m_ofs;
		unref();
		m_ptr = pbf.refer();
	}
public:
	inline char *p() { return m_ptr->m_p; }
	inline size_t limit() const { return m_limit; }
	inline char *end() { return p() + m_limit; }
	inline size_t available() const { ASSERT(m_len <= m_limit); return (m_limit - m_len); }
	inline char *last_p() { return p() + m_len; }
	inline size_t last() const { return m_len; }
	inline void set_last(size_t s) { m_len = s; }
	inline char *cur_p() { return p() + m_ofs; }
	inline void commit(size_t sz) {
		ASSERT((m_len + sz) <= m_limit);
		m_len += sz;
	}
	inline void rollback(size_t sz) {
		ASSERT((m_len + sz) <= m_limit && (m_len >= sz));
		if (sz >= m_len) { m_len = 0; }
		else { m_len -= sz; }
	}
 	inline void shrink_used_buffer_size(size_t sz) {
 		ASSERT(sz <= m_len);
 		if (sz >= m_len) { m_len = 0; }
 		else {
 			util::mem::move(m_ptr->m_p, m_ptr->m_p + sz, m_len - sz);
			m_len -= sz;
 		}
 	}
	inline int reserve(size_t sz) {
		size_t r = available();
		if (r >= sz) { return NBR_OK; }
		ptr *p;
		//TRACE("pbuf: m_ptr=%p %lld %lld %lld %lld %lld\n", m_ptr, (S64)r, (S64)sz, (S64)m_len, (S64)m_limit, (S64)m_ofs);
		if (m_ptr) {
			/* create new ptr object and copy unread buffer into it. */
			size_t copyb = (last() - ofs());
			sz += copyb;
			size_t org = m_limit;
			//TRACE("cpb, sz, org = %lld %lld %lld\n", (S64)copyb, (S64)sz, (S64)org);
			while (sz > m_limit) { m_limit <<= 1; }
			if (!(p = reinterpret_cast<ptr *>(util::mem::alloc(m_limit + sizeof(ptr))))) {
				m_limit = org;
				return NBR_EMALLOC;
			}
			if (copyb > 0) { util::mem::copy(p->m_p, cur_p(), copyb); }
			m_ofs = 0;
			m_len = copyb;
			m_ptr->unref();	/* decrement refcnt. it should be freed (if refcnt => 0) */
		}
		else {
			m_limit = INITIAL_BUFFER_SIZE;
			/* now m_ptr == NULL, means no m_ofs, m_len. so compare with m_limit work fine. */
			while (sz > m_limit) { m_limit <<= 1; }
			if (!(p = reinterpret_cast<ptr *>(util::mem::alloc(m_limit + sizeof(ptr))))) {
				m_limit = 0;
				return NBR_EMALLOC;
			}
		}
		p->m_cnt = 1;
		m_ptr = p;
		return NBR_OK;
	}
 	inline void add_parsed_ofs(int sz) {
		ASSERT(sz == 0 || (sz < 0 && (m_ofs > (size_t)(-sz))) || (sz > 0 && (m_ofs + sz) <= m_len));
		m_ofs += sz;
	}
 	inline void reset() {
 		m_len = 0;
 	}
public:
	inline char pop() { return p()[m_ofs++]; }
	inline char *push(U8 b) { p()[m_len++] = (char)b; return last_p(); }
	inline bool readable() const { return m_ofs < m_len; }
	inline size_t ofs() const { return m_ofs; }
	inline size_t parsed() const { return m_ofs; }
};

class sbuf {
	FORBID_COPY(sbuf);
protected:
	static const size_t ALLOC_UNIT_SIZE = 256;
	struct alloc_unit {
		struct alloc_unit *next;
		char p[0];
	} *m_alloc;
	char *m_block;
	size_t m_free;
	struct buf_reference {
		struct buf_reference *m_next;
		pbuf::ptr *m_ptr;
	} *m_refs;
public:
	sbuf() : m_alloc(NULL), m_free(0), m_refs(NULL) {}
	~sbuf() { fin(); }
public:
	inline void fin() {
		struct buf_reference *ref = m_refs, *nref;
		while ((nref = ref)) {
			SBUF_TRACE("pbuf::ptr free: %p %p\n", this, nref->m_ptr);
			ref = ref->m_next;
			nref->m_ptr->unref();
		}
		struct alloc_unit *au = m_alloc, *nau;
		while ((nau = au)) {
			au = au->next;
			SBUF_TRACE("sbuf chunk free: %p %p\n", this, nau);
			util::mem::free(nau);
		};
	}
	inline sbuf *refer(pbuf &pbf) {
		struct buf_reference *ref = reinterpret_cast<buf_reference *>(
			this->malloc(sizeof(struct buf_reference))
		);
		if (!ref) { ASSERT(false); return NULL; }
		ref->m_next = m_refs;
		ref->m_ptr = pbf.refer();
		m_refs = ref;
		return this;
	}
	inline void *malloc(size_t s) {
		if (m_free > s) { m_free -= s; }
		else {
			size_t sz = ALLOC_UNIT_SIZE;
			while (s > sz) { sz <<= 1; }
			alloc_unit *ptr = reinterpret_cast<alloc_unit *>(
				util::mem::alloc(sz + sizeof(alloc_unit))
			);
			if (!ptr) { ASSERT(false); return NULL; }
			SBUF_TRACE("sbuf: new chunk: %p/%u\n", ptr, sz + sizeof(alloc_unit));
			ptr->next = m_alloc;
			m_alloc = ptr;
			m_free = (sz - s);
			m_block = m_alloc->p;
		}
		char *p = m_block;
		(m_block += s);
		SBUF_TRACE("sbuf:alloc:%p for %u\n", p, s);
		return p;
	}
};

}
}

#endif
