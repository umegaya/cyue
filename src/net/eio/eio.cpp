/***************************************************************
 * eio.cpp : event IO loop
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of pfm framework.
 * pfm framework is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * pfm framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#include "eio.h"
#include "parking.h"
#include "sbuf.h"
#include "signalfd.h"
#include "timerfd.h"
#include "types.h"

namespace yue {
namespace module {
namespace net {
namespace eio {
/* wbuf.h */
msgid_generator<U32> wbuf::m_gen;
selector::method *writer::g_write_poller = NULL;
/* eio.h */
transport **loop::basic_processor::m_transport = NULL;
loop::basic_processor::write_poller loop::basic_processor::m_wp;
selector::method *loop::basic_processor::m_rp = NULL;
loop *loop::basic_processor::m_e = NULL;
signalfd loop::basic_processor::m_sig;
timerfd loop::basic_processor::m_timer;
loop::sync_poller loop::m_sync;
/* signalfd.h */
DSCRPTR signalfd::m_pair[2];
signalfd::handler signalfd::m_hmap[signalfd::SIGMAX];
/* parking.h */
transport *parking::INVALID_TRANSPORT = (transport *)0x1;



/* eio.h */
transport *loop::divide_addr_and_transport(
	const char *addr, char *out, int len) {
	const char *p; char proto[256];
	if (!(p = strstr(addr, "://"))) {
		return module::net::eio::parking::invalid();
	}
	util::str::copy(proto, addr, (p - addr) + 1); /* +1 for '\0' (added by str::copy) */
	util::str::copy(out, p + 3, len);
	return from(proto);
}

transport *loop::divide_addr_and_transport(
	const char *addr, address &a) {
	char out[256];
	transport *t = divide_addr_and_transport(
		addr, out, sizeof(out));
	if (!parking::valid(t)) {
		TRACE("invalid address. do you forget to protocol specifier(foo://) to address?\n");
		//ASSERT(false);
		return module::net::eio::parking::invalid();
	}
	if (a.set(out, t) < 0) {
		return module::net::eio::parking::invalid();
	}
	return t;
}

}
}
}
}
