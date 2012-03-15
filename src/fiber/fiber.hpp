/***************************************************************
 * fiber.hpp : fiber/fabric handler implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__FIBER_HPP__)
#define __FIBER_HPP__

#include "server.h"

namespace yue {
fabric &fabric::tlf() {
	return server::tlsv()->fbr();
}
inline int fabric::delegate(fiber *f, object &o) {
	task t(f, o);
	return m_server->fque().mpush(t) ? fiber::exec_delegate : fiber::exec_error;
}
inline int fabric::delegate(fiber::handler &fh, object &o) {
	task t(fh, o);
	return m_server->fque().mpush(t) ? fiber::exec_delegate : fiber::exec_error;
}
inline int fabric::delegate(fiber::phandler &ph, void *p) {
	task t(ph, p);
	return m_server->fque().mpush(t) ? fiber::exec_delegate : fiber::exec_error;
}
inline int fabric::delegate(server *s, object &o) {
	task t(s, o);
	return m_server->fque().mpush(t) ? fiber::exec_delegate : fiber::exec_error;
}
inline void fabric::task::operator () (server &s) {
	switch(type) {
	case FIBER:
		fb().m_f->resume(s.fbr(), fb().m_o);
		fb().m_o.fin();
		break;
	case HANDLER:
		hd().m_h(s.fbr(), hd().m_o);
		hd().m_o.fin();
		break;
	case PHANDLER: phd().m_h(s.fbr(), phd().m_p); break;
	case SERVER: {
		handler::session::loop_handle lh(sv().m_s);
		s.fbr().recv(lh, sv().m_o);	//m_o will free in fbr().recv.
	} break;
	}
}
template <class RESP>
inline int fiber::send_handler(serializer &sr, RESP &resp) {
	int r; fabric &fbr = fabric::tlf();
	if (fabric::pack_as_object(resp, sr) < 0) {
		fbr.set_last_error(NBR_EFORMAT, fiber::msgid(), c_nil());
		if (fabric::pack_as_object(fbr.last_error(), sr) < 0) {
			object o = sr.result();
			r = handler_ref().operator () (fbr, o);
			o.fin();
			return NBR_EFORMAT;
		}
		ASSERT(false); return NBR_EINVAL;
	}
	object o = sr.result();
	r = handler_ref().operator () (fbr, o);
	o.fin();
	return r;
}
template <class RESP>
inline int fiber::send_loop(serializer &sr, RESP &resp) {
	int r;
	if (fabric::pack_as_object(resp, sr) < 0) {
		fabric &fbr = fabric::tlf();
		fbr.set_last_error(NBR_EFORMAT, fiber::msgid(), c_nil());
		if (fabric::pack_as_object(fbr.last_error(), sr) < 0) {
			object o = sr.result();
			fabric::task t(fbr.served(), o);
			r = (thread_ref().m_l->fque().mpush(t) ? NBR_OK : NBR_EEXPIRE);
			o.fin();
			return r;
		}
		ASSERT(false); return NBR_EINVAL;
	}
	object o = sr.result();
	fabric::task t(server::tlsv(), o);
	r = (thread_ref().m_l->fque().mpush(t) ? NBR_OK : NBR_EEXPIRE);
	o.fin();
	return r;
}
}

#endif
