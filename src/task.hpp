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
inline void io::operator () (loop &l) {
	switch(type) {
	case WRITE_AGAIN: {
		l.write(m_ev, m_serial);
	} break;
	case READ_AGAIN: {
		l.read(m_ev, m_serial);
	} break;
	case CLOSE: {
		TRACE("fd=%d closed\n", m_fd);
		handler::base *h = l.hl()[m_fd];
		if (h && h->serial() == m_serial) {
			l.close(m_fd);
		}
	} break;
	default: ASSERT(false); break;
	}
}
}
}

#endif
