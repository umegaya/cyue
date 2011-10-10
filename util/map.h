/***************************************************************
 * map.h : map collection class
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
#if !defined(__MAP_H__)
#define __MAP_H__

#include "array.h"

namespace yue {
namespace util {
template <class V, typename K>
class map : 
	public array<V> {
public:
	typedef array<V> super;
	typedef typename super::iterator iterator;
	typedef typename super::value	value;
	typedef typename super::retval	retval;
	typedef typename super::element element;
	/* specialization */
	enum { KT_NORMAL = 0, KT_PTR = 1, KT_INT = 2 };
	template <class C, typename T>
	struct 	kcont {
		typedef const T &type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T));
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, kp(t), kl(t), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, kp(t), kl(t));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, kp(t), kl(t));
		}
		static inline const char *kp(type t) { return
			reinterpret_cast<const char *>(&t); }
		static inline int kl(type t) { return sizeof(T); }
	};
/*	template <class C>
	struct 	kcont<C,address> {
		typedef const address &type;
		static inline const char *kp(type t) {
			return reinterpret_cast<const char *>(t.a()); }
		static inline int kl(type t) { return t.len(); }
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, address::SIZE);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, kp(t), kl(t), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, kp(t), kl(t));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, kp(t), kl(t));
		}
	};*/
	template <class C, typename T, size_t N>
	struct 	kcont<C,T[N]> {
		typedef const T type[N];
		static inline const void *kp(type t) {
			return reinterpret_cast<const void *>(t); }
		static inline int kl(type t) { return sizeof(T) * N; }
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T) * N);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, kp(t), kl(t), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, kp(t), kl(t));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, kp(t), kl(t));
		}
	};
	template <class C, typename T>
	struct 	kcont<C,T*> {
		typedef const T *type;
		static inline const char *kp(type t) {
			return reinterpret_cast<const char *>(t); }
		static inline int kl(type t) { return sizeof(T); }
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T));
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, kp(t), kl(t), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, kp(t), kl(t));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, kp(t), kl(t));
		}
	};
	template <class C>
	struct	kcont<C,U32> {
		typedef U32 type;
		static inline const void *kp(type t) {
			return reinterpret_cast<const void *>(&t); }
		static int kl(type t) { return sizeof(U32); }
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_int_engine(max, opt, hashsz);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_int_regist(s, (int)t, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_int_unregist(s, (int)t);
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_int_get(s, (int)t);
		}
	};
	template <class C, size_t N>
	struct	kcont<C,char[N]> {
		typedef const char *type;
		static inline const void *kp(type t) {
			return reinterpret_cast<const void *>(t); }
		static inline int kl(type t) { return strlen(t); }
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_str_engine(max, opt, hashsz, N);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_str_regist(s, t, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_str_unregist(s, t);
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_str_get(s, t);
		}
	};
	typedef typename kcont<V,K>::type key;
	typedef kcont<V,K> key_traits;
protected:
	SEARCH	m_s;
	RWLOCK 	m_lk;
public:
	map();
	~map();
	inline bool		init(int max, int hsz, int size = -1, int opt = opt_expandable);
	inline void 	fin();
	inline retval	*insert(value v, key k);
	inline retval	*alloc(value v, key k);
	inline retval	*alloc(key k);
	inline retval	*find(key k) const;
	inline bool		find_and_erase(key k, value v);
	inline bool		erase_if(key k);
	inline void		erase(key k);
	inline bool		initialized() { return super::initialized() && m_s != NULL; }
#if defined(_DEBUG)
	SEARCH get_s() { return m_s; }
#endif
protected:
	inline element 	*rawalloc(value v, key k, bool nil_if_exist);
	inline void		rawerase(key k);
	inline bool		rawerase_if(key k);
private:
	map(const map &m);
};


template<class V, typename K>
map<V,K>::map()
: array<V>(), m_s(NULL), m_lk(NULL)
{}

template<class V, typename K>
map<V,K>::~map()
{
	fin();
}

