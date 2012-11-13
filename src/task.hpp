/***************************************************************
 * task.hpp : task object handler implement
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__TASK_HPP__)
#define __TASK_HPP__

#include "task.h"

namespace yue {
namespace task {
inline io::io(emittable *e, poller::event &ev, U8 t) : 
	m_type(t), m_emitter(e), m_ev(ev) { REFER_EMPTR(e); }
inline io::io(emittable *e, U8 t) :
        m_type(t), m_emitter(e) { REFER_EMPTR(e); }
inline void io::operator () (loop &l) {
	handler::base *h = reinterpret_cast<handler::base *>(m_emitter);
	switch(m_type) {
	case WRITE_AGAIN: {
		l.write(*h);
		UNREF_EMPTR(h);
	} break;
	case READ_AGAIN: {
		l.read(*h, m_ev);
		UNREF_EMPTR(h);
	} break;
	case CLOSE: {
		//TRACE("fd=%d closed\n", m_fd);
		TRACE("fd=%d closed from %s(%u)\n", h->fd(), h->file(), h->line());
		l.close(*h);
		UNREF_EMPTR(h);
	} break;
	default: ASSERT(false); break;
	}
}
}
}

#endif
