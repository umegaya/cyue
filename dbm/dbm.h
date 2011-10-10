/***************************************************************
 * dbm.h : local KVS (data base manager like QDBM)
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
#if !defined(__DBM_H__)
#define __DBM_H__

#include "impl.h"
#include "util.h"
#include <stddef.h>

namespace yue {
typedef module::dbm::_DBM impl;
class dbm : public impl {
public:
	struct interface : public impl {
		/* traits */
		template <class V> struct traits {
			static const void *p(V &v) { return (const void *)&v; }
			static int l(V &v) { return sizeof(V); }
		};
		template <typename V, int N> struct traits<V[N]> {
			static const void *p(V v[N]) { return (const void *)v; }
			static int l(V v[N]) { return N * sizeof(V); }
		};
		template <typename V> struct traits<V*> {
			static const void *p(V *&v) { return (const void *)v; }
			static int l(V *&v) { return sizeof(V); }
		};
		/* generic return value */
		struct genptr {
			void *m_p;
			genptr(void *p) : m_p(p) {}
			template <class T> T *as(ptrdiff_t ofs = 0) {
				ASSERT(m_p);
				return (T *)(((U8 *)m_p) + ofs); }
		};
		/* method */
		template <typename K>
		genptr fetch(K &k) {
			int l; 
			return genptr(reinterpret_cast<void*>(
				impl::fetch(traits<K>::p(k), traits<K>::l(k), l)));
		}
		template <typename K, typename R>
		int fetch(K &k, R &r) {
			int l;
			return impl::fetch(traits<K>::p(k), traits<K>::l(k),
				traits<R>::p(r), traits<R>::l(r));
		}
		template <typename V, typename K>
		bool replace(V &v, K &k) {
			return impl::replace(traits<V>::p(v), traits<V>::l(v),
				traits<K>::p(k), traits<K>::l(k));
		}
		template <typename V, typename K>
		bool put(V &v, K &k, bool &exist) {
			return impl::put(traits<V>::p(v), traits<V>::l(v),
				traits<K>::p(k), traits<K>::l(k), exist);
		}
		template <typename K>
		bool remove(K &k) {
			return impl::remove(traits<K>::p(k), traits<K>::l(k));
		}
	};
	interface &driver() { return 
		*(reinterpret_cast<interface*>(reinterpret_cast<impl *>(this))); }
	template <class C> class cache {
		C &m_c;
	public:
		typedef typename C::key key;
		inline cache(C &c) : m_c(c) {}
		inline void *fetch(key &k, int &l) { return m_c.fetch(k, l); }
		inline int put(key &k, void *p, int l) { return m_c.put(k, p, l); }
		inline int replace(key &k, void *p, int l) { return m_c.replace(k, p, l); }
		inline int remove(key &k) { return m_c.remove(k); }
		template <class VISITOR> int flush_dirty_cache(VISITOR &v, int max) {
			return m_c.visit_dirty_cache(v, max);
		}
	};
	/* commit cache data to disk */
	template <class T> int operator () (
		cache<T> &c, typename cache<T>::key &k) {
		int l; void *p = c.fetch(k, l);
		if (!p) { return NBR_ENOTFOUND; }
		return !impl::put(k, sizeof(typename cache<T>::key), p, l) ?
			NBR_ESYSCALL : NBR_OK;
	}
	template <class T> int flush(cache<T> &c, int max) {
		return c.flush_dirty_cache(*this, max);
	}
	template <class T> int put(cache<T> &c,
		typename cache<T>::key &k, void *p, int l) {
		int r; bool b;
		if ((r = c.put(k, p, l)) < 0) { return r; }
		if (!impl::put(k, sizeof(typename cache<T>::key), p, l, b)) {
			return b ? NBR_EALREADY : NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	template <class T> int replace(cache<T> &c,
		typename cache<T>::key &k, void *p, int l) {
		int r;
		if ((r = c.replace(k, p, l)) < 0) { return r; }
		if (!impl::replace(k, sizeof(typename cache<T>::key), p, l)) {
			return NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	template <class T> void *fetch(cache<T> &c, typename cache<T>::key &k, int &l) {
		int r; void *p;
		if ((p = c.fetch(k, sizeof(typename cache<T>::key)))) { return p; }
		if (!(p = impl::fetch(k, sizeof(typename cache<T>::key), l))) {
			return NBR_ENOTFOUND;
		}
		if ((r = c.put(k, p, l)) < 0) { return r; }
		impl::free(p);
		return c.select(k, l);
	}
};
/* explicit specialization */
template <> struct dbm::interface::traits<const char*> {
	static const void *p(const char *&v) { return (const void *)v; }
	static int l(const char *&v) { return util::str::length(v); }
};

template <int N> struct dbm::interface::traits<const char [N]> {
	static const void *p(const char v[N]) { return (const void *)v; }
	static int l(const char v[N]) { return N - 1; }
};


}


#endif
