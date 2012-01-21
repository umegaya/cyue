/***************************************************************
 * array.h : array collection class
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
#if !defined(__ARRAY_H__)
#define __ARRAY_H__

#include "common.h"
#include "util.h"
#include <pthread.h>

/* read/write lock macro */
#define ARRAY_READ_LOCK(__a, __ret) { \
	if (__a->lock_required() && pthread_rwlock_rdlock(__a->lock()) != 0) { return __ret; } }
#define ARRAY_READ_UNLOCK(__a) { pthread_rwlock_unlock(__a->lock()); }
#define ARRAY_WRITE_LOCK(__a, __ret) { \
	if (__a->lock_required() && pthread_rwlock_wrlock(__a->lock()) != 0) { return __ret; } }
#define ARRAY_WRITE_UNLOCK(__a) if (__a->lock_required()){ pthread_rwlock_unlock(__a->lock()); }

namespace yue {
namespace util {
enum {
	opt_not_set	   = 0,
	opt_threadsafe = 1 << 0,
	opt_expandable = 1 << 1,
};
class fix_size_allocator {
	/*-------------------------------------------------------------*/
	/* constant													   */
	/*-------------------------------------------------------------*/
	typedef enum eELEMFLAG {
		elem_used			= 1 << 0,
		elem_from_heap		= 1 << 1,
	} ELEMFLAG;

	/*-------------------------------------------------------------*/
	/* internal types											   */
	/*-------------------------------------------------------------*/
	typedef struct _element_t
	{
		_element_t	*m_prev;
		_element_t	*m_next;
		U32			m_flag;
		U8			m_data[0];
		/* element_t related */
		inline int inuse() const
		{
			return (m_flag & elem_used);
		}

		inline void *get_data()
		{
			return (m_data);
		}

		inline void set_flag(int on, U32 f)
		{
			if (on) { m_flag |= f; }
			else 	{ m_flag &= ~(f); }
		}

		inline bool get_flag(U32 f) const
		{
			return (m_flag & f);
		}

		static inline int get_size(size_t s)
		{
			return (sizeof(element_t) + s);
		}
	} element_t;

	/* global ary info */
	/* array data structure */
	/* first->....->last->....->end->endptr(&g_end) */
	/* ALLOC elem:
		first->..->prev->last->next->...->end->endptr
		first->..->prev->(prev->next (this pointer is allocated))
		->last->next (thus last ptr value will be last->next)
	*/
	/* FREE elem:
		first->..->elem->...->last->..->end->endptr
	=> first->..->last->..->end->elem->endptr and end=elem
	(elem is removed from chain and add as a last element)
	*/
	element_t	*m_used;	/* first fullfilled list element */
	element_t	*m_free;	/* first empty list element */
	U32			m_max;	/* max number of allocatable element */
	U32			m_use;	/* number of element in use */
	U32			m_size;	/* size of each allocated chunk */
	U32			m_option;	/* behavior option */
	pthread_rwlock_t m_lock;	/* for multi thread use */
protected:
	/*-------------------------------------------------------------*/
	/* internal methods											   */
	/*-------------------------------------------------------------*/
#if defined(_DEBUG)
	inline void array_dump()
	{
		element_t *e;
		int i = 0;
		TRACE( "arydump: %u %u/%u\n", m_size, m_use, m_max);
		TRACE( "used = %p, free = %p\n", m_used, m_free );
		TRACE( "dump inside...\n" );
		e = m_used;
		while(e) {
			TRACE( "used[%u]: %p,%s (%p<->%p)\n",
				i, e, e->m_flag ? "use" : "empty", e->m_prev, e->m_next );
			i++;
			e = e->m_next;
		}
		i = 0;
		e = m_free;
		while(e) {
			TRACE( "free[%u]: %p,%s (%p<->%p)\n",
				i, e, e->m_flag ? "use" : "empty", e->m_prev, e->m_next );
			i++;
			e = e->m_next;
		}
	}

