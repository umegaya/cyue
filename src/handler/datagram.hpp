/***************************************************************
 * datagram.hpp : datagram message implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__DATAGRAM_HPP__)
#define __DATAGRAM_HPP__

#include "session.h"

namespace yue {
namespace handler {

inline base::result session::read_dgram(loop &l) {
	address a; int r;
	while (true) {
		if (pbf().reserve(1492/*MTU*/) < 0) { return destroy; }
		if ((r = net::syscall::recvfrom(m_fd, pbf().last_p(), pbf().available(),
			a.addr_p(), a.len_p(), loop::tl()[m_fd])) < 0) {
			ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
			return (util::syscall::error_again() && r < 0) ? again : destroy;
		}
		pbf().commit(r);
		session::datagram_handle h(this, a);
retry:
		switch(m_sr.unpack(pbf())) {
		case serializer::UNPACK_SUCCESS:
			if (reinterpret_cast<server &>(l).fbr().recv(h, m_sr.result()) < 0) {
				return destroy;
			}
			return keep;
		case serializer::UNPACK_EXTRA_BYTES:
			if (reinterpret_cast<server &>(l).fbr().recv(h, m_sr.result()) < 0) {
				return destroy;
			}
			goto retry;
		case serializer::UNPACK_CONTINUE:
			return keep;
		case serializer::UNPACK_PARSE_ERROR:
		default:
			ASSERT(false);
			return destroy;
		}
	}
	return destroy;
}

}
}


#endif
