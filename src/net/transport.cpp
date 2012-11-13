/***************************************************************
 * transport.cpp : implementation of TCP, UDP transport
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
#include "common.h"
#include "parking.h"
#include "syscall.h"
#include "address.h"
#include "syscall.h"
#include "map.h"
#if defined(__ENABLE_SENDFILE__)
#include <sys/sendfile.h>
#else
#define sendfile NULL
#endif
#include "serializer.h"
#if defined(__NBR_OSX__)
#include <sys/uio.h>
#endif

namespace yue {
namespace net {
//transport *parking::INVALID_TRANSPORT = reinterpret_cast<transport *>(0xdeadbeef);
transport *parking::divide_addr_and_transport(
	const char *addr, char *out, int len) {
	const char *p; char proto[256];
	if (!(p = strstr(addr, "://"))) {
		return net::parking::invalid();
	}
	util::str::copy(proto, addr, (p - addr) + 1); /* +1 for '\0' (added by str::copy) */
	util::str::copy(out, p + 3, len);
	return find_ptr(proto);
}

transport *parking::divide_addr_and_transport(
	const char *addr, address &a) {
	char out[256];
	transport *t = divide_addr_and_transport(
		addr, out, sizeof(out));
	if (!parking::valid(t)) {
		TRACE("invalid address(%s). do you forget to protocol specifier(foo://) to address?\n", addr);
		//ASSERT(false);
		return net::parking::invalid();
	}
	if (a.set(out, t) < 0) {
		return net::parking::invalid();
	}
	return t;
}
}
}

#include "protocol/tcp.hpp"
#include "protocol/udp.hpp"
#include "protocol/mcast.hpp"
#include "protocol/popen.hpp"
#include "protocol/ws.hpp"

