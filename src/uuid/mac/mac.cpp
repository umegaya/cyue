/***************************************************************
 * mac.cpp : kvs key generator using mac ID
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
#include "mac.h"
#include "syscall.h"

namespace yue {
namespace module {
namespace uuid {

static mac UUID_INVALID;
static mac UUID_SEED;

extern mac *invalid_uuid() { return &(UUID_INVALID); }
extern mac *seed_uuid() { return &(UUID_SEED); }

int mac::init(yue::dbm &db)
{
	int r;
	if ((r = load(db)) < 0) {
		if (r != NBR_ENOTFOUND) {
			return r;	/* not found means need initialize */
		}
		char ifc[] = "eth0";
		if ((r = util::syscall::get_macaddr(ifc, UUID_SEED.macaddr)) < 0) {
			return r;
		}
	}
	char buf[256];
	fprintf(stderr,"UUID>current seed %s\n", UUID_SEED.to_s(buf, sizeof(buf)));
	return NBR_OK;
}

void mac::fin(yue::dbm &db)
{
	save(db);
	U32 flag = 0;
	db.driver().replace("_flag_", flag);
}

int
mac::save(yue::dbm &db)
{
	return db.driver().replace(UUID_SEED, "_uuid_") < 0 ?
		NBR_ESYSCALL : NBR_OK;
}

int
mac::load(yue::dbm::dbm &db)
{
	int seedl, flagl;
	mac *seed = db.driver().fetch("_uuid_").as<mac>();
	if (!seed) {
		return NBR_ENOTFOUND;	/* initial. */
	}
	if (seedl != sizeof(mac)) {
		util::mem::free(seed);
		return NBR_EINVAL;
	}
	U32 *f = db.driver().fetch("_flag_").as<U32>();
	if (flagl != sizeof(int)) {
		util::mem::free(seed);
		return NBR_EINVAL;
	}
	db.free(seed);
	if (*f) {
		/* disaster recovery : add 1M (worst 2M) to seed */
		if (((U64)UUID_SEED.id2 + disaster_addid) >
			0x00000000FFFFFFFF) {
			UUID_SEED.id1++;
		}
		UUID_SEED.id2 += disaster_addid;
		if (save(db) < 0) {
			util::mem::free(f);
			return NBR_ESYSCALL;
		}
	}
	db.free(f);
	U32 flag = 1;
	db.driver().replace(flag, "_flag_");
	return NBR_OK;
}

}
}
}

