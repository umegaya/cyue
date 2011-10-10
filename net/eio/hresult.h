/***************************************************************
 * handler.h : definition for event IO handler
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
#if !defined(__HRESULT_H__)
#define __HRESULT_H__

namespace yue {
namespace module {
namespace net {
namespace eio {
struct handler_result {
	typedef enum {
		again_rw = 2,
		again = 1,
		destroy = -1,
		keep = 0,
		nop = -2,
	} result;
};
enum {
	S_ESTABLISH,
	S_SVESTABLISH,
	S_EST_FAIL,
	S_SVEST_FAIL,
	S_CLOSE,
	S_SVCLOSE,
};
}
}
}
}

#endif