	inline int count_usenum()
	{
		/* count number of element inuse with 2 way */
		size_t c1 = 0, c2 = 0;
		element_t *e = m_used;
		while(e) {
			c1++;
			e = e->m_next;
		}
		e = m_free;
		while(e) {
			c2++;
			e = e->m_next;
		}
		/* if count differ, something strange must occur */
		if (!((m_max == (c1 + c2)) && (m_use == c1))) {
			TRACE( "illegal count: %u %u %u %u\n", m_max, m_use, (int)c1, (int)c2);
			//array_dump(a);
			ASSERT(false);
		}
		return (m_max == (c1 + c2)) && (m_use == c1);
	}
#else
	#define array_dump()
	#define count_usenum()
#endif
	/* array_t related */
	static inline int get_alloc_size(int max, size_t s)
	{
		return (sizeof(fix_size_allocator) + (element_t::get_size(s) * max));
	}

	inline void *get_top()
	{
		return (void *)(this + 1);
	}

	inline bool check_align(const element_t *e)
	{
		ASSERT(m_size > 0);
		return ((
			(((U8*)e) - ((U8*)get_top()))
			%
			element_t::get_size(m_size)
			) == 0);
	}

	inline size_t get_index(const element_t *e)
	{
		if (check_align(e)) {
			return (
				(((U8*)e) - ((U8*)get_top()))
				/
				element_t::get_size(m_size)
				);
		}
		return -1;
	}

	inline element_t *get_from_index(int index)
	{
		if (m_max > ((size_t)index)) {
			return ((element_t *)(((U8 *)get_top()) + 
				index * element_t::get_size(m_size)));
		}
		return NULL;
	}

	static inline int get_data_ofs()
	{
		element_t e;
		return (int)(sizeof(e.m_prev) + sizeof(e.m_next) + sizeof(e.m_flag));
	}

	static inline element_t *get_top_address(const void *p)
	{
		return (element_t *)(((U8*)p) - ((size_t)get_data_ofs()));
	}

	inline bool check_address(const element_t *e)
	{
		size_t	idx = get_index(e);
		return idx >= 0 ? (idx < m_max) : false;
	}

	inline void
	set_data(element_t *e, const void *data, size_t sz)
	{
		util::mem::copy(e->m_data, data, m_size > sz ? sz : m_size);
	}

	inline int
	init_header(int mx, size_t s, U32 opt)
	{
		size_t i;
		element_t *e, *ep;

		util::mem::bzero(this, get_alloc_size(mx, s));
		m_size = s;
		m_use = 0;
		m_max = mx;
		m_option = opt;
		m_used = NULL;
		m_free = (element_t *)get_top();
		ep = e = m_free;
		ASSERT(ep);
		ASSERT(!ep->m_prev);
		for (i = 1; i < m_max; i++) {
			e = get_from_index(i);
			ep->m_next = e;
			ep = e;
		}
		e->m_next = NULL;
		if (lock_required()) {
			if (0 != pthread_rwlock_init(&m_lock, NULL)) {
				return NBR_EPTHREAD;
			}
		}
		count_usenum();
		return NBR_OK;
	}

	inline element_t *alloc_elm()
	{
		element_t *e;
		if (!m_free) {
			if (m_option & opt_expandable) {
				if (!(e = (element_t *)util::mem::calloc(1, 
					element_t::get_size(m_size)))) {
					return NULL;
				}
				e->set_flag(1, elem_from_heap);
				TRACE("array: alloc from heap %p\n", e);
				m_max++;
			}
			else { return NULL; }
		}
		else {
			e = m_free;
			m_free = e->m_next;
			//TRACE("array: alloc from freepool: %p->%p\n", e, e->next);
		}
		e->m_prev = NULL;
		e->m_next = m_used;
		ASSERT(!(m_used) || !(m_used->m_prev));
		if (e->m_next) { e->m_next->m_prev = e; }
		m_used = e;
		return e;
	}