template<class V, typename K> bool
map<V,K>::init(int max, int hashsz, int size/* = -1 */,
				int opt/* = opt_expandable */)
{
	if (array<V>::init(max, size, opt)) {
		m_s = kcont<V,K>::init(max, opt & (~(opt_threadsafe)), hashsz);
	}
	if (!m_s) {
		fin();
	}
	if (opt_threadsafe & opt) {
		m_lk = nbr_rwlock_create();
		if (!m_lk) { fin(); }
	}
	return super::m_a && m_s;
}

template<class V, typename K> void
map<V,K>::fin()
{
	if (m_s) {
		nbr_search_destroy(m_s);
		m_s = NULL;
	}
	if (m_lk) {
		nbr_rwlock_destroy(m_lk);
		m_lk = NULL;
	}
	array<V>::fin();
}

template<class V, typename K> 
typename map<V,K>::retval *map<V,K>::find(key k) const
{
	ASSERT(m_s);
	if (m_lk) { nbr_rwlock_rdlock(m_lk); }
	element *e = kcont<V,K>::get(m_s, k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return e ? e->get() : NULL;
}

template<class V, typename K>
bool map<V,K>::find_and_erase(key k, value v)
{
	ASSERT(m_s);
	if (m_lk) { nbr_rwlock_rdlock(m_lk); }
	element *e = kcont<V,K>::get(m_s, k);
	if (e) { v = *e->get(); }
	rawerase(k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return e != NULL;
}

template<class V, typename K>
typename map<V,K>::retval *map<V,K>::insert(value v, key k)
{
	element *e = rawalloc(v, k, true);
	return e ? e->get() : NULL;
}

template<class V, typename K>
typename map<V,K>::retval *map<V,K>::alloc(value v, key k)
{
	element *e = rawalloc(v, k, false);
	return e ? e->get() : NULL;
}

template<class V, typename K>
typename map<V,K>::retval *map<V,K>::alloc(key k)
{
	if (nbr_array_full(super::m_a)) {
		return NULL;	/* no mem */
	}
	int r;
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	element *a = kcont<V,K>::get(m_s, k);
	if (a) {
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		return a;
	}
	if (!(a = new(super::m_a) element)) {
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		return NULL;
	}
	if ((r = kcont<V,K>::regist(m_s, k, a)) < 0) {
		super::free(a);
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		return NULL;
	}
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return a->get();
}

template<class V, typename K> typename map<V,K>::element *
map<V,K>::rawalloc(value v, key k, bool nil_if_exist)
{
	if (nbr_array_full(super::m_a)) {
		return NULL;	/* no mem */
	}
	int r;
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	element *a = kcont<V,K>::get(m_s, k);
	if (a) {
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		a->set(v);
		return nil_if_exist ? NULL : a;
	}
	if (!(a = new(super::m_a) element(v))) {
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		return NULL;
	}
	TRACE("map::rawalloc: a = %p\n", a);
	if ((r = kcont<V,K>::regist(m_s, k, a)) < 0) {
		super::free(a);
		if (m_lk) { nbr_rwlock_unlock(m_lk); }
		return NULL;
	}
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return a;
}

template<class V, typename K> void
map<V,K>::erase(key k)
{
	ASSERT(m_s && super::m_a);
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	rawerase(k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
}

template<class V, typename K> bool
map<V,K>::erase_if(key k)
{
	ASSERT(m_s && super::m_a);
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	bool r = rawerase_if(k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return r;
}

template<class V, typename K> void
map<V,K>::rawerase(key k)
{
	ASSERT(m_s && super::m_a);
	element	*e = kcont<V,K>::get(m_s, k);
	kcont<V,K>::unregist(m_s, k);
	if (e) { super::erase(e); }
	else { TRACE("key not found\n"); }
}

template<class V, typename K> bool
map<V,K>::rawerase_if(key k)
{
	ASSERT(m_s && super::m_a);
	element	*e = kcont<V,K>::get(m_s, k);
	kcont<V,K>::unregist(m_s, k);
	if (e) {
		super::erase(e);
		return true;
	}
	else {
		TRACE("key not found\n");
		return false;
	}
}

}
}
#endif
