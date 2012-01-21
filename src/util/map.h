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

/* read/write lock macro */
#define ARRAY_READ_LOCK(__a, __ret) { \
	if (__a->lock_required() && pthread_rwlock_rdlock(__a->lock()) != 0) { return __ret; } }
#define ARRAY_READ_UNLOCK(__a) { pthread_rwlock_unlock(__a->lock()); }
#define ARRAY_WRITE_LOCK(__a, __ret) { \
	if (__a->lock_required() && pthread_rwlock_wrlock(__a->lock()) != 0) { return __ret; } }
#define ARRAY_WRITE_UNLOCK(__a) if (__a->lock_required()){ pthread_rwlock_unlock(__a->lock()); }

namespace yue {
namespace util {
class hash {
	/*-------------------------------------------------------------*/
	/* constant													   */
	/*-------------------------------------------------------------*/
	static const U32 BIG_PRIME = (16754389);

	typedef enum HUSH_KEY_TYPE {
		HKT_NONE = 0,
		HKT_INT,
		HKT_STR,
		HKT_MEM,
	} hush_key_type;



	/*-------------------------------------------------------------*/
	/* internal types											   */
	/*-------------------------------------------------------------*/
	typedef struct _hushelm {
		struct _hushelm	*m_next, *m_prev;
		union	{
			struct { int k;		}	integer;
			struct { char k[0];	}	string;
			struct { char k[0]; } 	mem;
		}	m_key;
	}	hushelm_t;
	typedef struct _generic_key {
		const char *key;
		int len;
	} generic_key;

	hush_key_type		m_type;
	hushelm_t			**m_table;
	U32					m_size, m_key_size, m_val_size;
	fix_size_allocator	*m_a;	/* for allocating new hushelm when hush collision occured */


protected:
	/*-------------------------------------------------------------*/
	/* internal method											   */
	/*-------------------------------------------------------------*/
	static inline int get_key_size(hush_key_type type, int keybuf_size)
	{
		int base = (sizeof(hushelm_t*) * 2) + sizeof(void *);
		switch(type) {
		case HKT_INT:
			return base + sizeof(int);
		case HKT_STR:
		case HKT_MEM:
			return base + (((0x03 + keybuf_size) >> 2) << 2);	/* string aligns by 4 byte */
		default:
			ASSERT(false);
			return 0;
		}
	}

	inline int get_keybuf_size()
	{
		ASSERT(m_key_size > ((sizeof(hushelm_t*) * 2) + sizeof(void *)));
		return m_key_size - ((sizeof(hushelm_t*) * 2) + sizeof(void *));
	}

	inline void *get_value_ptr(hushelm_t *e) {
		ASSERT((m_key_size % 4) == 0);
		return (((U8 *)e) + m_key_size);
	}

	inline hushelm_t *get_hushelm_from(void *p) {
		return ((hushelm_t *)(((U8 *)p) - m_key_size));
	}

	inline hushelm_t **get_hushelm(int index)
	{
		ASSERT(((size_t)index) < m_size);
		return (hushelm_t **)&(m_table[index]);
	}

	inline int get_hush(int key)
	{
		ASSERT(m_size > 0 && m_type == HKT_INT);
		return (key % m_size);
	}

	inline int cmp_key(hushelm_t *e, int key) {
		ASSERT(m_type == HKT_INT);
		return e->m_key.integer.k == key;
	}

	inline void set_key(hushelm_t *e, int key) {
		ASSERT(m_type == HKT_INT);
		e->m_key.integer.k = key;
	}

	inline int get_hush(const char *str)
	{
		ASSERT(m_type == HKT_STR);
		return util::math::pjw_hush(m_size, (U8 *)str);
	}

	inline int cmp_key(hushelm_t *e, const char *key) {
		ASSERT(m_type == HKT_STR);
		return util::str::cmp(e->m_key.string.k, key, m_key_size) == 0;
	}

	inline void set_key(hushelm_t *e, const char *key) {
		ASSERT(m_type == HKT_STR);
		util::str::copy(e->m_key.string.k, key, m_key_size);
	}

	inline int get_hush(const generic_key &k)
	{
		ASSERT(m_type == HKT_MEM);
		return util::math::MurmurHash2(k.key, k.len, BIG_PRIME) % m_size;
	}

