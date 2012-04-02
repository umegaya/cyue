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
class base {
public:
	typedef U16 handler_serial;
protected:
	static util::msgid_generator<handler_serial> m_gen;
	U8 m_type, padd;
	handler_serial m_serial;
#if defined(_DEBUG)
	struct debug_close_info {
		const char *file; int line;
	} m_dci;
#endif
public:
	enum {
		LISTENER,
		TIMER,
		SIGNAL,
		SESSION,
		WPOLLER,
	};
	typedef enum {
		again_rw = 2,
		again = 1,
		destroy = -1,
		keep = 0,
		nop = -2,
	} result;
	inline base(U8 type) : m_type(type), m_serial(m_gen.new_id()) {
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
	inline handler_serial serial() const { return m_serial; }
	INTERFACE ~base() {}
	INTERFACE DSCRPTR on_open(U32 &, transport **);
	INTERFACE void on_close();
	INTERFACE result on_read(loop &, poller::event &);
	INTERFACE result on_write(poller &, DSCRPTR);
};
}
}
#endif