	inline void free_elm(element_t *e)
	{
		ASSERT(check_address(e) || e->get_flag(elem_from_heap));
		if (m_used == e) {		/* first used elem */
			ASSERT(!e->m_prev);
			ASSERT(!e->m_next || (e->m_next->m_prev == e));
			if (e->m_next) { e->m_next->m_prev = NULL; }
			m_used = e->m_next;
			ASSERT(!(m_used) || !(m_used->m_prev));
		}
		else if (!e->m_next) {	/* last used elem */
			ASSERT(e->m_prev);
			e->m_prev->m_next = NULL;
			ASSERT(!(m_used) || !(m_used->m_prev));
		}
		else {
			ASSERT(e->m_prev && e->m_next);
			e->m_prev->m_next = e->m_next;
			e->m_next->m_prev = e->m_prev;
			ASSERT(!(m_used) || !(m_used->m_prev));
		}
		if (e->get_flag(elem_from_heap)) {
			if ((m_max / 2) >= m_use) {
				util::mem::free(e);
				m_max--;
				return;
			}
		}
		e->m_prev = NULL;
		e->m_next = m_free;
		m_free = e;
		e->set_flag(0, elem_used);
		ASSERT(!(m_used) || !(m_used->m_prev));
	}
public:
	/*-------------------------------------------------------------*/
	/* external methods											   */
	/*-------------------------------------------------------------*/
	inline void *alloc()
	{
		element_t *e;
		ARRAY_WRITE_LOCK(this,NULL);
		e = alloc_elm();
		if (e) {
	//		TRACE( "alloc: data=0x%08x\n", array_get_data(e) );
			ASSERT(e->get_data());
			e->set_flag(1, elem_used);
			m_use++;
			ASSERT(m_max >= m_use || count_usenum());
			ARRAY_WRITE_UNLOCK(this);
			return e->get_data();
		}
		ARRAY_WRITE_UNLOCK(this);
		return NULL;
	}

	inline int free(void *p)
	{
		element_t *e = get_top_address(p);
		//TRACE( "free: p=0x%08x, 0x%08x, %s, top=0x%08x, elm=0x%08x\n", p, e,
		//		element_is_inuse(e) ? "use" : "empty", array_get_top(a), array_get_top_address(p));
		//, array_get_elm_size(a->size), sizeof(element_t), sizeof(a->first->data) );
		if (!e->inuse()) {
			//array_dump(a);
			ASSERT(false);
			return NBR_EALREADY;
		}
		if (e->get_flag(elem_from_heap) || check_address(e)) {
			ARRAY_WRITE_LOCK(this,NBR_EPTHREAD);
			free_elm(e);
			m_use--;
			ASSERT((m_use >= 0 && m_max >= m_use) || count_usenum());
			ARRAY_WRITE_UNLOCK(this);
			return NBR_OK;
		}
		array_dump();
		ASSERT(false);
		return NBR_EINVAL;
	}

	inline bool lock_required() const { return (m_option & opt_threadsafe); }
	inline pthread_rwlock_t *lock() { return &m_lock; }

	inline void *first()
	{
		void *p;
		ARRAY_READ_LOCK(this,NULL);
		ASSERT(!m_used || (m_used && !m_used->m_prev));
		p = m_used ? m_used->get_data() : NULL;
		ARRAY_READ_UNLOCK(this);
		return p;
	}

	inline void *next(void *p)
	{
		void *ptr;
		element_t *e;
		ARRAY_READ_LOCK(this,NULL);
		e = get_top_address(p);
		ptr = e->m_next ? e->m_next->get_data() : NULL;
		ARRAY_READ_UNLOCK(this);
		return ptr;
	}

