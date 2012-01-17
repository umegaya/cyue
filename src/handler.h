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

namespace yue {
class loop;
namespace handler {
#define NON_VIRTUAL
#if !defined(NON_VIRTUAL)
#define INTERFACE virtual
#else
#define INTERFACE inline
#endif
class base {
	U8 m_type, padd[3];
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
	inline base(U8 type) : m_type(type) {}
	inline U8 type() const { return m_type; }
	INTERFACE ~base() {}
	INTERFACE DSCRPTR on_open(U32 &, transport **);
	INTERFACE void on_close();
	INTERFACE result on_read(loop &, poller::event &);
	INTERFACE result on_write(poller &, DSCRPTR);
};
}
}
#endif