	inline int cmp_key(hushelm_t *e, const generic_key &k) {
		ASSERT(m_type == HKT_MEM);
		return util::mem::cmp(e->m_key.mem.k, k.key, k.len) == 0;
	}

	inline void set_key(hushelm_t *e, const generic_key &k) {
		ASSERT(m_type == HKT_MEM);
		util::mem::copy(e->m_key.mem.k, k.key, 
			((size_t)k.len) < m_key_size ? ((size_t)k.len) : m_key_size);
	}
public:
	/*-------------------------------------------------------------*/
	/* external methods											   */
	/*-------------------------------------------------------------*/
	inline int init(hush_key_type type,
		int max_element, int table_size, int option, int keybuf_size, int value_size = sizeof(void *))
	{
		int prime = util::math::prime(table_size);
		if (prime < 0) {
			return NBR_EINVAL;
		}
		m_size = prime;
		m_type = type;
		m_key_size = get_key_size(type, keybuf_size);
		m_val_size = value_size;
		if (!(m_table = (hushelm_t **)util::mem::alloc(m_size * sizeof(hushelm_t*)))) {
			return NBR_EMALLOC;
		}
		if (!(m_a = fix_size_allocator::create(max_element, m_key_size + m_val_size, option))) {
			return NBR_EEXPIRE;
		}
		util::mem::bzero(m_table, m_size * sizeof(hushelm_t*));
		return 0;
	}

	inline bool initialized() const { return m_a != NULL; }
	fix_size_allocator *to_a() { return m_a; }

	template <class V, typename ARG>
	inline int iterate(int (*fn)(V*,ARG&), ARG &a) {
		int cnt = 0, r;
		for (void *p = m_a->first(); p; p = m_a->next(p)) {
			if ((r = fn(reinterpret_cast<V *>(p), a)) < 0) {
				return r;
			}
			cnt++;
		}
		return cnt;
	}


	inline int fin()
	{
		if (m_table) {
			util::mem::free(m_table);
		}
		if (m_a) {
			m_a->destroy();
		}
		util::mem::bzero(this, sizeof(*this));
		return 0;
	}

	inline void *begin() { 
		return get_value_ptr((hushelm_t *)m_a->first()); 
	}
	inline void *next(void *p) {
		return get_value_ptr((hushelm_t *)m_a->next(get_hushelm_from(p)));
	}

	template <class KEY>
	inline void *get(KEY key)
	{
		hushelm_t *e;
		ARRAY_READ_LOCK(m_a,NULL);
		e = *get_hushelm(get_hush(key));
		if (e == NULL) {
			ARRAY_READ_UNLOCK(m_a);
			return NULL;
		}
		while(e) {
			if (cmp_key(e, key)) {
				ARRAY_READ_UNLOCK(m_a);
				return get_value_ptr(e);
			}
			e = e->m_next;
		}
		ARRAY_READ_UNLOCK(m_a);
		return NULL;
	}

	struct ptr_insert_functor {
		void *p;
		inline void *operator () (void *data, bool) {
			*((void **)data) = p;
			return data;
		}
	};
	template <class KEY>
	inline int insert(KEY k, void *data) {
		ptr_insert_functor f = { data };
		return insert(k, f);
	}
	template <class KEY, class DATA_SET_FUNCTOR>
	inline void *insert(KEY k, DATA_SET_FUNCTOR f) {
		hushelm_t *tmp, **e; void *p;
		ARRAY_WRITE_LOCK(m_a,NULL);
		e = get_hushelm(get_hush(k));
		tmp = *e;
		while (tmp) {
			if (cmp_key(tmp, k)) { break; }
			tmp = tmp->m_next;
		}
		if (tmp) {
			return f(get_value_ptr(tmp), true);
		}
		else {
			tmp = reinterpret_cast<hushelm_t *>(m_a->alloc());
			if (tmp) {
				tmp->m_next = NULL;
				tmp->m_prev = NULL;
				set_key(tmp, k);
				p = f(get_value_ptr(tmp), false);
			}
			else {
				//SEARCH_ERROUT(ERROR,EXPIRE,"used: %d,%d",
				//	m_use, m_max);
				ARRAY_WRITE_UNLOCK(m_a);
				return NULL;
			}
			ASSERT(tmp != *e);
			tmp->m_next = *e;
			if (*e) {
				(*e)->m_prev = tmp;
			}
			*e = tmp;
		}
		ARRAY_WRITE_UNLOCK(m_a);
		return p;
	}

