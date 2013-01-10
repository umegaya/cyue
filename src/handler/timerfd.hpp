/***************************************************************
 * timerfd.hpp : timer fd implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__TIMERFD_HPP__)
#define __TIMERFD_HPP__

#include "timerfd.h"
#include "event.h"

namespace yue {
namespace handler {
/*bool timerfd::taskgrp::task::operator () (struct task *) {
	event::timer ev(this);
	emit_one(event::ID_TIMER, ev);
	return true;
}*/
void timerfd::taskgrp::task::close() {
#if defined(__ENABLE_TIMER_FD__)
	m_owner->remove_timer_reserve(this);
#else
	loop::timer().tg()->remove_timer_reserve(this);
#endif
}
void timerfd::taskgrp::task::destroy() {
#if defined(__ENABLE_TIMER_FD__)
	m_owner->remove_timer(this);
#else
	loop::timer().tg()->remove_timer(this);
#endif
}
/* timer callback */
int timerfd::taskgrp::operator () (U64 c) {
	__sync_add_and_fetch(&m_trigger_count, c);
	process_one_shot(m_trigger_count);
	return NBR_OK;
}
void timerfd::taskgrp::process_one_shot(U64 count) {
	/* prevent from executing next task list before previous task list execution finished */
	TFD_SPEC_TRACE("pos: start: %llu\n", count);
	if (!__sync_bool_compare_and_swap(&m_processed, false, true)) { return; }
	/* if two or more trigger skipped on above, execute tasks until catch up to latest */
	TFD_SPEC_TRACE("pos: enter loop %llu\n", count);
	while (m_count < count) {
		task *t, *tt;
		int idx = m_count % m_size;
		/* remove idx-th task list from scheduler so that
		 * another thread can add new task at this idx freely. */
		t = m_sched[idx];
		m_sched[idx] = NULL;
		m_count++;
		TFD_SPEC_TRACE("idx = %u: %p %u\n", idx, t, m_size);
		/* TODO: too much process count, should we exit? */
		while((tt = t)) {
			t = t->m_next;
			if (tt->m_data.m_removed) {
				remove_timer(tt);
				TFD_SPEC_TRACE("idx = %u: %p removed\n", idx, t);
				continue;
			}
			if (tt->m_data.m_delay > 0) {
				tt->m_data.m_delay--;
				TFD_SPEC_TRACE("%p: %u removed = %u\n", tt, tt->m_data.m_delay, tt->m_data.m_removed);
				insert_timer_at(tt, idx);
				TFD_SPEC_TRACE("idx = %u: %p insert again %u\n", idx, tt, tt->m_data.m_removed);
				continue;
			}
			if (tt->m_h(tt) < 0) {
				remove_timer(tt);
				TFD_SPEC_TRACE("idx = %u: %p cb returns minus\n", idx, t);
				continue;
			}
			if (!insert_timer(tt, tt->m_idx, idx)) {
				remove_timer(tt);
				TFD_SPEC_TRACE("idx = %u: %p insert timer fails\n", idx, t);
				continue;
			}
		}
		if (m_sched_insert) {
			{
				util::thread::scoped<util::thread::mutex> lk(m_mtx);
				if (lk.lock() < 0) { ASSERT(false); continue; }
				t = m_sched_insert;
				m_sched_insert = NULL;
			}
			while((tt = t)) {
				t = t->m_next;
				U16 start_idx = tt->m_start_idx;
				tt->m_data.m_removed = 0;
				TFD_SPEC_TRACE("insert_timer initial: %p %u %u\n", tt, tt->m_start_idx, tt->m_data.m_removed);
				if (!insert_timer(tt, start_idx, idx)) {
					remove_timer(tt);
					continue;
				}
				TFD_SPEC_TRACE("insert_timer initial: aft %p %u %u\n", tt, tt->m_data.m_delay, tt->m_data.m_removed);
			}
		}
	}
	if (m_trigger_count > MAX_COUNT) {
		int idx = m_count % m_size;
		__sync_add_and_fetch(&m_trigger_count, idx - m_count);
		m_count = idx;
	}
	m_processed = false;

	util::time::update_clock();
}
int timerfd::operator () (U64 c) {
	for (U64 i = 0; i < c; i++) {
		event::timer ev(this);
		emittable::emit_one(event::ID_TIMER, ev);
	}
	return NBR_OK;
}
}
}

#endif
