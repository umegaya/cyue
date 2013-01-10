/***************************************************************
 * shalloc.h : provide named memory which safely allocates from multi-thread
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail.
 ****************************************************************/
#if !defined(__SHALLOC_H__)
#define __SHALLOC_H__

#include "map.h"

namespace yue {
namespace util {
namespace pattern {
template <class ELEMENT, typename KEY>
class shared_allocator {
public:
	struct element : public ELEMENT {
		U32 m_refc;
		element() : ELEMENT(), m_refc(0) {}
		inline int init() { return ELEMENT::init(); }
		inline void fin() { ELEMENT::fin(); }
		inline int check(int n_chk) { return ELEMENT::check(n_chk); }
		inline void refer() { __sync_add_and_fetch(&m_refc, 1); }
		inline bool unref() {
			if (__sync_add_and_fetch(&m_refc, -1) <= 0) {
				fin();
				return true;
			}
			return false;
		}
		static int initializer(element *e) { return e->init(); }
		static int checker(element *e, int n_chk) { return e->check(n_chk); }
	};
protected:
	map<element, KEY> m_pool;
public:
	inline map<element, KEY> &pool() { return m_pool; }
	inline ELEMENT *alloc(KEY k) {
		element *e = m_pool.alloc_and_init(k, element::initializer, element::checker);
		if (e) { e->refer(); }
		return e;
	}
	template <class INITIALIZER>
	inline ELEMENT *alloc(KEY k, INITIALIZER &izr) {
		element *e = m_pool.alloc_and_init(k, izr, element::checker);
		if (e) { e->refer(); }
		return e;
	}
	template <class INITIALIZER, class CHECKER>
	inline ELEMENT *alloc(KEY k, INITIALIZER &izr, CHECKER &chk) {
		element *e = m_pool.alloc_and_init(k, izr, chk);
		if (e) { e->refer(); }
		return e;
	}
	inline void free(ELEMENT *e) {
		ASSERT(e);
		element *tmp = reinterpret_cast<element *>(e);
		if (tmp->unref()) {
			m_pool.erase(*tmp);
		}
	}
	int init(int size_hint) {
		return m_pool.init(size_hint, size_hint, -1, opt_threadsafe | opt_expandable) ? NBR_OK : NBR_EMALLOC;
	}
	void fin() {
		m_pool.fin();
	}
};
}
}
}

#endif