	inline int get_index(void *p)
	{
		element_t *e;
		if (m_option & opt_expandable) { return NBR_EINVAL; }
		e = get_top_address(p);
		return get_index(e);
	}
	static inline fix_size_allocator *create(int max, int size, int option)
	{
		int n_size = fix_size_allocator::get_alloc_size(max, size);
		fix_size_allocator *a = 
			(fix_size_allocator *)util::mem::alloc(n_size);
		if (a == NULL) {
			return NULL;
		}
		util::mem::bzero(a, n_size);
		if (a->init_header(max, size, option) != NBR_OK) {
			return NULL;
		}
		return a;
	}
	inline void destroy()
	{
		element_t *e, *pe;
		if (lock_required()) {
			pthread_rwlock_destroy(&m_lock);
		}
		m_use = 0;	/* force free heap elem in array_free_elm */
		e = m_used;
		while ((pe = e)) {
			e = e->m_next;
			if (pe->get_flag(elem_from_heap)) {
				util::mem::free(pe);
			}
		}
		e = m_free;
		while ((pe = e)) {
			ASSERT(e->get_flag(elem_from_heap) || check_align(e));
			e = e->m_next;
			if (pe->get_flag(elem_from_heap)) {
				util::mem::free(pe);
			}
		}
		util::mem::free(this);
	}
	inline int max() const { return m_max; }
	inline int use() const { return m_use; }
	inline int size() const { return m_size; }
	inline bool full() const { return max() <= use(); }

#if defined(_DEBUG)
	bool sanity_check()
	{
		element_t *e = m_used, *pe;
		while ((pe = e)) {
			if (!e->get_flag(elem_from_heap) && !check_align(e)) {
				return false;
			}
			e = e->m_next;
		}
		e = m_free;
		while ((pe = e)) {
			if (!e->get_flag(elem_from_heap) && !check_align(e)) {
				return false;
			}
			e = e->m_next;
		}
		return true;
	}
#endif
#undef ARRAY_READ_LOCK
#undef ARRAY_WRITE_LOCK
#undef ARRAY_READ_UNLOCK
#undef ARRAY_WRITE_UNLOCK
};
#define ARRAY_SCAN(__a, __p) 	\
	for (__p = __a->first(); __p; __p = __a->next(__p) )


template <class E>
class array {
public:
	typedef fix_size_allocator allocator;
	/* specialization */
	template<typename T>
	struct 	vcont {
		typedef T &value;
		typedef T retval;
		class	element : public T {
		public:
			element() : T() {}
			element(const value v) : T(v) {}
			template <class A0>
			element(A0 a0) : T(a0) {}
			template <class A0, class A1>
			element(A0 a0, A1 a1) : T(a0, a1) {}
			template <class A0, class A1, class A2>
			element(A0 a0, A1 a1, A2 a2) : T(a0, a1, a2) {}
			template <class A0, class A1, class A2, class A3>
			element(A0 a0, A1 a1, A2 a2, A3 a3) : T(a0, a1, a2, a3) {}
			template <class A0, class A1, class A2, class A3, class A4>
			element(A0 a0, A1 a1, A2 a2, A3 a3, A4 a4) : T(a0, a1, a2, a3, a4) {}
			template <class A0, class A1, class A2, class A3, class A4, class A5>
			element(A0 a0, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) : T(a0, a1, a2, a3, a4, a5) {}
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, allocator *a) { return a->alloc(); }
			void	operator	delete	(void*) {}
			void	set(value v) { ((value)*this) = v; }
			T	*get() { return this; }
			static class element *to_e(T *v) { return (class element *)v; }
		};
	};
	template<typename T>
	struct 	vcont<T*> {
		typedef T *value;
		typedef T retval;
		class	element {
		public:
			T	*data;
		public:
			element() : data(NULL) {}
			element(const value v) { set(v); }
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, allocator *a) { return a->alloc(); }
			void	operator	delete	(void*) {}
			void	set(value v) { data = v; }
			T	*get() { return data; }
			static class element *to_e(T *v) { ASSERT(false); return NULL; }
		};
	};
	template<typename T, size_t N>
	struct 	vcont<T[N]> {
		typedef T *value;
		typedef T retval;
		class	element {
		public:
			T	data[N];
		public:
			element() {}
			element(const value v) { set(v); }
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, allocator *a) { return a->alloc(); }
			void	operator	delete	(void*) {}
			void	set(const value v) {
				for (int i = 0; i < N; i++) { data[i] = v[i]; }
			}
			T	*get() { return data; }
			static class element *to_e(T *v) { return (class element *)v; }
		};
	};
	typedef typename vcont<E>::element	element;
	typedef typename vcont<E>::value 	value;
	typedef typename vcont<E>::retval	retval;
	class iterator {
	public:
		element *m_e;
	public:
		iterator() : m_e(NULL) {}
		iterator(element *e) : m_e(e) {}
		inline retval& operator * () { return *(m_e->get()); }
		inline retval* operator-> () { return m_e->get(); }
		inline bool operator == (const iterator &i) const { return m_e == i.m_e; }
		inline bool operator != (const iterator &i) const { return m_e != i.m_e; }
	};
protected:
	allocator *m_a;
