/***************************************************************
 * socket.hpp : socket implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__SOCKET_HPP__)
#define __SOCKET_HPP__

#include "socket.h"
#include "endpoint.h"
#include "event.h"

namespace yue {
namespace handler {
void socket::emit(int state) {
	event::session ev(state, this);
	emittable::emit(event::ID_SESSION, ev);
}
void socket::close() {
	set_flag(F_FINALIZED, true);
	base::sched_close();
}
base::result socket::read(loop &l) {
	switch(m_socket_type) {
	case STREAM:	return read_stream(l);
	case DGRAM:		return read_dgram(l);
	case RAW:		return read_raw(l);
	default:		ASSERT(false); return destroy;
	}
}
inline base::result socket::read_raw(loop &l) {
	emit(RECVDATA);
	return nop;	//if anyone try to read this socket and returns EAGAIN, then this socket back to poller.
}
inline base::result socket::read_and_parse(loop &l, int &parse_result) {
	int r;
	if (pbf().reserve(MINIMUM_FREE_PBUF) < 0) {
		ASSERT(false); return destroy;
	}
#if defined(__NOUSE_PROTOCOL_PLUGIN__)
	if ((r = net::syscall::read(fd, pbf().last_p(), pbf().available(), NULL)) < 0) {
		ASSERT(util::syscall::error_again());
		return util::syscall::error_again() ? read_again : destroy;
	}
#else
	/* r == 0 means EOF so we should destroy this DSCRPTR. */
	if ((r = net::syscall::read(m_fd,
		pbf().last_p(), pbf().available(), m_t)) <= 0) {
		//TRACE("syscall::read: errno = %d %d\n", util::syscall::error_no(), r);
		ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
		return (util::syscall::error_again() && r < 0) ? read_again : destroy;
	}
	TRACE("stream_read: read %u byte\n", r);
#endif
	pbf().commit(r);
	parse_result = m_sr.unpack(pbf());
	return nop;
}
inline base::result socket::read_stream(loop &l) {
	int pres; base::result r;
	if ((r = read_and_parse(l, pres)) != nop) {
		return r;
	}
retry:
	ASSERT(m_listener);
	switch(pres) {
	case serializer::UNPACK_SUCCESS: {
		rpc::endpoint::stream ep(this);
		event::proc ev(m_listener);
		ev.m_object = m_sr.result();
		if (reinterpret_cast<server &>(l).fbr().recv(ep, ev) < 0) {
			return destroy;
		}
		return keep;
	}
	case serializer::UNPACK_EXTRA_BYTES: {
		rpc::endpoint::stream ep(this);
		event::proc ev(m_listener);
		ev.m_object = m_sr.result();
		if (reinterpret_cast<server &>(l).fbr().recv(ep, ev) < 0) {
			return destroy;
		}
		pres = m_sr.unpack(pbf());
		goto retry;
	}
	case serializer::UNPACK_CONTINUE:
		return keep;
	case serializer::UNPACK_PARSE_ERROR:
	default:
		ASSERT(false);
		return destroy;
	}
}
inline base::result socket::read_dgram(loop &l) {
	address a; int r;
	while (true) {
		if (pbf().reserve(1492/*MTU*/) < 0) { return destroy; }
		if ((r = net::syscall::recvfrom(m_fd, pbf().last_p(), pbf().available(),
			a.addr_p(), a.len_p(), m_t)) < 0) {
			ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
			return (util::syscall::error_again() && r < 0) ? read_again : destroy;
		}
		pbf().commit(r);
retry:
		switch(m_sr.unpack(pbf())) {
		case serializer::UNPACK_SUCCESS: {
			event::proc ev(this);
			rpc::endpoint::datagram ep(this, a);
			ev.m_object = m_sr.result();
			if (reinterpret_cast<server &>(l).fbr().recv(ep, ev) < 0) {
				return destroy;
			}
			return keep;
		}
		case serializer::UNPACK_EXTRA_BYTES: {
			event::proc ev(this);
			rpc::endpoint::datagram ep(this, a);
			ev.m_object = m_sr.result();
			if (reinterpret_cast<server &>(l).fbr().recv(ep, ev) < 0) {
				return destroy;
			}
			goto retry;
		}
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

