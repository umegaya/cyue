/***************************************************************
 * stream.hpp : message stream implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__STREAM_HPP__)
#define __STREAM_HPP__

#include "session.h"

namespace yue {
namespace handler {

inline base::result session::read_and_parse(loop &l, int &parse_result) {
	int r;
	if (pbf().reserve(MINIMUM_FREE_PBUF) < 0) {
		ASSERT(false); return destroy;
	}
#if defined(__NOUSE_PROTOCOL_PLUGIN__)
	if ((r = net::syscall::read(fd, pbf().last_p(), pbf().available(), NULL)) < 0) {
		ASSERT(util::syscall::error_again());
		return util::syscall::error_again() ? again : destroy;
	}
#else
	/* r == 0 means EOF so we should destroy this DSCRPTR. */
	if ((r = net::syscall::read(m_fd,
		pbf().last_p(), pbf().available(), loop::tl()[m_fd])) <= 0) {
		//TRACE("syscall::read: errno = %d %d\n", util::syscall::error_no(), r);
		ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
		return (util::syscall::error_again() && r < 0) ? again : destroy;
	}
	TRACE("stream_read: read %u byte\n", r);
#endif
	pbf().commit(r);
	parse_result = m_sr.unpack(pbf());
	return nop;
}
inline base::result session::read_stream(loop &l) {
	int pres; base::result r;
	if ((r = read_and_parse(l, pres)) != nop) {
		return r;
	}
	session::stream_handle h(this);
retry:
	switch(pres) {
	case serializer::UNPACK_SUCCESS:
		if (reinterpret_cast<server &>(l).fbr().recv(h, m_sr.result()) < 0) {
			return destroy;
		}
		return keep;
	case serializer::UNPACK_EXTRA_BYTES:
		if (reinterpret_cast<server &>(l).fbr().recv(h, m_sr.result()) < 0) {
			return destroy;
		}
		pres = m_sr.unpack(pbf());
		goto retry;
	case serializer::UNPACK_CONTINUE:
		return keep;
	case serializer::UNPACK_PARSE_ERROR:
	default:
		ASSERT(false);
		return destroy;
	}
}

}
}

#endif
