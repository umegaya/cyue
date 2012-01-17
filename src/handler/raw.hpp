/***************************************************************
 * raw.hpp : raw socket IO  implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__RAW_HPP__)
#define __RAW_HPP__

#include "session.h"
#include "fiber.h"

namespace yue {
namespace handler {

inline base::result session::read_raw(loop &l) {
	this->operator () (m_fd, S_RECEIVE_DATA);
	return again;
}
template <class SR, class OBJ>
inline int session::loop_handle::send(SR &sr, OBJ &o) {
	int r;
	if (fabric::pack_as_object(o, sr) < 0) {
		ASSERT(false); return NBR_EINVAL;
	}
	object obj = sr.result();
	fabric::task t(server::tlsv(), obj);
	r = (m_l->fque().mpush(t) ? NBR_OK : NBR_EEXPIRE);
	obj.fin();
	return r;
}

}
}


#endif
