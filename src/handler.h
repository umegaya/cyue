/***************************************************************
 * handler.h : abstruct interface of socket handler
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__HANDLER_H__)
#define __HANDLER_H__

#include "selector.h"
#include "msgid.h"
#include "emittable.h"

struct transport;
namespace yue {
class loop;
namespace handler {
#define NON_VIRTUAL
#if !defined(NON_VIRTUAL)
#define INTERFACE virtual
#else
#define INTERFACE inline
#endif
#if defined(_DEBUG)
#define DEBUG_SET_CLOSE(h) {h->debug_set_close(__FILE__,__LINE__);}
#define CLEAR_SET_CLOSE(h) {h->clear_set_close();}
#else
#define DEBUG_SET_CLOSE(h)
#define CLEAR_SET_CLOSE(h)
#endif
class base : public emittable {
protected:
#if defined(_DEBUG)
	struct debug_close_info {
		const char *file; int line;
	} m_dci;
#endif
public:
	enum {	/* handler type */
		LISTENER,
		TIMER,
		SIGNAL,
		SOCKET,
		WPOLLER,
		FILESYSTEM,
		FSWATCHER,

		HANDLER_TYPE_MAX,
	};
	typedef enum {
		write_again = 2,
		read_again = 1,
		destroy = -1,
		keep = 0,
		nop = -2,
	} result;
	inline base(U8 type) : emittable(type) {
#if defined(_DEBUG)
		m_dci.file = NULL;
#endif
	}
#if defined(_DEBUG)
	inline void debug_set_close(const char *f, int l) { 
		ASSERT(!m_dci.file);
		m_dci.file = f; m_dci.line = l; 
	}
	inline void clear_set_close() { m_dci.file = NULL; }
	inline const char *file() const { return m_dci.file; }
	inline int line() const { return m_dci.line; }
#endif
	inline U8 type() const { return m_type; }
	inline void sched_unref();
	inline void sched_close();
	inline void sched_read(DSCRPTR fd);
	INTERFACE ~base() {}
	INTERFACE DSCRPTR fd();
	INTERFACE transport *t();
	INTERFACE DSCRPTR on_open(U32 &);
	INTERFACE void on_close();
	INTERFACE void close();
	INTERFACE result on_read(loop &, poller::event &);
	INTERFACE result on_write(poller &);
};
}
}
#endif