	struct nop_remove_functor {
		inline bool operator () (void *) { return true; }
	};
	template <class KEY>
	inline void *remove(KEY k) {
		nop_remove_functor nop;
		return remove(k, nop);
	}
	template <class KEY, class FUNCTOR>
	inline void *remove(KEY k, FUNCTOR f) {
		hushelm_t **pe, *e;
		ARRAY_WRITE_LOCK(m_a,NULL);
		pe = get_hushelm(get_hush(k));
		e = *pe;
		while(e) {
			if (cmp_key(e, k)) { break; }
			e = e->m_next;
		}
		if (!e) {
			ARRAY_WRITE_UNLOCK(m_a);
			return NULL;
		}
		/* if functor returns false, not remove from table
		 * but returns result pointer */
		if (!f(get_value_ptr(e))) { 
			return get_value_ptr(e); 
		}
		/* remove e from hush-collision chain */
		if (e->m_prev) {
			ASSERT(e->m_prev->m_next == e);
			e->m_prev->m_next = e->m_next;
		}
		else {
			*pe = e->m_next;
		}
		if (e->m_next) {
			ASSERT(e->m_next->m_prev == e);
			e->m_next->m_prev = e->m_prev;
		}
		/* free e into array m_a */
		m_a->free(e);
		ARRAY_WRITE_UNLOCK(m_a);
		return get_value_ptr(e);
	}


	#if defined(_DEBUG)
	bool sanity_check()
	{
		return m_a->sanity_check();
	}
	#endif
};

template <class V, typename K>
class map {
public:
	typedef V retval;
	/* specialization */
	enum { KT_NORMAL = 0, KT_PTR = 1, KT_INT = 2 };
	template <class C, typename T>
	struct 	kcont {
		typedef const T &type;
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_MEM, max, opt, hashsz, sizeof(T), sizeof(C));
		}
		static inline hash::generic_key key_for_hash(type t) {
			hash::generic_key k = {
				reinterpret_cast<const char *>(&t), sizeof(T)
			};
			return k;
		}
	};
	template <class C>
	struct 	kcont<C,const char*> {
		typedef const char *type;
		static inline const char *key_for_hash(type t) { return t; }
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_STR, max, opt, hashsz, 1024, sizeof(C));
		}
	};
	template <class C, typename T, size_t N>
	struct 	kcont<C,T[N]> {
		typedef const T type[N];
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_MEM, max, opt, hashsz, sizeof(T), sizeof(C));
		}
		static inline hash::generic_key key_for_hash(type t) {
			hash::generic_key k = {
				reinterpret_cast<const char *>(t), sizeof(T) * N
			};
			return k;
		}
	};
	template <class C, typename T>
	struct 	kcont<C,T*> {
		typedef const T *type;
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_MEM, max, opt, hashsz, sizeof(T), sizeof(C));
		}
		static inline hash::generic_key key_for_hash(type t) {
			hash::generic_key k = {
				reinterpret_cast<const char *>(t), sizeof(T)
			};
			return k;
		}
	};
	template <class C>
	struct	kcont<C,U32> {
		typedef U32 type;
		static inline U32 key_for_hash(type t) { return t; }
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_INT, max, opt, 
				hashsz, sizeof(type), sizeof(C));
		}
	};
	template <class C, size_t N>
	struct	kcont<C,char[N]> {
		typedef const char *type;
		static inline const char *key_for_hash(type t) { return t; }
		static int init(hash &h, int max, int opt, int hashsz) {
			return h.init(hash::HKT_STR, max, opt, hashsz, N, sizeof(C));
		}
	};
	template <class C, class T>
	struct 	vcont {
		struct _C : public C {
			_C(const C & c) : C(c) {}
			void *operator new (size_t, void *p) { return p; }
		};
		static inline C *create(void *memp, C *v) { 
			return new(memp) _C(*v);
		}
	};
	template <class C, class T>
	struct vcont<C*,T> {
		static inline C **create(void *memp, C **v) {
			*(reinterpret_cast<C**>(memp)) = *v;
			return reinterpret_cast<C**>(memp);
		}
	};
	typedef typename kcont<V,K>::type key;
	typedef kcont<V,K> key_traits;
	typedef vcont<V,K> val_traits; 
	struct check_removable_functor {
		inline bool operator () (void *p) {
			V *v = reinterpret_cast<V *>(p);
			return v->removable();
		}
	};
	struct insert_set_data_functor {
		V *val;
		inline void *operator () (void *p, bool exists) {
			if (!exists) { return val_traits::create(p, val); }
			else { return NULL; }
		}
	};
	struct alloc_set_data_functor {
		V *val;
		inline void *operator () (void *p, bool exists) {
			if (!exists) { return val_traits::create(p, val); }
			else {
				*(reinterpret_cast<V *>(p)) = *val;
				return p;
			}
		}
	};
	struct alloc_if_set_data_functor {
		bool *exist;
		inline void *operator () (void *p, bool exists) {
			if (exist) { *exist = exists; }
			return p;
		}
	};
