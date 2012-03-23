/***************************************************************
 * queue.h : fast queue implementation for thread messaging (inspired by yqueue of zmq)
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
#if !defined(__QUEUE_H__)
#define __QUEUE_H__

#include "util.h"
#include "thread.h"

#define Q_TRACE(...) 

namespace yue {
namespace util {
template <class T, U16 N>
class queue {
protected:
	typedef int chunk_pos;
	struct chunk {
		struct chunk *next;
		T p[0];
	} *m_c, *m_w, *m_r, *m_free;
	T *m_wp, *m_rp;/* read/write buffer ptr for next pop/push */
	chunk_pos m_wpos, m_rpos;
	thread::mutex m_mtx;
public:
	queue() { util::mem::bzero(this, sizeof(*this)); }
	~queue() { fin(); }
	int init() {
		if (m_mtx.init() < 0) { return NBR_EPTHREAD; }
		if (!(m_c = allocate())) { return NBR_EMALLOC; }
		m_w = m_r = m_c;
		m_free = NULL;
		m_wpos = m_rpos = 0;
		m_wp = current(m_w, m_wpos);
		m_rp = current(m_r, m_rpos);
		m_c->next = NULL;
		dump("start");
		return NBR_OK;
	}
	inline void dump(const char *tag) {
		Q_TRACE("%s:", tag);
		Q_TRACE("thrd=%lx:c,w,r=%p,%p,%p\n",
			thread::current() ? thread::current()->id() : 0, m_c, m_w, m_r);
	}
	void fin() {
		Q_TRACE("free used chunk\n");
		struct chunk *c = m_c, *nc;
		while ((nc = c)) {
			c = c->next;
			queue::free(nc, false);/* real free memory */
		}
		Q_TRACE("free cache chunk\n");
		c = m_free;
		while ((nc = c)) {
			c = c->next;
			queue::free(nc, false);/* real free memory */
		}
		m_free = NULL;
	}
	/* if called from multiple thread, pop is not thread safe. */
	inline bool pop(T &p) {
		return readable() ? (movestep(m_r, m_rpos, &m_rp, &p, true) != NULL) : false;
	}
	inline bool mpop(T &p) {
		return readable() ? (mmovestep(m_r, m_rpos, &m_rp, &p, true) != NULL) : false;
	}
	/* if called from multiple thread, push is not thread safe. */
	inline bool push(T &p) {
		return NULL != movestep(m_w, m_wpos, &m_wp, &p, false);
	}
	inline bool mpush(T &p) {
		return NULL != mmovestep(m_w, m_wpos, &m_wp, &p, false);
	}
