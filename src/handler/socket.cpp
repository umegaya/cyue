/***************************************************************
 * socket.h : base class for describing normal socket (file, stream, datagram)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "server.h"
#include "handshake.h"
#include "socket.h"

namespace yue {
namespace util {
int handshake::init(int maxfd) {
	if (!m_hsm.init(maxfd / 10, maxfd / 10, -1, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	return server::create_timer(*this, 0.0, 1.0, true) ? NBR_OK : NBR_EEXPIRE;
}
}
namespace handler {
util::handshake socket::m_hs;
}
}
