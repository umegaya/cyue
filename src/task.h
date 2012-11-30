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
class emittable;
namespace task {
struct io {
	U8 m_type, padd; U16 m_serial;
	emittable *m_emitter;
	poller::event m_ev;	
	enum {
		WRITE_AGAIN,
		READ_AGAIN,
		CLOSE,
		TYPE_MAX,
	};
	inline io() {}
	inline io(emittable *e, poller::event &ev, U8 t);
	inline io(emittable *e, U8 t);
	inline void operator () (loop &l);
};
}
}

#endif
