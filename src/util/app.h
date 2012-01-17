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
	/* template arg LOOP which passed to init/run has following interface */
	template <class IMPL> class loop : public IMPL {
	public:
		static inline int static_init(app &a, int thn, int argc, char *argv[]) {
			return IMPL::static_init(a, thn, argc, argv);
		}
		static inline void static_fin() { IMPL::static_fin(); }
		inline int init(class app &a) { return IMPL::init(a); }
		inline void fin() { IMPL::fin(); }
		inline void run(class app &a) { IMPL::run(a); }
	};
	thread_pool m_thp;
	int m_thn;
	bool m_alive;
public:
	app() : m_thp(), m_thn(0), m_alive(true) {}
	~app() {}
	inline int thn() const { return m_thn; }
	inline bool alive() const { return m_alive; }
	inline bool die() { return __sync_bool_compare_and_swap(&m_alive, true, false); }
	template <class IMPL>
	int run(int argc, char *argv[], int thn = -1) {
		int r = NBR_OK;
		if (thn < 0) { thn = app::get_suitable_worker_count(); }
		if ((r = util::init()) < 0) { return r; }
		if ((m_thn = loop<IMPL>::static_init(*this, thn, argc, argv)) < 0) {
			return m_thn;
		}
		if (m_thn == 1) {
			app::start<IMPL>(this);
			goto end;
		}
		if ((r = m_thp.start(m_thn, app::start<IMPL>, this)) < 0) {
			goto end;
		}
		r = join();
	end:
		fin<IMPL>();
		return r;
	}
protected:
	template <class IMPL>
	void fin() {
		m_thp.fin();
		loop<IMPL>::static_fin();
		util::fin();
	}
	int join() {
		return m_thp.join();
	}
	template <class IMPL>
	static void *start(void *p) {
		class app *a = reinterpret_cast<class app *>(p);
		loop<IMPL> l;
		if (l.init(*a) < 0) { return NULL; }
		l.run(*a);
		l.fin();
		return p;
	}
	static int get_suitable_worker_count() {
		return 2;
	}
};
}
}
#endif
