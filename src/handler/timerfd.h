/***************************************************************
 * timerfd.h : event IO timer handling (can efficiently handle 10k timers)
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__TIMERFD_H__)
#define __TIMERFD_H__

#include "handler.h"
#include "syscall.h"
#include "functional.h"
#include "thread.h"
#include "util.h"
#include "parking.h"
#include "emittable.h"
#if defined(__ENABLE_TIMER_FD__)
#include <sys/timerfd.h>
#else
#include <sys/time.h>
#include <signal.h>
#endif
#include <time.h>

namespace yue {
class loop;
namespace handler {
using namespace util;
class timerfd : public base {
public:
	struct task;
	typedef functional<int (struct task*)> handler;
	struct task {
		U16 m_idx; U8 m_removed, padd;
		U32 m_count;
		handler m_h;
		struct task *m_next;
		task() : m_removed(0), m_count(0), m_next(NULL) {}
		inline U32 tick() const { return m_count; }
	};
#define INVALID_TIMER (NULL)
#if defined(__NBR_OSX__)
	typedef void *timer_t;
#endif
protected:
	task **m_sched;
	thread::mutex m_mtx;
	array<task> m_entries;
	U64 m_count;
	int m_size, m_res_us;
	int m_max_task, m_max_intv_sec;
	timer_t m_timer;
	DSCRPTR m_fd;
public:
	timerfd() : base(TIMER), m_sched(NULL), m_mtx(), m_entries(), m_count(0LL),
		m_size(0), m_res_us(RESOLUTION_US),
		m_max_task(MAX_TASK), m_max_intv_sec(MAX_INTV_SEC), m_timer(INVALID_TIMER),
		m_fd(INVALID_FD) {}
	~timerfd() {}
	DSCRPTR fd() const { return m_fd; }
	inline int max_task() const { return m_size; }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	static const int MAX_TASK = 10000;
	static const int MAX_INTV_SEC = 10;
	static const int RESOLUTION_US = 100 * 1000;
	bool configure(int max_task, int max_intv_sec, int resolution_us) {
		m_max_task = max_task,
		m_max_intv_sec = max_intv_sec,
		m_res_us = resolution_us;
		return true;
	}
	INTERFACE DSCRPTR on_open(U32 &) {
#if !defined(__ENABLE_TIMER_FD__)
		ASSERT(false);
#endif
		return init();
	}
	int init() {
		int r;
		if (m_fd >= 0) { return m_fd; }
		if (m_res_us <= 0) { return NBR_EINVAL; }
		m_size = (int)((double)(m_max_intv_sec * 1000 * 1000)/((double)m_res_us));
		if (!(m_sched = new task*[m_size])) { return NBR_EMALLOC; }
		util::mem::fill(m_sched, 0, sizeof(task*) * m_size);
		if ((r = m_mtx.init()) < 0) { return r; }
		if ((r = m_entries.init(m_max_task, -1, opt_expandable | opt_threadsafe)) < 0) {
			return r;
		}
#if defined(__ENABLE_TIMER_FD__)
		struct itimerspec spec;
		if (::clock_gettime(CLOCK_REALTIME, &(spec.it_value)) == -1) { return NBR_ESYSCALL; }
		spec.it_interval.tv_sec = m_res_us / (1000 * 1000);
		spec.it_interval.tv_nsec = (1000 * m_res_us) % (1000 * 1000 * 1000);
		if ((m_fd = ::timerfd_create(CLOCK_REALTIME, 0)) == INVALID_FD) {
			return NBR_ESYSCALL;
		}
		if (::timerfd_settime(m_fd, TFD_TIMER_ABSTIME, &spec, NULL) == -1) {
			return NBR_ESYSCALL;
		}
		TRACE("timerfd: open: %d\n", m_fd);
		return m_fd;
#elif defined(__NBR_OSX__)
		struct itimerval spec;
		/* tv_sec or tv_usec must be non-zero. */
		spec.it_value.tv_sec = 0;
		spec.it_value.tv_usec = 1;
		spec.it_interval.tv_sec = m_res_us / (1000 * 1000);
		spec.it_interval.tv_usec = m_res_us % (1000 * 1000);
		if (setitimer(ITIMER_REAL, &spec, NULL) != 0) { return NBR_ESYSCALL; }
