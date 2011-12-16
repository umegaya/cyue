/***************************************************************
 * tc.h : tokyocabinet wrapper
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
#if !defined(__TC_H__)
#define __TC_H__

#include <tchdb.h>
#include "common.h"
#include <tcutil.h>

namespace yue {
namespace module {
namespace dbm {
/* tokyocabinet */
class tc {
public:
protected:
	TCHDB *m_db;
public:
	~tc() { close(); }
	int open(const char *opt) {
		if (!(m_db = tchdbnew())) {
			return NBR_EMALLOC;
		}
		if (!(tchdbopen(m_db, opt, 
			HDBOCREAT | HDBOREADER | HDBOWRITER))) {
			close();
			return NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	void close() {
		if (m_db) {
			tchdbclose(m_db);
			tchdbdel(m_db);
			m_db = NULL;
		}
	}
	int rnum() const {
		ASSERT(m_db);
		return tchdbrnum(m_db);
	}
	void clear() {
		if (m_db) {
			tchdbvanish(m_db);
			tchdbsync(m_db);
		}
	}
	void free(void *p) { return tcfree(p); }
	/* iterator all record in database. not thread safe */
	template <class FUNC>
	bool iterate(FUNC fn) {
		if (!tchdbiterinit(m_db)) { return false; }
		char *k; int ksz;
		while ((k = (char *)tchdbiternext(m_db, &ksz))) {
			if (fn(this, k, ksz) < 0) {
				return false;
			}
			tcfree(k);
		}
		return true;
	}
public:
	inline void *fetch(const void *k, int kl, int &vl) {
		ASSERT(m_db);
		return tchdbget(m_db, k, kl, &vl);
	}
	inline int fetch(const void *k, int kl, void *v, int vl) {
		ASSERT(m_db);
		return tchdbget3(m_db, k, kl, v, vl);
	}
	inline bool replace(const void *v, int vl, const void *k, int kl) {
		ASSERT(m_db);
		return tchdbput(m_db, v, vl, k, kl);
	}
	inline bool put(const void *v, int vl, const void *k, int kl, bool &exists) {
		ASSERT(m_db);
		bool b = tchdbputkeep(m_db, v, vl, k, kl);
		if (!b) { exists = (tchdbecode(m_db) == TCEKEEP); }
		else { exists = false; }
		return b;
	}
	inline bool remove(const void *k, int kl) {
		ASSERT(m_db);
		return tchdbout(m_db, k, kl);
	}
};
}
}
}

#endif

