/***************************************************************
 * monitor.h : session event monitor callback management
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__MONITOR_H__)
#define __MONITOR_H__
namespace yue {
using namespace util;
namespace handler {
class session;
class monitor {
public:
	typedef yue::util::functional<bool (session *, int)> watcher;
	struct watch_entry {
		watch_entry(watcher &w) : m_w(w) {}
		watcher m_w;
		struct watch_entry *m_next, *m_prev;
	};
	static bool nop(session *, int) { return false; }
	static const bool KEEP_WATCH = true;
	static const bool STOP_WATCH = false;
protected:
	static array<watch_entry> m_wl;
	static struct watch_entry *m_gtop;
	static thread::mutex m_gmtx;
	struct watch_entry *m_top;
	thread::mutex m_mtx;
public:
	inline monitor() : m_top(NULL) {}
	static int static_init(int maxfd) {
		if (m_gmtx.init() < 0) { return NBR_EPTHREAD; }
		return m_wl.init(maxfd, -1, opt_threadsafe | opt_expandable) ?
				NBR_OK : NBR_EMALLOC;
	}
	static void static_fin() {
		m_gmtx.fin();
		m_wl.fin();
	}
	inline int init() { return m_mtx.init(); }
	inline void fin() { m_mtx.fin(); }
	inline void notice(session *s, int state) {
		notice(s, m_top, m_mtx, state);
		notice(s, m_gtop, m_gmtx, state);
	}
	void notice(session *s, watch_entry *&top, thread::mutex &mtx, int state) {
		if (!top) { return; }
		//TRACE("notice\n");
		util::thread::scoped<util::thread::mutex> lk(mtx);
		if (lk.lock() < 0) {
			DIE("mutex lock fails (%d)\n", util::syscall::error_no());
			return;
		}
		watch_entry *w = top, *pw, *tw = NULL, *last = NULL;
		top = NULL;
		lk.unlock();
		while((pw = w)) {
			w = w->m_next;
			if (!pw->m_w(s, state)) { m_wl.free(pw); }
			else {
				if (!last) { last = pw; }
				pw->m_next = tw;
				tw = pw;
			}
		}
		if (last) {
			if (lk.lock() < 0) {
				DIE("mutex lock fails (%d)\n", util::syscall::error_no());
				return;
			}
			ASSERT(tw);
			last->m_next = top;
			top = tw;
		}
	}
	static inline int add_static_watcher(watcher &wh) {
		return add_watcher(m_gtop, wh, m_gmtx);
	}
	static inline int add_watcher(watch_entry *&we, watcher &wh, thread::mutex &mtx) {
		watch_entry *w = m_wl.alloc(wh);
		w->m_next = we;
		w->m_w = wh;
		//TRACE("add watcher %p\n", w);
		util::thread::scoped<util::thread::mutex> lk(mtx);
		if (lk.lock() < 0) {
			m_wl.free(w);
			return NBR_EPTHREAD;
		}
		we = w;
		return NBR_OK;
	}
	inline int add_watcher(watcher &wh) {
		return add_watcher(m_top, wh, m_mtx);
	}
};
}
}
#endif
