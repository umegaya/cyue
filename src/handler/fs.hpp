/***************************************************************
 * fs.hpp : filesystem implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__FS_HPP__)
#define __FS_HPP__

#include "fs.h"
#include "event.h"

namespace yue {
namespace handler {
inline void fs::watcher::emit(notification *notify) {
	event::fs ev(notify, this);
	emittable::emit(event::ID_FILESYSTEM, ev);
}
void inotify::watcher::close() { loop::filesystem().unwatch(this); }
#if defined(__ENABLE_INOTIFY__)
fs::watcher *inotify::watch(const char *path, event_flags f) {
	DSCRPTR fd = INVALID_FD; watcher *ptr;
	if ((fd = inotify_add_watch(m_inotify_fd, path, f)) < 0) { goto error; }
	if (!(ptr = m_pool.alloc(fd))) { goto error; }
	if (loop::open(*ptr) < 0) { goto error; }
	return ptr;
error:
	if (ptr) { unwatch(ptr); }
	else if (fd != INVALID_FD) { ::close(fd); }
	return NULL;
}
#elif defined(__ENABLE_KQUEUE__)
inline int inotify::init() {
	int flags = util::opt_threadsafe | util::opt_expandable;
	if (!m_pool.init(DEFAULT_INOTIFY_WATCHER_COUNT_HINT, -1, flags)) { return NBR_EMALLOC; }
	if ((m_inotify_fd = kqueue()) < 0) { return NBR_ESYSCALL; }
	if (loop::open(*this) < 0) { return NBR_ESYSCALL; }
	return NBR_OK;
}
base::result inotify::on_read(loop &, poller::event &ev) {
	notification occur[loop::maxfd()]; int n_ev;
	if ((n_ev = ::kevent(m_inotify_fd, NULL, 0, occur, loop::maxfd(), NULL)) < 0) {
		if (util::syscall::error_again()) { return read_again; }
		TRACE("poller::wait: %d", util::syscall::error_no());
		return destroy;
	}
	for (int i = 0; i < n_ev; i++) {
		watcher *w = reinterpret_cast<watcher *>(occur[i].udata);
		w->emit(&(occur[i]));
	}
	/* all event fully read? */
	return n_ev < loop::maxfd() ? read_again : keep;
}
#endif
inline fs::event_flags fs::event_flags_from(const char *events) {
	event_flags f = 0;
	char *dup_events = util::str::dup(events), *buff[ms_mapping_size];
	if (!dup_events) { ASSERT(false); return 0; }
	int count = util::str::split(dup_events, ",", buff, countof(buff));
	for (int i = 0; i < count; i++) {
		for (int j = 0; j < ms_mapping_size; j++) {
			if (util::str::cmp_nocase(buff[i],
				ms_mapping[j].m_symbol + 2/* skip first __ */) == 0) {
				f |= ms_mapping[j].m_flag;
				break;
			}
		}
	}
	util::mem::free(dup_events);
	return f;
}
inline const char *fs::symbol_from(event_flags f) {
	for (int j = 0; j < ms_mapping_size; j++) {
		if ((f & ms_mapping[j].m_flag)) {
			return ms_mapping[j].m_symbol;
		}
	}
	return NULL;
}
template <class ITER>
inline void fs::each_flag(event_flags f, ITER it) {
	for (int j = 0; j < ms_mapping_size; j++) {
		if ((f & ms_mapping[j].m_flag)) {
			it(ms_mapping[j].m_symbol);
		}
	}
	return;
}

}
}

#endif
