/***************************************************************
 * functional.h : template utility for defining functional, acts like boost::functional
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 *
 * see license.text for license detail
 ****************************************************************/
#if !defined(__FUNCTIONAL_H__)
#define __FUNCTIONAL_H__

namespace yue {
namespace util {

union anyptr {
	void *obj;
	void (*fn)();
};

struct referer {
	template <class T> struct nop {
		static inline void refer(anyptr &a, T *p) { a.obj = p; }
		static inline void unref(anyptr &a) {}
	};
	template <class T> struct counter {
		static inline void refer(anyptr &a, T *p) { p->refer(); a.obj = p; }
		static inline void unref(anyptr &a) { if (a.obj) { reinterpret_cast<T *>(a.obj)->unref(); } }
	};
	template <class T> struct copy {
		static inline void refer(anyptr &a, T *p) { a.obj = new T(*p); }
		static inline void unref(anyptr &a) { if (a.obj) { delete reinterpret_cast<T *>(a.obj); }  }
	};
	static void nop_finalizer(anyptr&) {}
};

template <class F, template <class T> class REFER = referer::template nop>
class functional;

/* specialization */
template <typename R, template <class T> class REFER>
class functional<R (), REFER> {
	anyptr m_p;
	R (*m_fn)(anyptr&);
	void (*m_fzr)(anyptr&);
	template <class OBJ, template <class T> class REF>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { REF<OBJ>::refer(p, &o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p) { return get(p)(); }
		static inline void destroy(anyptr &p) { REF<OBJ>::unref(p); }
	};
	template <typename _R, template <class T> class REF>
	struct callee<_R (*)(), REF> {
		typedef _R (*type)();
		static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
		static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
		static inline R invoke(anyptr &p) { return get(p)(); }
		static inline void destroy(anyptr &a) {}
	};
public:
	inline functional() : m_fzr(referer::nop_finalizer) {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn), m_fzr(f.m_fzr) {
		const_cast<functional &>(f).m_p.obj = NULL;
	}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)()) { set(fn); }
	inline ~functional() { fin(); }
	inline void fin() { m_fzr(m_p); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR, REFER>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR, REFER>::invoke);
		m_fzr = &(callee<FUNCTOR, REFER>::destroy);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR, REFER>::type ref() {
		return callee<FUNCTOR, REFER>::get(m_p);
	}

	inline R operator () () { return m_fn(m_p); }
};

template <typename R, typename T0, template <class T> class REFER>
class functional<R (T0), REFER> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0);
	void (*m_fzr)(anyptr&);
	template <class OBJ, template <class T> class REF>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { REF<OBJ>::refer(p, &o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0) { return get(p)(t0); }
		static inline void destroy(anyptr &p) { REF<OBJ>::unref(p); }
	};
	template <typename _R, typename _T0, template <class T> class REF>
	struct callee<_R (*)(_T0), REF> {
		typedef _R (*type)(_T0);
		static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
		static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
		static inline R invoke(anyptr &p, T0 t0) { return get(p)(t0); }
		static inline void destroy(anyptr &) {}
	};
public:
	inline functional() : m_fzr(referer::nop_finalizer) {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn), m_fzr(f.m_fzr) {
		const_cast<functional &>(f).m_p.obj = NULL;
	}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0)) { set(fn); }
	inline ~functional() { fin(); }
	inline void fin() { m_fzr(m_p); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR, REFER>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR, REFER>::invoke);
		m_fzr = &(callee<FUNCTOR, REFER>::destroy);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR, REFER>::type ref() {
		return callee<FUNCTOR, REFER>::get(m_p);
	}
	inline R operator () (T0 t0) { return m_fn(m_p, t0); }
};

template <typename R, typename T0, typename T1, template <class T> class REFER>
class functional<R (T0, T1), REFER> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1);
	void (*m_fzr)(anyptr&);
	template <class OBJ, template <class T> class REF>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { REF<OBJ>::refer(p, &o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1) { return get(p)(t0, t1); }
		static inline void destroy(anyptr &p) { REF<OBJ>::unref(p); }
	};
	template <typename _R, typename _T0, typename _T1, template <class T> class REF>
	struct callee<_R (*)(_T0, _T1), REF> {
		typedef _R (*type)(_T0, _T1);
		static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
		static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1) { return get(p)(t0, t1); }
		static inline void destroy(anyptr &) {}
	};
