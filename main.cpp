/***************************************************************
 * main.cpp
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
#include "server.h"

using namespace yue;

int main(int argc, char *argv[]) {
#if !defined(__NO_TEST__)
	if (util::mem::cmp(argv[1], "-t=", 3) == 0) {
		extern int test(char *, int, char *[]);
		return test(argv[1] + 3, argc, argv);
	}
#endif
	server s;
	if (s.init(argv[1]) < 0) { return 0; }
	return s.run();
}

