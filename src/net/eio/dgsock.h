/***************************************************************
 * dgsock.h : data gram socket handler
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
#if !defined(__DGSOCK_H__)
#define __DGSOCK_H__

#include "wbuf.h"

namespace yue {
namespace module {
namespace net {
namespace eio {
class dgsock : public wbuf {
	static const U32 MTU = 1492;
public:
	dgsock() {}
	~dgsock() {}
	int init() { return wbuf::init(); }
	template <class EM, class SR> static inline int
	read(EM &em, DSCRPTR fd, yue::util::pbuf &pbf, SR &sr) {
		address a; int r;
		while (true) {
			if (pbf.reserve(MTU) < 0) { return handler_result::destroy; }
			if ((r = syscall::recvfrom(fd, pbf.last_p(), pbf.available(),
				a.addr_p(), a.len_p(), em.proc().from(fd))) < 0) {
				ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
				return (util::syscall::error_again() && r < 0) ? em.again(fd) : em.destroy(fd);
			}
			pbf.commit(r);
			dgram_actor dga(em.proc().wp().get(fd), a);
	retry:
			switch(sr.unpack(pbf)) {
			case SR::UNPACK_SUCCESS:
				if (em.proc().dispatcher().recipient().recv(dga, sr.result()) < 0) {
					return handler_result::destroy;
				}
				return handler_result::keep;
			case SR::UNPACK_EXTRA_BYTES:
				if (em.proc().dispatcher().recipient().recv(dga, sr.result()) < 0) {
					return handler_result::destroy;
				}
				goto retry;
			case SR::UNPACK_CONTINUE:
				return handler_result::keep;
			case SR::UNPACK_PARSE_ERROR:
			default:
				ASSERT(false);
				return handler_result::destroy;
			}
		}
	}
};

}
}
}
}
#endif//__DGSOCK_H__