public:
	inline functional() : m_fzr(referer::nop_finalizer) {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn), m_fzr(f.m_fzr) {
		const_cast<functional &>(f).m_p.obj = NULL;
	}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1)) { set(fn); }
	inline ~functional() { fin(); }
	inline void fin() { m_fzr(m_p); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR, REFER>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR, REFER>::invoke);
		m_fzr = &(callee<FUNCTOR, REFER>::destroy);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR, REFER>::type ref() {
		return callee<FUNCTOR, REFER>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1) { return m_fn(m_p, t0, t1); }
};

template <typename R, typename T0, typename T1, typename T2,
	template <class T> class REFER>
class functional<R (T0, T1, T2), REFER> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1,T2);
	void (*m_fzr)(anyptr&);
	template <class OBJ, template <class T> class REF>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, type o) { REF<OBJ>::refer(p, &o); }
		static inline OBJ &get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2) { return get(p)(t0, t1, t2); }
		static inline void destroy(anyptr &p) { REF<OBJ>::unref(p); }
	};
	template <typename _R, typename _T0, typename _T1, typename _T2,
		template <class T> class REF>
	struct callee<_R (*)(_T0, _T1, _T2), REF> {
		typedef _R (*type)(_T0, _T1, _T2);
		static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
		static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2) { return get(p)(t0, t1, t2); }
		static inline void destroy(anyptr &) {}
	};
public:
	inline functional() : m_fzr(referer::nop_finalizer) {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn), m_fzr(f.m_fzr) {
		const_cast<functional &>(f).m_p.obj = NULL;
	}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1, T2)) { set(fn); }
	inline ~functional() { fin(); }
	inline void fin() { m_fzr(m_p); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR, REFER>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR, REFER>::invoke);
		m_fzr = &(callee<FUNCTOR, REFER>::destroy);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR, REFER>::type ref() {
		return callee<FUNCTOR, REFER>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1, T2 t2) { return m_fn(m_p, t0, t1, t2); }
};

template <typename R, typename T0, typename T1, typename T2, typename T3,
	template <class T> class REFER>
class functional<R (T0, T1, T2, T3), REFER> {
	anyptr m_p;
	R (*m_fn)(anyptr&,T0,T1,T2,T3);
	void (*m_fzr)(anyptr&);
	template <class OBJ, template <class T> class REF>
	struct callee {
		typedef OBJ &type;
		static inline void set(anyptr &p, OBJ &o) { REF<OBJ>::refer(p, &o); }
		static inline type get(anyptr &p) { return *(reinterpret_cast<OBJ *>(p.obj)); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2, T3 t3) { return get(p)(t0, t1, t2, t3); }
		static inline void destroy(anyptr &p) { REF<OBJ>::unref(p); }
	};
	template <typename _R, typename _T0, typename _T1, typename _T2, typename _T3,
		template <class T> class REF>
	struct callee<_R (*)(_T0, _T1, _T2, _T3), REF> {
		typedef _R (*type)(_T0, _T1, _T2, _T3);
		static inline void set(anyptr &p, type f) { p.fn = reinterpret_cast<void (*)()>(f); }
		static inline type get(anyptr &p) { return reinterpret_cast<type>(p.fn); }
		static inline R invoke(anyptr &p, T0 t0, T1 t1, T2 t2, T3 t3) { return get(p)(t0, t1, t2, t3); }
		static inline void destroy(anyptr &) {}
	};
public:
	inline functional() : m_fzr(referer::nop_finalizer) {}
	inline functional(const functional &f) : m_p(f.m_p), m_fn(f.m_fn), m_fzr(f.m_fzr) {
		const_cast<functional &>(f).m_p.obj = NULL;
	}
	template <class FUNCTOR> inline functional(FUNCTOR &fn) { set(fn); }
	inline functional(R (*fn)(T0, T1, T2, T3)) { set(fn); }
	inline ~functional() { fin(); }
	inline void fin() { m_fzr(m_p); }
	template <class FUNCTOR> inline void set(FUNCTOR &fn) {
		callee<FUNCTOR, REFER>::set(m_p, fn);
		m_fn = &(callee<FUNCTOR, REFER>::invoke);
		m_fzr = &(callee<FUNCTOR, REFER>::destroy);
	}
	template <class FUNCTOR> inline typename callee<FUNCTOR, REFER>::type ref() {
		return callee<FUNCTOR, REFER>::get(m_p);
	}
	inline R operator () (T0 t0, T1 t1, T2 t2, T3 t3) { return m_fn(m_p, t0, t1, t2, t3); }
};
}
}
#endif
