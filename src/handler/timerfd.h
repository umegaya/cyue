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
#include "constant.h"
#if defined(__ENABLE_TIMER_FD__)
#include <sys/timerfd.h>
#else
#include <sys/time.h>
#include <signal.h>
#endif
#include <time.h>

#define TFD_TRACE(...)

#if defined(_DEBUG)
#define TFD_SPEC_TRACE(fmt, ...) //if (true /*m_tfd->fd() == 15*/) { TRACE("fd(%d)"fmt, m_tfd->fd(), __VA_ARGS__); }
#else
#define TFD_SPEC_TRACE(...)
#endif

#if defined(__NBR_OSX__) || defined(__NBR_IOS__)
#define USE_LEGACY_TIMER
#endif

namespace yue {
class loop;
namespace handler {
using namespace util;
class timerfd : public base {
public:
#define INVALID_TIMER (0)
#if defined(USE_LEGACY_TIMER)
	typedef void *timer_t;
#endif
	typedef util::functional<int (U64)> handler;
	class taskgrp {
	public:
		static const int MAX_TASK = 10000;
		static const int MAX_INTV_SEC = 10;
		static const int RESOLUTION_US = 100 * 1000;
		static const int MAX_DELAY = ((1 << 15) - 1);
		static const int MAX_INDEX = ((1 << 16) - 1);
#if defined(__ANDROID_NDK__)
		// http://code.google.com/p/android/issues/detail?id=41297
		typedef U32 globalcount;
		static const U32 MAX_COUNT = 0x7FFFFFFF;
#else
		typedef U64 globalcount;
		static const U64 MAX_COUNT = 0x7FFFFFFFFFFFFFFF;
#endif
		struct task;
		typedef util::functional<int (struct task *)> handler;
		struct task {
			struct task *m_next;
			U16 m_idx;
			union {
				struct { U16 m_removed:1, m_delay:15; } m_data;
				U16 m_start_idx;
			};
			handler m_h;
			taskgrp *m_owner;
			task(taskgrp *tg, int start_idx) : m_next(NULL), m_owner(tg) { m_start_idx = start_idx; }
			inline void close();
			inline void destroy();
		};
	protected:
		task **m_sched, *m_sched_insert;
		thread::mutex m_mtx;
		array<task> m_entries;
		bool m_processed;
		globalcount m_count, /* how many times callback actually executed? */
			m_trigger_count/* how many times system triggered call back? */;
		int m_size, m_res_us;
		int m_max_task, m_max_intv_sec;
		timerfd *m_tfd;
	public:
		taskgrp(timerfd *tfd, int max_task, int max_intv_sec, int resolution_us) :
			m_sched(NULL), m_sched_insert(NULL), m_mtx(), m_entries(), m_processed(false),
			m_count(0LL), m_trigger_count(0LL), m_size(0), m_res_us(resolution_us),
			m_max_task(max_task), m_max_intv_sec(max_intv_sec), m_tfd(tfd) {}
		~taskgrp() { fin(); }
		int init() {
			int r;
			if (m_res_us <= 0) { return NBR_EINVAL; }
			m_size = (int)((double)(m_max_intv_sec * 1000 * 1000)/((double)m_res_us));
			if (!(m_sched = new task*[m_size])) { return NBR_EMALLOC; }
			util::mem::fill(m_sched, 0, sizeof(task*) * m_size);
			if ((r = m_mtx.init()) < 0) { return r; }
			return m_entries.init(m_max_task, -1, opt_expandable | opt_threadsafe);
		}
	public:
		void fin() {
			if (m_sched) {
				delete []m_sched;
				m_sched = NULL;
			}
			m_sched_insert = NULL;
			if (m_entries.initialized()) {
				m_entries.fin();
			}
			m_mtx.fin();
		}
		/* timer callback */
		inline int operator () (U64 c);
		template <class H>
		inline task *add_timer(H &h, double start_sec, double intval_sec, task **ppt = NULL) {
			task *t = create_task(start_sec, intval_sec);
			if (!t) { goto error; }
			t->m_h.set(h);
			if (ppt) { *ppt = t; }
			if (!insert_timer(t, start_sec, intval_sec)) { goto error; }
			return t;
		error:
			ASSERT(false);
			if (t) { m_entries.free(t); }
			return NULL;
		}
		inline void remove_timer_reserve(task *t) {
			t->m_data.m_removed = 1;
		}
	protected:
		inline task *insert_timer(task *t, double start_sec, double intval_sec) {
			util::thread::scoped<util::thread::mutex> lk(m_mtx);
			if (lk.lock() < 0) { ASSERT(false); return NULL; }
			t->m_next = m_sched_insert;
			m_sched_insert = t;
			TFD_SPEC_TRACE("%lf, %lf, index_from=%d\n", start_sec, intval_sec, t->m_start_idx);
			return t;
		}
		inline task *create_task(double start_sec, double intval_sec) {
			int idx = get_duration_index(start_sec);
            taskgrp *g = this;
			task *t = m_entries.alloc(g, idx);
			if (!t) { return NULL; }
			idx = get_duration_index(intval_sec);
			if (idx > MAX_INDEX || idx < 1) {
				ASSERT(false);
				m_entries.free(t);
				return NULL;
			}
			t->m_idx = idx;
			return t;
		}
		inline int get_duration_index(double duration_sec) {
			return (int)((duration_sec * 1000000 / m_res_us));
		}
		inline void remove_timer(task *t) {
			m_entries.free(t);
		}
		inline bool insert_timer_at(task *t, int index) {
			t->m_next = m_sched[index];
			m_sched[index] = t;
			return true;
		}
		inline bool insert_timer(task *t, int index, int current) {
			/*
			index = 1060 @ count == 150
			index = 1060, current = 50
			tick1 @ 1110 = 11 * 100 + 10 => idx = 10, delay = 10
			tick2 @ 1070 = 10 * 100 + 70 => idx = 70, delay = 10
			tick3 @ 1130 = 11 * 100 + 30 => idx = 30, delay = 10

			index = 20 @ count = 195
			index = 20, current = 95
			tick1 @ 115 = 1 * 100 + 15 => idx = 15, delay = 1 = >wrong!!
			delay should be 0
			so?

			index = 10 @ count 10 (size = 10)
			index = 10 , current 0
			tick1 @ 10 = 1 * 10 + 0 => idx = 0, delay = 1

			index = 20 @ count 10 (size = 10)
			index = 20 , current 0
			tick1 @ 20 = 2 * 10 + 0 => idx = 0, delay = 1

			*/
			int tmp = (index + current);
			int delay = index / m_size;
			index = tmp % m_size;
			if (index == current) {
				ASSERT(((tmp - current) % m_size) == 0);
				if (delay > 0) { delay--; }
				else { index++; } /* if delay <= 0, means interval is less than max_intval,
				++ for executing this task for next task callback otherwise first execution delayed max_interval seconds. */
			}
			TFD_SPEC_TRACE("insert timer: %u %u %u %u %u\n", index, delay, tmp - current, current, m_size);
			ASSERT(index < m_size);
			if (delay > MAX_DELAY) { ASSERT(false); return false; }
			t->m_data.m_delay = delay;
			return insert_timer_at(t, index);
		}
		inline void process_one_shot(U64 count);
	};
protected:
	timer_t m_timer;
	DSCRPTR m_fd;
	handler m_h;
	taskgrp *m_tg;
	U8 m_closed, padd[3];
	char *m_name;
public:
	timerfd() : base(TIMER), m_timer(INVALID_TIMER), m_fd(INVALID_FD), m_h(*this), m_tg(NULL), m_closed(0), m_name(NULL) {}
	template <class H>
	timerfd(H &h) : base(TIMER), m_timer(INVALID_TIMER), m_fd(INVALID_FD), m_h(h), m_tg(NULL), m_closed(0), m_name(NULL) {}
	~timerfd() { fin(); }
	DSCRPTR fd() const { return m_fd; }
	taskgrp *tg() { return m_tg; }
	inline const char *set_name(const char *name) { return (m_name = util::str::dup(name)); }
	inline const char *name() const { return m_name; }
	inline void clear_commands_and_watchers() { emittable::clear_commands_and_watchers(); }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	int init_taskgrp(
		int max_task = taskgrp::MAX_TASK,
		int max_intv_sec = taskgrp::MAX_INTV_SEC,
		int resolution_us = taskgrp::RESOLUTION_US) {
		if (!(m_tg = new taskgrp(this, max_task, max_intv_sec, resolution_us))) {
			return NBR_EMALLOC;
		}
		m_h.set(*m_tg);
		return init(0.0f, ((double)resolution_us) / (1000 * 1000));
	}
	int init(double start_sec, double intval_sec) {
		int r;
		U64 start_us = start_sec * 1000 * 1000, intval_us = intval_sec * 1000 * 1000;
		if (intval_us <= 0) { intval_us = 1; }
		if (m_fd >= 0) { return m_fd; }
		if (m_tg && (r = m_tg->init()) < 0) { return r; }
#if defined(__ENABLE_TIMER_FD__)
		struct itimerspec spec;
		if (::clock_gettime(CLOCK_REALTIME, &(spec.it_value)) == -1) { return NBR_ESYSCALL; }
		spec.it_value.tv_sec += start_us / (1000 * 1000);
		U64 ns = (spec.it_value.tv_nsec + (1000 * (start_us % (1000 * 1000))));
		if (ns >= (1000 * 1000 * 1000)) {
			spec.it_value.tv_nsec = (ns % (1000 * 1000 * 1000));
			spec.it_value.tv_sec++;
		}
		spec.it_interval.tv_sec = intval_us / (1000 * 1000);
		spec.it_interval.tv_nsec = (1000 * intval_us) % (1000 * 1000 * 1000);
		DSCRPTR fd;
		if ((fd = ::timerfd_create(CLOCK_REALTIME, 0)) == INVALID_FD) {
			return NBR_ESYSCALL;
		}
		if (::timerfd_settime(fd, TFD_TIMER_ABSTIME, &spec, NULL) == -1) {
			return NBR_ESYSCALL;
		}
		m_fd = fd;
		TRACE("timerfd: open: %d\n", m_fd);
		return m_fd;
#elif defined(USE_LEGACY_TIMER)
		struct itimerval spec;
		/* tv_sec or tv_usec must be non-zero. */
		spec.it_value.tv_sec = start_us / (1000 * 1000);
		spec.it_value.tv_usec = start_us / (1000 * 1000);
		if (spec.it_value.tv_sec == 0 && spec.it_value.tv_usec == 0) {
			spec.it_value.tv_usec = 1;	//both 0 causes timer disable
		}
		spec.it_interval.tv_sec = intval_us / (1000 * 1000);
		spec.it_interval.tv_usec = intval_us % (1000 * 1000);
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
		spec.it_value.tv_sec = start_us / (1000 * 1000);
		spec.it_value.tv_nsec = (1000 * start_us) % (1000 * 1000 * 1000);
		spec.it_interval.tv_sec = intval_us / (1000 * 1000);
		spec.it_interval.tv_nsec = (1000 * intval_us) % (1000 * 1000 * 1000);
		if (::timer_settime(m_timer, TIMER_ABSTIME, &spec, NULL) != 0) { return NBR_ESYSCALL; }
#endif
		return 0;
	}
	void fin() {
		if (m_tg) {
			util::debug::bt();
			delete m_tg;
			m_tg = NULL;
		}
		if (m_name) {
			util::mem::free(m_name);
			m_name = NULL;
		}
	}
	INTERFACE DSCRPTR on_open(U32 &) {
#if !defined(__ENABLE_TIMER_FD__)
		ASSERT(false);
#endif
		return m_fd;
	}
	void close() {
		if (__sync_bool_compare_and_swap(&m_closed, 0, 1)) {
			base::sched_close();
		}
	}
	INTERFACE void on_close() {
#if !defined(USE_LEGACY_TIMER)
		if (m_timer != INVALID_TIMER) {
			::timer_delete(m_timer);
			m_timer = INVALID_TIMER;
		}
#endif
		if (m_fd != INVALID_FD) {
			::close(m_fd);
			m_fd = INVALID_FD;
		}
	}
	/* for timerfd callback from poller */
	INTERFACE result on_read(loop &, poller::event &e) {
		U64 c;
		/* if entire program execution is too slow, it is possible that
		 * multiple timer expiration happened. (c > 1) */
		while (net::syscall::read(m_fd, &c, sizeof(c)) == sizeof(c)) {
			TFD_TRACE("timerfd: %d %llu\n", m_fd, c); ASSERT(c < 100000000); m_h(c);
		}
		return error_again() ? read_again : destroy;
	}
	/* for sigalrm */
	void operator () (int) { m_h(1); }

	/* timer callback */
	inline int operator () (U64 c);
};
}
}

#endif
