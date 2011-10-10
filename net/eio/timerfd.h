/***************************************************************
 * timerfd.h : event IO timer handling (can efficiently handle 10k timers)
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
#if !defined(__TIMERFD_H__)
#define __TIMERFD_H__

#include "selector.h"
#include "syscall.h"
#include "functional.h"
#if defined(__ENABLE_TIMER_FD__)
#include <sys/timerfd.h>
#else
#include <sys/time.h>
#endif
#include <time.h>

namespace yue {
namespace module {
namespace net {
namespace eio {
class timerfd {
public:
	typedef functional<int (U64)> handler;
	struct task {
		U16 m_idx; U8 m_removed, padd;
		U32 m_count;
		handler m_h;
		struct task *m_next;
		task() : m_removed(0), m_count(0), m_next(NULL) {}
	};
protected:
	task **m_sched;
	util::thread::mutex m_mtx;
	array<task> m_entries;
	U64 m_count;
	int m_size, m_res_us;
	DSCRPTR m_fd;
public:
	timerfd() : m_sched(NULL), m_mtx(), m_entries(), m_count(0LL),
		m_size(0), m_res_us(0), m_fd(INVALID_FD) {}
	~timerfd() {}
	DSCRPTR fd() const { return m_fd; }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	static const int MAX_TASK = 10000;
	static const int MAX_INTV_SEC = 10;
	static const int RESOLUTION_US = 100 * 1000;
	int init(int max_task = MAX_TASK,
			int max_intv_sec = MAX_INTV_SEC, int resolution_us = RESOLUTION_US) {
		int r;
		if (resolution_us <= 0) { return NBR_EINVAL; }
		m_res_us = resolution_us;
		m_size = (int)((double)(max_intv_sec * 1000 * 1000)/((double)m_res_us));
		if (!(m_sched = new task*[m_size])) { return NBR_EMALLOC; }
		util::mem::fill(m_sched, 0, sizeof(task*) * m_size);
		if ((r = m_mtx.init()) < 0) { return r; }
		if ((r = m_entries.init(max_task, -1, opt_expandable | opt_threadsafe)) < 0) {
			return r;
		}
#if defined(__ENABLE_TIMER_FD__)
		struct itimerspec spec;
		if (::clock_gettime(CLOCK_REALTIME, &(spec.it_value)) == -1) { return NBR_ESYSCALL; }
		spec.it_interval.tv_sec = resolution_us / (1000 * 1000);
		spec.it_interval.tv_nsec = resolution_us % (1000 * 1000);
		if ((m_fd = ::timerfd_create(CLOCK_REALTIME, 0)) == INVALID_FD) {
			return NBR_ESYSCALL;
		}
		if (::timerfd_settime(m_fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1) {
			return NBR_ESYSCALL;
		}
#else
		struct itimerval spec;
		/* tv_sec or tv_usec must be non-zero. */
		spec.it_value.tv_sec = 0;
		spec.it_value.tv_usec = 1;
		spec.it_interval.tv_sec = resolution_us / (1000 * 1000);
		spec.it_interval.tv_usec = resolution_us % (1000 * 1000);
		if (setitimer(ITIMER_REAL, &spec, NULL) != 0) { return NBR_ESYSCALL; }
#endif
		return NBR_OK;
	}
	void fin() {
		if (m_sched) {
			delete []m_sched;
			m_sched = NULL;
		}
		if (m_entries.initialized()) {
			m_entries.fin();
		}
		m_mtx.fin();
	}
	/* for timerfd callback from poller */
	template <class EM>
	int operator () (EM &em, selector::method::event &e) {
		return process(e);
	}
	/* for sigalrm */
	void operator () (int) {
		process_one_shot(m_count);
	}
	timerfd::task *add_timer(handler &h, double start_sec, double intval_sec) {
		task *t = create_task(h, start_sec, intval_sec);
		insert_timer(t, index_from(start_sec));
		return t;
	}
	void remove_timer_reserve(task *t) {
		t->m_removed = 1;
	}
protected:
	task *create_task(handler &h, double start_sec, double intval_sec) {
		task *t = m_entries.alloc();
		t->m_h = h;
		t->m_idx = get_duration_index(intval_sec);
		return t;
	}
	int get_duration_index(double duration_sec) {
		return (int)((duration_sec * 1000000 / m_res_us));
	}
	int index_from(double duration_sec) {
		return get_duration_index(duration_sec) + (m_count % m_size);
	}
	void remove_timer(task *t) {
		m_entries.free(t);
	}
	void insert_timer(task *t, int index) {
		if (index >= m_size) { ASSERT(false); return; }
		util::thread::scoped<util::thread::mutex> lk(m_mtx);
		t->m_next = m_sched[index];
		m_sched[index] = t;
		return;
	}
	int process(selector::method::event &e) {
		U64 c;
		/* if entire program execution is too slow, it is possible that
		 * multiple timer expiration happened. */
		while (syscall::read(m_fd, (char *)&c, sizeof(c)) > 0) {
			process_one_shot(c);
		}
		return error_again() ? handler_result::again :
			handler_result::destroy;/* if EAGAIN, back to epoll fd set */
	}
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
			if (tt->m_removed || tt->m_h(++(tt->m_count)) < 0) {
				remove_timer(tt);
			}
			else {
				insert_timer(tt, (m_count + tt->m_idx) % m_size);
			}
		}
		m_count++;
		/* TODO: remove nbr_*** */
		util::time::update_clock();
	}
};
}
}
}
}

#endif
