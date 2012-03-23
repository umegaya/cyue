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
	class watcher : public yue::util::functional<bool (session *, int)> {
	public:
		typedef yue::util::functional<bool (session *, int)> super;
		U8 m_dead, padd[3];
		template <class FUNCTOR> inline watcher(FUNCTOR &f) :
			super(f), m_dead(0) { f.set_watcher(this); }
		inline void kill() { m_dead = 1; }

	};
	struct watch_entry {
		template <class FUNCTOR> watch_entry(FUNCTOR &f) : m_w(f) {}
		inline bool dead() const { return m_w.m_dead; }
		watcher m_w;
		struct watch_entry *m_next, *m_prev;
	};
	struct nop {
		inline bool operator () (session *, int) { return false; }
		inline void set_watcher(watcher *) {}
	};
	static const bool KEEP = true;
	static const bool STOP = false;
	static const size_t NOTICE_STACK = 2;
protected:
	static array<watch_entry> m_wl;
	static struct watch_entry *m_gtop;
	static thread::mutex m_gmtx;
	struct watch_entry *m_top;
	thread::mutex m_mtx;
	struct m_notice_stack {
		U8 m_stack[NOTICE_STACK], m_height, padd;
	} m_notices;
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
		m_gtop = NULL;
	}
	inline int init() {
		m_notices.m_height = 0;
		return m_mtx.init();
	}
	inline void fin() {
		util::thread::scoped<util::thread::mutex> lk(m_mtx);
		if (lk.lock() < 0) {
			TRACE("mutex lock fails (%d)\n", util::syscall::error_no());
			ASSERT(false);
		}
		watch_entry *w = m_top, *pw;
		m_top = NULL;
		lk.unlock();
		while((pw = w)) {
			w = w->m_next;
			m_wl.free(pw);
		}
		m_mtx.fin();
	}
	inline void notice(session *s, int state) {
		ASSERT(m_notices.m_height < NOTICE_STACK);
		m_notices.m_stack[m_notices.m_height++] = (U8)state;
		if (m_notices.m_height > 1) {
			return;	/* wait for previous notice finished */
		}
		exec_notice(s, state);
		if (m_notices.m_height > 1) {
			for (int i = 1; i < (int)m_notices.m_height; i++) {
				exec_notice(s, m_notices.m_stack[i]);
			}
		}
		m_notices.m_height = 0;
	}
	inline void exec_notice(session *s, int state) {
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
			if (pw->dead() || !pw->m_w(s, state)) { m_wl.free(pw); }
			else {
				if (last) { last->m_next = pw; }
				else { ASSERT(!tw); tw = pw; }
				last = pw;
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
	template <class FUNCTOR>
	static inline int add_static_watcher(FUNCTOR &f) {
		return add_watcher(m_gtop, f, m_gmtx);
	}
	template <class FUNCTOR>
	static inline int add_watcher(watch_entry *&we, FUNCTOR &f, thread::mutex &mtx) {
		watch_entry *w = m_wl.alloc(f);
		return add_watcher_entry(we, w, mtx);
	}
	static inline int add_watcher_entry(watch_entry *&we, watch_entry *w, thread::mutex &mtx) {
		w->m_next = we;
		//TRACE("add watcher %p\n", w);
		util::thread::scoped<util::thread::mutex> lk(mtx);
		if (lk.lock() < 0) {
			m_wl.free(w);
			return NBR_EPTHREAD;
		}
		we = w;
		return NBR_OK;
	}
	template <class FUNCTOR>
	inline int add_watcher(FUNCTOR &f) {
		return add_watcher(m_top, f, m_mtx);
	}
};
}
}
#endif
