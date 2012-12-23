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
	if (m_fd != INVALID_FD) {
		/* m_fd processing state is following 2 pattern:
		 * 1. processed by one of the worker thread. m_fd is not registered selector fd-set.
		 * 2. waiting for event in selector fd-set.
		 * for 1, we check F_FINALIZED flag in on_read handler and handler do close.
		 * for 2, we need to dispatch close in this function.
		 * to check m_fd state is 1 or 2, we try to use fd delete facility of selector.
		 * if fail to delete, state is 1. otherwise 2. and also if delete is success,
		 * we can assure that another worker thread never start to process this fd
		 * (because this fd never appeared in event list from selector)
		 *
		 * TODO: there is one edge case that this connection not closed immediately.
		 * if loop::read do retach just after detach call (because on_read returns read_again), 
		 * base::sched_close() not called.
		 * but if some read event happen on this fd, immediately on_read returns destroy, so connection closed.
		 * otherwise that means this fd never receive any packet, 
		 * so finally TCP timeout close this socket... (this is the worst case) 
		 * so if take some time, it is no problem but I want to fix such an unexpected behavior.
		 *
		 * another problem, for epoll system, detach (EPOLL_CTL_DEL) still success even if some thread processing
		 * corresponding fd.
		 */
		if (loop::tls()->p().detach(m_fd) < 0) {
			ASSERT(util::syscall::error_no() == ENOENT);
			TRACE("this fd %d already processed by worker thread. wait for destroy\n", m_fd);
		}
		else {
			/* for epoll system, indicate close event already dispatched */
			set_flag(F_CLOSED, true);
			base::sched_close();
			TRACE("this fd %d in fd-set and wait for event. dispatch close now\n", m_fd);
		}
	}
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
	ASSERT(ns_key());
	switch(pres) {
	case serializer::UNPACK_SUCCESS: {
		rpc::endpoint::stream ep(this);
		event::proc ev(ns_key());
		ev.m_object = m_sr.result();
		if (reinterpret_cast<server &>(l).fbr().recv(ep, ev) < 0) {
			return destroy;
		}
		return keep;
	}
	case serializer::UNPACK_EXTRA_BYTES: {
		rpc::endpoint::stream ep(this);
		event::proc ev(ns_key());
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