public:
	array();
	~array();
	inline bool		init(int max, int size = -1, int opt = opt_expandable);
	inline void 	fin();
	inline retval	*alloc();
	template <class A0>
	retval *alloc(A0 a0);
	template <class A0, class A1>
	retval *alloc(A0 a0, A1 a1);
	template <class A0, class A1, class A2>
	retval *alloc(A0 a0, A1 a1, A2 a2);
	template <class A0, class A1, class A2, class A3>
	retval *alloc(A0 a0, A1 a1, A2 a2, A3 a3);
	template <class A0, class A1, class A2, class A3, class A4>
	retval *alloc(A0 a0, A1 a1, A2 a2, A3 a3, A4 a4);
	retval *alloc(const value t);
	inline void		insert(iterator p, const value t) { p->set(t); }
	inline void 	erase(iterator p);
	inline void		free(retval *v);
	inline void		free(element *e);
	inline int		size() const;
	inline int		use() const;
	inline int		max() const;
public:
	inline iterator	begin() const;
	inline iterator	end() const;
	inline iterator	next(iterator p) const;
	inline bool		initialized() const { return m_a != NULL; }
	template <class FUNC, typename ARG>
	inline int iterate(FUNC &fn, ARG &a) {
		int r, cnt = 0;
		iterator it = begin(), pit;
		for (; it != end(); ) {
			pit = it;
			it = next(it);
			if ((r = fn(&(*pit), a)) < 0) { return r; }
			cnt++;
		}
		return cnt;
	}
	template <typename ARG>
	inline int iterate(int (*fn)(E*,ARG&), ARG &a);
#if defined(_DEBUG)
	fix_size_allocator *get_a() { return m_a; }
#endif
private:
	array(const array &a);
};

template <class E>
template <typename ARG>
inline int array<E>::iterate(int (*fn)(E*,ARG&), ARG &a) {
	int r, cnt = 0;
	iterator it = begin(), pit;
	for (; it != end(); ) {
		pit = it;
		it = next(it);
		if ((r = fn(&(*pit), a)) < 0) { return r; }
		cnt++;
	}
	return cnt;
}


template<class E>
array<E>::array()
{
	m_a = NULL;
}

template<class E>
array<E>::~array()
{
	fin();
}

template<class E> bool
array<E>::init(int max, int size/* = -1 */,
		int opt/* = opt_expandable */)
{
	if (size == -1) {
		size = sizeof(element);	/* default */
	}
	if (!(m_a = allocator::create(max, size, opt))) {
		fin();
	}
	return (m_a != NULL);
}

template<class E> void
array<E>::fin()
{
	if (m_a) {
		void *p;
		ARRAY_SCAN(m_a, p) {
			/* call destructor */
			((element *)p)->fin();
		}
		m_a->destroy();
		m_a = NULL;
	}
}

template<class E> int
array<E>::use() const
{
	return m_a->use();
}

template<class E> int
array<E>::max() const
{
	return m_a->max();
}

template<class E> int
array<E>::size() const
{
	return m_a->size();
}

template<class E> typename array<E>::retval *
array<E>::alloc()
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element;
	return e ? e->get() : NULL;
}

template<class E>
template<class A0>
typename array<E>::retval *
array<E>::alloc(A0 a0)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element(a0);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element(a0, a1);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2, class A3>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2, A3 a3)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2, a3);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2, class A3, class A4>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2, A3 a3, A4 a4)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2, a3, a4);
	return e ? e->get() : NULL;
}

template<class E> typename array<E>::retval *
array<E>::alloc(const value p)
{
	if (m_a->full()) { return NULL; }
	element *e = new(m_a) element(p);
	return e ? e->get() : NULL;
}

template<class E> void
array<E>::free(retval *v)
{
	element *e = element::to_e(v);
	ASSERT(m_a->get_index(e) < m_a->max());
	if (e) {
		e->fin();
		m_a->free(e);
	}
}

template<class E> void
array<E>::free(element *e)
{
	ASSERT(m_a->get_index(e) < m_a->max());
	if (e) {
		e->fin();
		m_a->free(e);
	}
}

template<class E> void
array<E>::erase(iterator p)
{
	if (p != end()) {
		p.m_e->fin();	/* call destructer only */
		m_a->free(p.m_e);	/* free its memory */
	}
	return;
}

template<class E> typename array<E>::iterator
array<E>::begin() const
{
	ASSERT(m_a);
	return iterator((element *)m_a->first());
}

template<class E> typename array<E>::iterator
array<E>::end() const
{
	return iterator();
}

template<class E> typename array<E>::iterator
array<E>::next(iterator p) const
{
	ASSERT(m_a);
	return iterator((element *)m_a->next(p.m_e));
}
}
}
#endif
