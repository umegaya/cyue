/***************************************************************
 * functional.h : template utility to define some kind of closure
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
#if !defined(__FUNCTIONAL_H__)
#define __FUNCTIONAL_H__

namespace yue {
namespace util {

union anyptr {
	void *obj;
	void (*fn)();
};

template <class F>
class functional;

/* specialization */
template <typename R>
class functional<R ()> {
	anyptr m_p;
	R (*m_fn)(anyptr&);
	template <class OBJ>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { p.obj = reinterpret_cast<void *>(&o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p) { return get(p)(); }
	};
public:
	inline functional() {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn) {}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)()) { set(fn); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR>::invoke);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR>::type ref() {
		return callee<FUNCTOR>::get(m_p);
	}
	inline R operator () () { return m_fn(m_p); }
};
template <typename R>
struct functional<R ()>::callee<R (*)()> {
	typedef R (*type)();
	static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
	static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
	static inline R invoke(anyptr &p) { return get(p)(); }
};

template <typename R, typename T0>
class functional<R (T0)> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0);
	template <class OBJ>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { p.obj = reinterpret_cast<void *>(&o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0) { return get(p)(t0); }
	};
public:
	inline functional() {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn) {}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0)) { set(fn); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR>::invoke);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR>::type ref() {
		return callee<FUNCTOR>::get(m_p);
	}
	inline R operator () (T0 t0) { return m_fn(m_p, t0); }
};
template <typename R, typename T0>
struct functional<R (T0)>::callee<R (*)(T0)> {
	typedef R (*type)(T0);
	static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
	static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
	static inline R invoke(anyptr &p, T0 t0) { return get(p)(t0); }
};

template <typename R, typename T0, typename T1>
class functional<R (T0, T1)> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1);
	template <class OBJ>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { p.obj = reinterpret_cast<void *>(&o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1) { return get(p)(t0, t1); }
	};
public:
	inline functional() {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn) {}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1)) { set(fn); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR>::invoke);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR>::type ref() {
		return callee<FUNCTOR>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1) { return m_fn(m_p, t0, t1); }
};
template <typename R, typename T0, typename T1>
struct functional<R (T0, T1)>::callee<R (*)(T0, T1)> {
	typedef R (*type)(T0, T1);
	static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
	static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
	static inline R invoke(anyptr &p, T0 t0, T1 t1) { return get(p)(t0, t1); }
};

template <typename R, typename T0, typename T1, typename T2>
class functional<R (T0, T1, T2)> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1,T2);
	template <class OBJ>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { p.obj = reinterpret_cast<void *>(&o); }
		static inline OBJ &get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2) { return get(p)(t0, t1, t2); }
	};
public:
	inline functional() {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn) {}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1, T2)) { set(fn); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR>::invoke);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR>::type ref() {
		return callee<FUNCTOR>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1, T2 t2) { return m_fn(m_p, t0, t1, t2); }
};
template <typename R, typename T0, typename T1, typename T2>
struct functional<R (T0, T1, T2)>::callee<R (*)(T0, T1, T2)> {
	typedef R (*type)(T0, T1, T2);
	static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
	static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
	static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2) { return get(p)(t0, t1, t2); }
};

template <typename R, typename T0, typename T1, typename T2, typename T3>
class functional<R (T0, T1, T2, T3)> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1,T2,T3);
	template <class OBJ>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, OBJ &o) { p.obj = reinterpret_cast<void *>(&o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2, T3 t3) { return get(p)(t0, t1, t2, t3); }
	};
public:
	inline functional() {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn) {}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1, T2, T3)) { set(fn); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR>::invoke);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR>::type ref() {
		return callee<FUNCTOR>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1, T2 t2, T3 t3) { return m_fn(m_p, t0, t1, t2); }
};
template <typename R, typename T0, typename T1, typename T2, typename T3>
struct functional<R (T0, T1, T2, T3)>::callee<R (*)(T0, T1, T2, T3)> {
	typedef R (*type)(T0, T1, T2, T3);
	static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
	static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
	static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2, T3 t3) { return get(p)(t0, t1, t2, t3); }
};
}
}
#endif
