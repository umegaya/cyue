/***************************************************************
 * task.h : task object
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__TASK_H__)
#define __TASK_H__

#include "selector.h"

namespace yue {
namespace task {
struct io {
	U8 type, padd;
	U16 m_serial;
	union {
		poller::event m_ev;
		DSCRPTR m_fd;
	};
	enum {
		WRITE_AGAIN,
		READ_AGAIN,
		CLOSE,
		TYPE_MAX,
	};
	inline io() {}
	inline io(DSCRPTR fd, U16 sn) : type(CLOSE), m_serial(sn), m_fd(fd) {}
	inline io(poller::event &ev, U8 t, U16 sn) : type(t), m_serial(sn), m_ev(ev) {}
	inline io(U8 t, U16 sn) : type(t), m_serial(sn) { poller::init_event(m_ev); }
	inline void operator () (loop &l);
	inline U16 serial() const { return m_serial; }
};
}
}

#endif
