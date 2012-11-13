/***************************************************************
 * socket.h : base class for describing normal socket (file, stream, datagram)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "server.h"
#include "socket.h"

namespace yue {
namespace handler {
util::handshake socket::m_hs;
const char *socket::state_symbols[socket::MAX_STATE] = {
	NULL, 			//socket::HANDSHAKE,
	"__open",		//socket::WAITACCEPT,
	"__establish",	//socket::ESTABLISH,
	"__data", 		//socket::RECVDATA,
	"__close",		//socket::CLOSED,
};
}
}
