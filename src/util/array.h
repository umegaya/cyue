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

namespace yue {
namespace util {
enum {
	opt_not_set	   = 0,
	opt_threadsafe = NBR_PRIM_THREADSAFE,
	opt_expandable = NBR_PRIM_EXPANDABLE,
};
template <class E>
class array {
public:
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
			void	*operator	new		(size_t, ARRAY a) { return nbr_array_alloc(a); }
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
			void	*operator	new		(size_t, ARRAY a) { return nbr_array_alloc(a); }
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
			void	*operator	new		(size_t, ARRAY a) { return nbr_array_alloc(a); }
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
	ARRAY	m_a;
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
	ARRAY get_a() { return m_a; }
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
		int opt/* = NBR_PRIM_EXPANDABLE */)
{
	if (size == -1) {
		size = sizeof(element);	/* default */
	}
	if (!(m_a = nbr_array_create(max, size, opt))) {
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
		nbr_array_destroy(m_a);
		m_a = NULL;
	}
}

template<class E> int
array<E>::use() const
{
	ASSERT(m_a >= 0);
	return nbr_array_use(m_a);
}

template<class E> int
array<E>::max() const
{
	ASSERT(m_a >= 0);
	return nbr_array_max(m_a);
}

template<class E> int
array<E>::size() const
{
	ASSERT(m_a >= 0);
	return nbr_array_get_size(m_a);
}

template<class E> typename array<E>::retval *
array<E>::alloc()
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element;
	return e ? e->get() : NULL;
}

template<class E>
template<class A0>
typename array<E>::retval *
array<E>::alloc(A0 a0)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element(a0);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element(a0, a1);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2, class A3>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2, A3 a3)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2, a3);
	return e ? e->get() : NULL;
}

template<class E>
template<class A0, class A1, class A2, class A3, class A4>
typename array<E>::retval *
array<E>::alloc(A0 a0, A1 a1, A2 a2, A3 a3, A4 a4)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a)	element(a0, a1, a2, a3, a4);
	return e ? e->get() : NULL;
}

template<class E> typename array<E>::retval *
array<E>::alloc(const value p)
{
	if (nbr_array_full(m_a)) { return NULL; }
	element *e = new(m_a) element(p);
	return e ? e->get() : NULL;
}

template<class E> void
array<E>::free(retval *v)
{
	element *e = element::to_e(v);
	ASSERT(nbr_array_get_index(get_a(), e) < nbr_array_max(get_a()));
	if (e) { nbr_array_free(m_a, e); }
}

template<class E> void
array<E>::free(element *e)
{
	ASSERT(nbr_array_get_index(get_a(), e) < nbr_array_max(get_a()));
	if (e) { nbr_array_free(m_a, e); }
}

template<class E> void
array<E>::erase(iterator p)
{
	if (p != end()) {
		p.m_e->fin();	/* call destructer only */
		nbr_array_free(m_a, p.m_e);	/* free its memory */
	}
	return;
}

template<class E> typename array<E>::iterator
array<E>::begin() const
{
	ASSERT(m_a);
	return iterator((element *)nbr_array_get_first(m_a));
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
	return iterator((element *)nbr_array_get_next(m_a, p.m_e));
}
}
}
#endif