#else
		struct sigevent sev;
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = SIGALRM;
		//sev.sigev_notify = SIGEV_THREAD;
		//sev.sigev_notify_function = timer_callback;
		//sev.sigev_notify_thread_id = syscall(SYS_gettid);	//someone says glibc does not provide wrapper of gettid
		if (::timer_create(CLOCK_MONOTONIC, &sev, &m_timer)) { return NBR_ESYSCALL; }
		struct itimerspec spec;
		/* tv_sec or tv_usec must be non-zero. */
		spec.it_value.tv_sec = 0;
		spec.it_value.tv_nsec = 1;
		spec.it_interval.tv_sec = m_res_us / (1000 * 1000);
		spec.it_interval.tv_nsec = (m_res_us % (1000 * 1000)) * 1000;
		if (::timer_settime(m_timer, TIMER_ABSTIME, &spec, NULL) != 0) { return NBR_ESYSCALL; }
#endif
		return 0;
	}
	INTERFACE void on_close() {
#if !defined(__NBR_OSX__)
		if (m_timer != INVALID_TIMER) {
			::timer_delete(m_timer);
			m_timer = INVALID_TIMER;
		}
#endif
		if (m_sched) {
			delete []m_sched;
			m_sched = NULL;
		}
		if (m_entries.initialized()) {
			m_entries.fin();
		}
		m_mtx.fin();
		if (m_fd != INVALID_FD) {
			::close(m_fd);
			m_fd = INVALID_FD;
		}
	}
	/* for timerfd callback from poller */
	INTERFACE result on_read(loop &, poller::event &e) {
		return process(e);
	}
	/* for sigalrm */
	void operator () (int) {
		process_one_shot(m_count);
	}
	template <class H>
	timerfd::task *add_timer(H &h, double start_sec, double intval_sec) {
		handler hd(h);
		return add_timer(hd, start_sec, intval_sec);
	}
	void remove_timer_reserve(task *t) {
		t->m_removed = 1;
	}
protected:
	timerfd::task *add_timer(handler &h, double start_sec, double intval_sec) {
		task *t = create_task(h, start_sec, intval_sec);
		if (!t) { ASSERT(false); return NULL; }
		insert_timer(t, index_from(start_sec));
		TRACE("%lf, %lf, index_from=%d\n", start_sec, intval_sec, 	index_from(start_sec));
		return t;
	}
	task *create_task(handler &h, double start_sec, double intval_sec) {
		task *t = m_entries.alloc();
		if (!t) { return NULL; }
		t->m_h = h;
		t->m_idx = get_duration_index(intval_sec);
		return t;
	}
	int get_duration_index(double duration_sec) {
		return (int)((duration_sec * 1000000 / m_res_us));
	}
	int index_from(double duration_sec) {
		return (get_duration_index(duration_sec) + m_count) % m_size;
	}
	void remove_timer(task *t) {
		m_entries.free(t);
	}
	void insert_timer(task *t, int index) {
		if (index >= m_size) { ASSERT(false); return; }
		util::thread::scoped<util::thread::mutex> lk(m_mtx);
		if (lk.lock() < 0) { ASSERT(false); return; }
		t->m_next = m_sched[index];
		m_sched[index] = t;
		return;
	}
	result process(poller::event &e) {
		U64 c;
		/* if entire program execution is too slow, it is possible that
		 * multiple timer expiration happened. */
		while (net::syscall::read(m_fd, (char *)&c, sizeof(c)) > 0) {
			process_one_shot(c);
		}
		return error_again() ? read_again : destroy;
	}
	static void timer_callback(union sigval v);
	/* caution: not reentrant */
	inline void process_one_shot(U64 count) {
		task *t, *tt;
		int idx = m_count % m_size;
		{
			/* remove idx-th task list from scheduler so that
			 * another thread can add new task at this idx freely. */
			util::thread::scoped<util::thread::mutex> lk(m_mtx);
			if (lk.lock() < 0) { ASSERT(false); return; }
			t = m_sched[idx];
			m_sched[idx] = NULL;
		}
		/* TODO: too much process count, should we exit? */
		while((tt = t)) {
			t = t->m_next;
			++tt->m_count;
			if (tt->m_removed || tt->m_h(tt) < 0) {
				remove_timer(tt);
			}
			else {
				insert_timer(tt, (m_count + tt->m_idx) % m_size);
			}
		}
		m_count++;
		util::time::update_clock();
	}
};
}
}

#endif