protected:
	hash	m_s;
public:
	map();
	~map();
	inline bool	init(int max, int hsz, int size = -1, int opt = opt_expandable);
	inline void 	fin();
	template <typename ARG>
	inline int 	iterate(int (*fn)(V*,ARG&), ARG &a);
	inline V	*insert(V &v, key k);
	inline V	*alloc(V &v, key k);
	inline V	*alloc(key k, bool *exist = NULL);
	inline V	*find(key k);
	inline bool	find_and_erase(key k, V &v);
	inline bool	find_and_erase_if(key k, V &v);
	inline bool	erase(key k);
	inline bool	initialized() { return m_s.initialized(); }
	static inline V	*cast(void *p) { return (V *)p; }
	inline V 	*begin() { return cast(m_s.begin()); }
	inline V 	*next(V *v) { return cast(m_s.next(v)); }
private:
	map(const map &m);
};


template<class V, typename K>
map<V,K>::map() : m_s() {}

template<class V, typename K>
map<V,K>::~map()
{
	fin();
}

template<class V, typename K> bool
map<V,K>::init(int max, int hashsz, int size/* = -1 */,
				int opt/* = opt_expandable */)
{
	if (kcont<V,K>::init(m_s, max, opt, hashsz) < 0) {
		fin();
	}
	return initialized();
}

template <class V, typename K>
template <typename ARG>
inline int map<V,K>::iterate(int (*fn)(V*,ARG&), ARG &a) {
	return m_s.iterate<V,ARG>(fn, a);
}

template<class V, typename K> void
map<V,K>::fin()
{
	m_s.fin();
}

template<class V, typename K> 
V *map<V,K>::find(key k)
{
	return cast(m_s.get(kcont<V,K>::key_for_hash(k)));
}

template<class V, typename K>
bool map<V,K>::find_and_erase(key k, V &v)
{
	V *pv = cast(m_s.remove(kcont<V,K>::key_for_hash(k)));
	if (pv) { v = *pv; }
	return pv != NULL;
}
template<class V, typename K>
bool map<V,K>::find_and_erase_if(key k, V &v)
{
	check_removable_functor f;
	V *pv = cast(m_s.remove(kcont<V,K>::key_for_hash(k), f));
	if (pv) { v = *pv; }
	return pv != NULL;
}
template<class V, typename K>
bool map<V,K>::erase(key k)
{
	return m_s.remove(kcont<V,K>::key_for_hash(k)) != NULL;
}


template<class V, typename K>
V *map<V,K>::insert(V &v, key k)
{
	insert_set_data_functor f = { &v };
	return cast(m_s.insert(kcont<V,K>::key_for_hash(k), f));
}
template<class V, typename K>
V *map<V,K>::alloc(V &v, key k)
{
	alloc_set_data_functor f = { &v };
	return cast(m_s.insert(kcont<V,K>::key_for_hash(k), f));
}
template<class V, typename K>
V *map<V,K>::alloc(key k, bool *exist)
{
	alloc_if_set_data_functor f = { exist };
	return cast(m_s.insert(kcont<V,K>::key_for_hash(k), f));
}
}
}
#endif
