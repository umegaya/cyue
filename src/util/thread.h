/***************************************************************
 * thread.h : thread pool / synchronize objects
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
#if !defined(__THREAD_H__)
#define __THREAD_H__

#include <pthread.h>
#include <sys/time.h>
#include "osdep.h"
#include "array.h"
#include "types.h"

namespace yue {
namespace util {

class thread {
public:
	class mutex {
	protected:
		pthread_mutex_t mtx;
	public:
		mutex() {}
		~mutex() { fin(); }
		operator pthread_mutex_t *() { return &mtx; }
		inline int init() {
			return (0 != pthread_mutex_init(&mtx, NULL)) ? NBR_EPTHREAD : NBR_OK;
		}
		inline void fin() {
			pthread_mutex_destroy(&mtx);
		}
		inline int lock() {
			return (0 != pthread_mutex_lock(&mtx)) ? NBR_EPTHREAD : NBR_OK;
		}
		inline bool try_lock() {
			return (0 != pthread_mutex_trylock(&mtx));
		}
		inline int unlock() {
			return (0 != pthread_mutex_unlock(&mtx)) ? NBR_EPTHREAD : NBR_OK;
		}
	};
	class rwlock {
	protected:
		pthread_rwlock_t rwl;
	public:
		rwlock() {}
		operator pthread_rwlock_t *() { return &rwl; }
		inline int init() {
			return (0 != pthread_rwlock_init(&rwl, NULL)) ? NBR_EPTHREAD : NBR_OK;
		}
		inline void fin() {
			pthread_rwlock_destroy(&rwl);
		}
		inline int lock() { return wrlock(); }
		inline bool try_lock() { return try_wrlock(); }
		inline int rdlock() {
			return (0 != pthread_rwlock_rdlock(&rwl)) ? NBR_EPTHREAD : NBR_OK;
		}
		inline bool try_rdlock() {
			return (0 != pthread_rwlock_tryrdlock(&rwl));
		}
		inline int wrlock() {
			return (0 != pthread_rwlock_wrlock(&rwl)) ? NBR_EPTHREAD : NBR_OK;
		}
		inline bool try_wrlock() {
			return (0 != pthread_rwlock_trywrlock(&rwl));
		}
		inline int unlock() {
			return (0 != pthread_rwlock_unlock(&rwl)) ? NBR_EPTHREAD : NBR_OK;
		}
	};
	class spinlk {
	protected:
		U32 lk;
	public:
		spinlk() : lk(0) { init(); }
		~spinlk() { unlock(); }
		inline int init() {
			return NBR_OK;
		}
		inline void fin() {}
		inline int lock() {
			while(__sync_bool_compare_and_swap(&lk, 0, 1)) { sched_yield(); }
			return NBR_OK;
		}
		inline bool trylock() {
			return !__sync_bool_compare_and_swap(&lk, 0, 1);
		}
		inline void unlock() {
			__sync_lock_test_and_set(&lk, 0);
		}
	};
	template <class object>
	class scoped {
		object &obj;
	public:
		scoped(object &o) : obj(o) {}
		~scoped() { unlock(); }
		int lock() { return obj.lock(); }
		void unlock() { obj.unlock(); }
	};
	class event {
	protected:
		pthread_cond_t cond;
		mutex mtx;
	public:
		event() {}
		operator pthread_cond_t *() { return &cond; }
		int init() {
			int r;
			if ((r = mtx.init()) < 0) { return r; }
			return pthread_cond_init(&cond, NULL) != 0 ? NBR_EPTHREAD : NBR_OK;
		}
		void fin() {
			mtx.fin();
			pthread_cond_destroy(&cond);
		}
		int wait(int timeout) {
			struct timeval tv;
			struct timespec ts;
			if (timeout < 0) {
				return pthread_cond_wait(&cond, mtx);
			}
			gettimeofday(&tv, NULL);
			ts.tv_sec = tv.tv_sec + (timeout / 1000);
			ts.tv_nsec = (tv.tv_usec * 1000 + ((timeout % 1000) * 1000000));
			if (ts.tv_nsec >= 1000000000) {
				ts.tv_nsec -= 1000000000;
				ts.tv_sec++;
			}
			return pthread_cond_timedwait(&cond, mtx, &ts);
		}
		int signal(bool bcast) {
			return bcast ? pthread_cond_broadcast(*this) :
				pthread_cond_signal(*this);
		}
	};
protected:
#if defined(__NBR_OSX__)
	static const pthread_t INVALID_PTHREAD;
#else
	static const pthread_t INVALID_PTHREAD = 0;
#endif
	pthread_t m_id;
	static pthread_key_t m_key;
	static volatile int m_key_initialized, m_rv;
	class thread_pool *m_belong;
	event m_event;
	void *m_p;
	void *m_tls;
	void *(*m_fn)(void *);
public:
	thread() : m_id(INVALID_PTHREAD), m_belong(NULL), m_p(NULL), m_tls(NULL) {}
	~thread() { fin(); }
	pthread_t id() const { return m_id; }
	void *param() { return m_p; }
	static int static_init() {
		if (__sync_bool_compare_and_swap(&m_key_initialized, 0, 1)) {
			int r = pthread_key_create(&m_key, NULL);
			m_rv = ((r != 0) ? NBR_EPTHREAD : NBR_OK);
			m_key_initialized = 2;
		}
		/* thread which not initialize m_key, should be wait for initialize thread 
		finished its job. */
		while (m_key_initialized == 1) {
			util::time::sleep(1 * 1000 * 1000);	//sleep 1ms
		}
		return m_rv;
	}
	int init(void *(*fn)(void *), void *p, class thread_pool *thp = NULL) {
		int r;
		if ((r = m_event.init()) < 0) { return r; }
		m_belong = thp;
		m_p = p;
		m_fn = fn;
		return (0 != pthread_create(&m_id, NULL, launch, this)) ? NBR_EPTHREAD : NBR_OK;
	}
	template <class CLOSURE>
	int init(class thread_pool *thp = NULL) {
		int r;
		if ((r = m_event.init()) < 0) { return r; }
		m_belong = thp;
		m_p = reinterpret_cast<void *>(new CLOSURE());
		m_fn = NULL;
		return (0 != pthread_create(&m_id, NULL, launch_closure<CLOSURE>, this)) ?
			NBR_EPTHREAD : NBR_OK;
	}
	static void *launch(void *p) {
		int r;
		thread *t = reinterpret_cast<thread *>(p);
		if (static_init() < 0) { return NULL; }
		//TRACE("m_key = %u\n", m_key);
		if (0 != (r = pthread_setspecific(m_key, t))) {
			ASSERT(false);
			return NULL;
		}
		return t->m_fn(t->m_p);
	}
	template <class CLOSURE>
	static void *launch_closure(void *p) {
		int r;
		thread *t = reinterpret_cast<thread *>(p);
		if (static_init() < 0) { return NULL; }
		if (0 != (r = pthread_setspecific(m_key, t))) {
			ASSERT(false);
			return NULL;
		}
		(*reinterpret_cast<CLOSURE *>(t->m_p))();
		delete reinterpret_cast<CLOSURE *>(t->m_p);
		return NULL;
	}
	static class thread *current() { return reinterpret_cast<thread *>(pthread_getspecific(m_key)); }
	template <class TLS> static void init_tls(TLS *tls) { current()->m_tls = reinterpret_cast<void *>(tls); }
	template <class TLS> static void fin_tls() { delete current_tls<TLS>(); }
	template <class TLS> static TLS *current_tls() { return reinterpret_cast<TLS *>(current()->m_tls); }
	template <class TLS> TLS *tls() { return reinterpret_cast<TLS *>(m_tls); }
	inline bool is_current() const { return current() == this; }
	inline bool has_pool() const { return m_belong != NULL; }
	int fin() {
		int r = stop();
		m_event.fin();
		return r;
	}
	int join() {
		void *r;
		int e = NBR_OK;
		if (m_id == INVALID_PTHREAD) { ASSERT(false);return NBR_OK; }
		if ((e = pthread_join(m_id, &r)) != 0) { e = NBR_EPTHREAD; }
		if(r && r != PTHREAD_CANCELED) { e = NBR_EPTHREAD; }
		m_id = INVALID_PTHREAD;
		return e;
	}
	int stop() {
		if (m_id == INVALID_PTHREAD) { return NBR_OK; }
		if (pthread_cancel(m_id) != 0) { return NBR_EPTHREAD; }
		return join();
	}
	static inline void cancelable(bool on) {
		if (on) { pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); }
		else { pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); }
	}
	static inline void try_cancel() {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	}
	static inline U64 SEC(int sec) { return (sec * 1000 * 1000 * 1000LL); }
	static inline U64 MS(int ms) { return (ms * 1000 * 1000LL); }
	static inline U64 US(int us) { return (us * 1000LL); }
	static inline U64 NS(int ns) { return ns; }
	static inline int sleep(U64 nanosec) {
		struct timespec ts;
		ts.tv_sec = nanosec / (1000 * 1000 * 1000);
		ts.tv_nsec = nanosec % (1000 * 1000 * 1000);
		return (0 == ::nanosleep(&ts, NULL) ? NBR_OK : NBR_ESYSCALL);
	}
	inline const event &ev() const { return m_event; }
};

