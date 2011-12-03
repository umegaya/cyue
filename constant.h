/***************************************************************
 * constant.h : constant definitions
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
#if !defined(__CONSTANT_H__)
#define __CONSTANT_H__

namespace yue {
namespace constant {
class fiber {
public:
	enum {
		exec_error,	/* unrecoverable error happen */
		exec_finish,/* procedure finish (it should reply to caller actor) */
		exec_yield, /* procedure yields. (will resume again) */
		exec_delegate,/* fiber execution is delegate to another native thread.
					because another native thread should process it.
					(eg. object method call which handled by different thread
					=> normally object loaded on virtual machine of specified thread (owner of object).
					so another thread receive rpc call for the object, it should be
					passed to owner thread)*/
	};
};
}
}

#endif
