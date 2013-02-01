/***************************************************************
 * fs.h : file system watcher
 * 2012/09/09 iyatomi : create
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__FS_H__)
#define __FS_H__

#include "task.h"
#include "serializer.h"
#include "sbuf.h"
#if defined(__ENABLE_INOTIFY__)
#include <sys/inotify.h>
#elif defined(__ENABLE_KQUEUE__)
//TODO : support kqueue
#include <sys/event.h>
#else
#error platform cannot support fs module
#endif

namespace yue {
namespace handler {


#if defined(__ENABLE_INOTIFY__)
class inotify : public base {
public:
	static const U32 MINIMUM_FREE_FSEVENT_QUEUE_BUFFER = 1024;
	static const int DEFAULT_INOTIFY_WATCHER_COUNT_HINT = 16;
	typedef U32 event_flags;
	static const U32 FLAG_NOT_SUPPORT = 0;
	static const char *UNDERLYING;
	static struct event_map {
		event_flags m_flag;
		const char *m_symbol;
	} ms_mapping[];
	static int ms_mapping_size;
	struct notification {
		sbuf *m_sbuf;
		inotify_event *m_buf;
	public:
		inline notification(sbuf *sbf, pbuf *pbf) : m_sbuf(sbf) {
			m_buf = reinterpret_cast<inotify_event *>(pbf->p());
		}
		inline void *operator new (size_t sz, void *p) {
			ASSERT(sz == sizeof(notification)); return p;
		}
		inline ~notification() {
			sbuf *sbf = m_sbuf;
			if (__sync_bool_compare_and_swap(&m_sbuf, sbf, NULL)) {
				if (sbf) { delete sbf; }
			}
		}
		inline bool flag(event_flags f) const { return flags() & f; }
		inline event_flags flags() const { return m_buf->mask; }
		inline const char *path() const { return m_buf->name; }
	};
	class watcher : public base {
		DSCRPTR m_fd;
		pbuf m_pbuf;
	public:
		watcher(DSCRPTR fd) : base(FSWATCHER), m_fd(fd), m_pbuf() {}
		inline pbuf &pbf() { return m_pbuf; }
		inline void fin() {
			if (m_fd != INVALID_FD) {
				::close(m_fd);
				m_fd = INVALID_FD;
			}
		}
		INTERFACE DSCRPTR fd() { return m_fd; }
		INTERFACE transport *t() { return NULL; }
		INTERFACE DSCRPTR on_open(U32 &) { return m_fd; }
		INTERFACE void on_close() { fin(); }
		INTERFACE void close();
		enum parse_result {
			SUCCESS,
			EXTRA_BYTE,
			CONTINUE,
			PARSE_ERROR,
		};
		static inline parse_result parse(pbuf &pbf, notification **rv) {
			sbuf *sbf; void *p; parse_result r = CONTINUE;
			if (pbf.last() < sizeof(inotify_event)) { return r; }
	
			inotify_event &d = *(reinterpret_cast<inotify_event *>(pbf.p()));
			if ((sizeof(inotify_event) + d.len) > pbf.last()) { return r; }
			else if ((sizeof(inotify_event) + d.len) < pbf.last()) { r = EXTRA_BYTE; }
			else { r = SUCCESS; }
	
			if (!(sbf = new sbuf())) { goto error; }
			if (!(p = sbf->malloc(sizeof(notification)))) { goto error; }
			if (!(*rv = new(p) notification(sbf->refer(pbf), &pbf))) { goto error; }
			return r;
		error:
			if (sbf) { delete sbf; }
			return PARSE_ERROR;
		}
		INTERFACE result on_read(loop &, poller::event &ev) {
			int r;
			notification *pev;
			if (m_pbuf.reserve(MINIMUM_FREE_FSEVENT_QUEUE_BUFFER) < 0) {
				ASSERT(false); return destroy;
			}
			if ((r = ::read(m_fd, pbf().last_p(), pbf().available())) < 0) {
				ASSERT(util::syscall::error_again());
				return util::syscall::error_again() ? read_again : destroy;
			}
			pbf().commit(r);
		retry:
			switch(parse(pbf(), &pev)) {
			case SUCCESS:		emit(pev); break;
			case EXTRA_BYTE: 	emit(pev); goto retry;
			case CONTINUE: 		break;
			case PARSE_ERROR:	return destroy;
			}
			return keep;
		}
		inline void emit(notification *ev);
	};
protected:
	DSCRPTR m_inotify_fd;
	array<watcher> m_pool;
public:
	inotify() : base(FILESYSTEM), m_inotify_fd(INVALID_FD), m_pool() {}
	~inotify() {}
	inline DSCRPTR create_inotify_fd() {
#if defined(IN_NONBLOCK)
		return inotify_init1(IN_NONBLOCK);
#else
		return inotify_init();
#endif
	}
	inline int init(poller *) {
		int flags = util::opt_threadsafe | util::opt_expandable;
		if (!m_pool.init(DEFAULT_INOTIFY_WATCHER_COUNT_HINT, -1, flags)) { return NBR_EMALLOC; }
		return ((m_inotify_fd = create_inotify_fd()) >= 0 ? NBR_OK : NBR_ESYSCALL);
	}
	inline void fin() {
		for (array<watcher>::iterator p = m_pool.begin();
			p != m_pool.end();
			p = m_pool.next(p)) {
			p->fin();
		}
		m_pool.fin();
		if (m_inotify_fd != INVALID_FD) {
			::close(m_inotify_fd);
			m_inotify_fd = INVALID_FD;
		}
	}
	array<watcher> &pool() { return m_pool; }
	inline watcher *watch(const char *path, event_flags f);
	void unwatch(watcher *ptr) {
		inotify_rm_watch(m_inotify_fd, ptr->fd());
		ptr->fin();
	}
	INTERFACE DSCRPTR fd() { return m_inotify_fd; }
	INTERFACE transport *t() { return NULL; }
	INTERFACE DSCRPTR on_open(U32 &) { ASSERT(false); return m_inotify_fd; }
	INTERFACE void on_close() { ASSERT(false); }
	INTERFACE result on_read(loop &, poller::event &ev) { ASSERT(false); return read_again; }
	INTERFACE void close() { ASSERT(false); }
};


#elif defined(__ENABLE_KQUEUE__)
class inotify : public base {
public:
	typedef U32 event_flags;
	static const U32 FLAG_NOT_SUPPORT = 0;
	static const int DEFAULT_INOTIFY_WATCHER_COUNT_HINT = 16;
	static const char *UNDERLYING;
	static struct event_map {
		event_flags m_flag;
		const char *m_symbol;
	} ms_mapping[];
	static int ms_mapping_size;
	struct notification : public kevent {
		inline bool flag(event_flags f) const { return flags() & f; }
		inline event_flags flags() const { return kevent::fflags; }
		inline const char *path() const { return ""; }
	};
	class watcher : public base {
		DSCRPTR m_fd;
	public:
		watcher(DSCRPTR fd) : base(FSWATCHER), m_fd(fd) {}
		inline void fin() {
			if (m_fd != INVALID_FD) {
				::close(m_fd);
				m_fd = INVALID_FD;
			}
		}
		inline void emit(notification *ev);
		INTERFACE DSCRPTR fd() { return m_fd; }
		INTERFACE transport *t() { return NULL; }
		INTERFACE DSCRPTR on_open(U32 &) { ASSERT(false); return m_fd; }
		INTERFACE void on_close() { ASSERT(false); fin(); }
		INTERFACE result on_read(loop &, poller::event &ev) { ASSERT(false); return read_again; }
		INTERFACE void close();
	};
protected:
	DSCRPTR m_inotify_fd;
	array<watcher> m_pool;
public:
	inotify() : base(FILESYSTEM), m_inotify_fd(INVALID_FD), m_pool() {}
	~inotify() {}
	inline int init(poller *);
	inline void fin() {
		for (array<watcher>::iterator p = m_pool.begin(), pp; p != m_pool.end();) {
			pp = p;
			p = m_pool.next(p);
			unwatch(&(*pp));
		}
		m_pool.fin();
		if (m_inotify_fd != INVALID_FD) {
			::close(m_inotify_fd);
			m_inotify_fd = INVALID_FD;
		}
	}
	array<watcher> &pool() { return m_pool; }
	inline watcher *watch(const char *path, event_flags f) {
		struct kevent ev; FILE *fp;
		DSCRPTR fd = INVALID_FD; watcher *ptr = NULL;
		if (f == 0) { goto error; }
		if (!(fp = fopen(path, "r"))) { goto error; }
		fd = fileno(fp);
		EV_SET(&ev, fd, EVFILT_VNODE, EV_ENABLE, f, 0, this);
		if (::kevent(m_inotify_fd, &ev, 1, NULL, 0, NULL) < 0) { goto error; }
		if (!(ptr = m_pool.alloc(fd))) { goto error; }
		return ptr;
	error:
		if (ptr) { 
			unwatch(ptr); 
			m_pool.free(ptr);
		}
		else if (fd != INVALID_FD) { ::close(fd); }
		return NULL;
	}
	void unwatch(watcher *ptr) {
		struct kevent ev;
		EV_SET(&ev, ptr->fd(), EVFILT_VNODE, EV_DELETE, 0, 0, this);
		::kevent(m_inotify_fd, &ev, 1, NULL, 0, NULL);
		ptr->fin();
	}
	INTERFACE DSCRPTR fd() { return m_inotify_fd; }
	INTERFACE transport *t() { return NULL; }
	INTERFACE DSCRPTR on_open(U32 &) { return m_inotify_fd; }
	INTERFACE void on_close() { fin(); }
	INTERFACE result on_read(loop &, poller::event &ev);
};
#endif
class fs : public inotify {
public:
	static inline const char *underlying() { return UNDERLYING; }
	static inline event_flags event_flags_from(const char *events);
	static inline const char *symbol_from(event_flags f);
	template <class ITER> static inline void each_flag(event_flags f, ITER it);
};


}
}

#endif