class thread_pool {
protected:
	array<thread> m_pool;
	thread::event m_event;
public:
	thread_pool() : m_pool(), m_event() {}
	int init(int num) {
		if (m_event.init() < 0) { return NBR_EPTHREAD; }
		return m_pool.init(num, -1, opt_threadsafe | opt_expandable) ? NBR_OK : NBR_EMALLOC;
	}
	bool started() const { return m_pool.initialized() && m_pool.use() > 0; }
	int start(int num, void *(*fn)(void *), void *p) {
		int r;
		if ((r = init(num))) { return r; }
		for (int i = 0; i < num; i++) {
			if ((r = addjob(fn, p)) < 0) { return r; }
		}
		return NBR_OK;
	}
	int addjob(void *(*fn)(void *), void *p) {
		thread *th = m_pool.alloc();
		if (!th) { return NBR_ESHORT; }
		return th->init(fn, p, this);
	}
	template <class CLOSURE>
	int start(int num) {
		int r;
		if ((r = init(num))) { return r; }
		for (int i = 0; i < num; i++) {
			if ((r = addjob<CLOSURE>()) < 0) { return r; }
		}
		return NBR_OK;
	}
	template <class CLOSURE>
	int addjob() {
		thread *th = m_pool.alloc();
		if (!th) { return NBR_ESHORT; }
		return th->init<CLOSURE>(this);
	}
	int join() {
		int r, rr = NBR_OK; array<thread>::iterator i = m_pool.begin(), ni;
		for (; i != m_pool.end();) {
			ni = i;
			i = m_pool.next(i);
			if ((r = (*ni).join()) < 0) { ASSERT(false); rr = r; }
			m_pool.erase(ni);
		}
		return rr;
	}
	int stop() {
		int r, rr = NBR_OK; array<thread>::iterator i = m_pool.begin(), ni;
		for (; i != m_pool.end();) {
			ni = i;
			i = m_pool.next(i);
			if ((r = (*ni).stop()) < 0) { rr = r; }
			m_pool.erase(ni);
		}
		return rr;
	}
	void fin() {
		if (!m_pool.initialized()) { return; }
		array<thread>::iterator i = m_pool.begin();
		for (; i != m_pool.end(); i = m_pool.next(i)) { (*i).fin(); }
		m_pool.fin();
	}
	inline const thread::event &ev() const { return m_event; }
};

}
}

#endif