protected:
#define ATOMIC(expr)	expr
	inline T *movestep(struct chunk *&c, chunk_pos &pos, T **ptr, T *val, bool rval) {
		T *p, *next_p;
		struct chunk *tmp;
		ATOMIC(p = *ptr);
		/* readable check done by comparison between m_wp and m_rp,
		 * so even if this queue during expand(), read thread never reach here
		 * because in that case m_wp is never changed until expand finish. */
		if (N == ++pos) {
			if (rval) {
				*val = *p;				/* read value */
				tmp = c;
				c = c->next;
				queue::free(tmp);
				dump("rnext");
			}
			else {
				*p = *val;				/* write value */
				if (!c->next && !expand()) {
					ASSERT(false);
					return NULL;
				}
				c = c->next;
				dump("wnext");
			}
			pos = 0;
		}
		else { 
			if (rval) { *val = *p; }	/* read value */
			else { *p = *val; }			/* write value */
		}
		next_p = current(c, pos);
		ATOMIC(*ptr = next_p);				/* this should be done atomically */
		return p;
	}
	inline T *mmovestep(struct chunk *&c, chunk_pos &pos, T **ptr, T *val, bool rval) {
		T *p, *next_p;
		volatile chunk_pos tmp_pos;
		struct chunk *tmp;
		/* readable check done by comparison between m_wp and m_rp,
		 * so even if this queue under expand, read thread never reach here
		 * because in that case m_wp is never changed until expand finish. */
retry:
		if (N <= (tmp_pos = __sync_add_and_fetch(&pos, 1))) {
			/* current chunk expire. start chunk swap phase 
			(set current chunk to chunk::next) */
			if (tmp_pos == N) {
				/* only one thread at the same time can come here
				 * so never lock these operation */
				p = current(c, N - 1);
				if (rval) { *val = *p; }	/* read value */
				else {
					*p = *val; 				/* write value */
					/* if no more next chunk, allocate. */
					if (!c->next && !expand()) {
						ASSERT(false); return NULL;
					}
				}
				next_p = current(c->next, 0);
				/* waiting for all read/write request finished before 
				 * pos become >= N */
				Q_TRACE("wait: %p,%p,%p=>%p(%p,%p)\n", thread::current(), *ptr, p, next_p, c, c->next);
				while (*ptr != p) { ::sched_yield(); }
				if (rval) {
					/* here, we can assure all read request 
					 * to current chunk c and which index < (N - 1) 
					 * then we can dispose current chunk c with safe */
					tmp = c;
					Q_TRACE("%p,mrnext:%p,%p\n", thread::current(), c, c->next);
					c = c->next;
					queue::free(tmp);
					dump("mrnext");
				}
				else {
					/* write chunk also can go next */
					Q_TRACE("%p,mwnext:%p,%p\n", thread::current(), c, c->next);
					c = c->next;
					dump("mwnext");
				}
				/* now c ==(m_w) is completely available for reader, then we update m_wp.
				 * if we do it before c = c->next, greedy reader read c and dispose it to free chunk.
				 * so c->next value is changed. (bug) */
				Q_TRACE("%p,%p,%p=>%p(%p,%p)\n", thread::current(), *ptr, p, next_p, c, c->next);
				while (!__sync_bool_compare_and_swap(ptr, p, next_p)) {
					::sched_yield();
				}
				Q_TRACE("%p,done\n", thread::current());
				ATOMIC(pos = 0);
			}
			else {
				while (pos >= N) { ::sched_yield(); }
				goto retry;
			}
		}
		else {
			ASSERT(tmp_pos > 0);	/* should not be 0 */
			p = current(c, tmp_pos - 1); /* p is never be same among each thread */
			next_p = current(c, tmp_pos);
			if (rval) { *val = *p; }	/* read value */
			else { *p = *val; }			/* write value */
			while (!__sync_bool_compare_and_swap(ptr, p, next_p)) {
				::sched_yield();
			}
		}
		return p;
	}
	static inline T *current(struct chunk *c, int pos) {
		ASSERT(pos < N);
		return &(c->p[pos]);
	}
protected:
	bool readable() const { return m_rp != m_wp; }
	struct chunk *allocate() {
		struct chunk *c = reinterpret_cast<struct chunk *>(
			util::mem::alloc(sizeof(struct chunk) + sizeof(T) * N));
		if (!c) { return NULL; }
		return c;
	}
	void free(struct chunk *c, bool cache = true) {
		Q_TRACE("queue::free %p, %s\n", c, cache ? "cache" : "free");
		if (c == m_c) {
			m_mtx.lock();
			m_c = m_c->next;
			m_mtx.unlock();
		}
		if (cache) {
			m_mtx.lock();
			c->next = m_free;
			m_free = c;
			m_mtx.unlock();
		}
		else {
			util::mem::free(c);
		}
	}
	inline struct chunk *cache() {
		m_mtx.lock();
		if (!m_free) { 
			m_mtx.unlock();
			return NULL; 
		}
		struct chunk *tmp = m_free;
		m_free = m_free->next;
		m_mtx.unlock();
		return tmp;
	}
	bool expand() {
		struct chunk *c = cache();
		if (!c) { c = allocate(); }
		if (!c) { ASSERT(false); return false; }
		c->next = NULL;
		m_w->next = c;
		return true;
	}
};

}
}

#endif
