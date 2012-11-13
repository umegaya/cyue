/***************************************************************
 * mac.h : kvs key generator using mac ID
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
#if !defined(__MAC_H__)
#define __MAC_H__

#include "common.h"
#include <memory.h>
#if defined(__ENABLE_DBM__)
#include "dbm.h"
#else
namespace yue {
typedef struct {
	void *dummy;
} dbm;
}
#endif

namespace yue {
namespace module {
namespace uuid {
class mac {
public:
	U8      macaddr[6];
	U16     id1;
	U32     id2;
	static const int strkeylen = 24 + 1;
	static const int disaster_addid = 1000000;
public:
	bool operator == (const mac &uuid) const {
			U32 *p = (U32 *)&uuid, *q = (U32 *)this;
			return p[0] == q[0] && p[1] == q[1] && p[2] == q[2];
	}
	const mac &operator = (const mac &uuid) {
			U32 *p = (U32 *)&uuid, *q = (U32 *)this;
			q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
			return *this;
	}
	const char *to_s(char *b, int bl) const {
			const U32 *p = (const U32 *)this;
			snprintf(b, bl, "%08x%08x%08x", p[0], p[1], p[2]);
			return b;
	}
public:
	mac() { U32 *p = (U32 *)this; p[0] = p[1] = p[2] = 0L; }
	~mac() {}
	static inline void new_id(mac &uuid);
	static inline const mac &invalid_id();
	static inline bool valid(const mac &uuid) {
		return uuid.id1 != 0 || uuid.id2 != 0; }
	static int init(yue::dbm &db);
	static void fin(yue::dbm &db);
	static int save(yue::dbm &db);
	static int load(yue::dbm &db);
private:
	mac(const mac &id) { *this = id; }
};

inline const mac &
mac::invalid_id()
{
	extern mac *invalid_uuid();
	return *invalid_uuid();
}

inline void
mac::new_id(mac &uuid)
{
	extern mac *seed_uuid();
	mac &s = *seed_uuid();
	if (0 == (uuid.id2 = __sync_add_and_fetch(&(s.id2), 1))) {
		uuid.id1 = s.id1++;
	}
	U32 *p = (U32 *)&s;
	U32 *q = (U32 *)&uuid;
	q[0] = p[0];
	U16 *pw = (U16 *)&s;
	U16 *qw = (U16 *)&uuid;
	qw[2] = pw[2];
}
}
}
}

#endif

