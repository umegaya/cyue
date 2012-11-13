/***************************************************************
 * app.h : application launcher
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see lisence.txt for detail
 **************************************************************/
#if !defined(__APP_H__)
#define __APP_H__

#include "thread.h"
#include "queue.h"
#include "array.h"
#include "functional.h"

namespace yue {
namespace util {
class app {
protected:
	static const int DEFAULT_WORKER_SIZE_HINT = 4;
	/* template arg LOOP which passed to init/run has following interface */
	template <class IMPL> class loop : public IMPL {
	public:
		typedef typename IMPL::launch_args launch_args;
		static inline int static_init(app &a) { return IMPL::static_init(a); }
		static inline void static_fin() { IMPL::static_fin(); }
		inline int init(launch_args &a) { return IMPL::init(a); }
		inline void fin() { IMPL::fin(); }
		inline void run(launch_args &a) { IMPL::run(a); }
	};
	thread_pool m_thp;
	bool m_alive;
	int m_argc; char **m_argv;
public:
	app() : m_thp(), m_alive(false), m_argc(0), m_argv(NULL) {}
	~app() {}
	inline thread_pool &tpool() { return m_thp; }
	inline int thn() const { return m_thp.pool().use(); }
	inline int argc() const { return m_argc; }
	inline char **argv() const { return m_argv; }
	inline bool alive() const { return m_alive; }
	inline bool die() { return __sync_bool_compare_and_swap(&m_alive, true, false); }
	template <class IMPL>
	int run(int argc, char *argv[]) {
		int r;
		if (!m_alive && (r = init<IMPL>(argc, argv)) < 0) { return r; }
		ASSERT(alive());
		r = join();
		fin<IMPL>();
		return r >= 0 || r == NBR_EPTHREAD ? NBR_OK : r;
	}
	/* if success, it returns number of worker thread lauched. */
	template <class IMPL>
	int init(int argc, char *argv[]) {
		int r;
		m_argc = argc;
		m_argv = argv;
		if (m_alive) { return NBR_OK; }
		m_alive = true;
		if ((r = util::static_init()) < 0) { return r; }
		if ((r = m_thp.init(DEFAULT_WORKER_SIZE_HINT)) < 0) { return r; }
		if ((r = loop<IMPL>::static_init(*this)) < 0) {
			return r;
		}
		return r;
	}
	template <class IMPL>
	static void *start(void *p) {
		typename IMPL::launch_args *a =
			reinterpret_cast<typename IMPL::launch_args *>(p);
		loop<IMPL> l;
		if (util::init() < 0) { return NULL; }
		if (l.init(*a) < 0) { return NULL; }
		l.run(*a);
		l.fin();
		util::fin();
		return p;
	}
protected:
	template <class IMPL>
	void fin() {
		m_thp.fin();
		loop<IMPL>::static_fin();
		util::static_fin();
		m_alive = false;
	}
	int join() {
		return m_thp.join();
	}
};
}
}
#endif
