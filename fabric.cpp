/***************************************************************
 * fabric.cpp : implementation of fabric.c
 * (create fiber from serializer::object: stream_dispatcher recipient)
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
#include "fabric.h"
#include "serializer.h"
#include "server.h"

namespace yue {
util::map<fabric::yielded, MSGID> fabric::m_yielded_fibers;
util::map<local_actor, UUID> fabric::m_object_assign_table;
int fabric::m_max_fiber = 0,
	fabric::m_max_object = 0,
	fabric::m_fiber_timeout_us = 0;
util::msgid_generator<U32> serializer::m_gen;
server *fabric::m_server = NULL;

int fabric::tls_init(local_actor &la) {
	int r;
	m_la = la;
	net::set_tls(this);
	return ((r = lang().init(served()->bootstrap_source(), m_max_fiber)) < 0) ?
		r : NBR_OK;
}

}

