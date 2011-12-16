/***************************************************************
 * msgid.h : auto increment ID generator (for any size of integer)
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
#if !defined(__MSGID_H__)
#define __MSGID_H__

namespace yue {
namespace util {

template <typename NUMERIC>
class msgid_generator {
protected:
	volatile NUMERIC m_msgid_seed;
public:
	static const NUMERIC MSGID_START = ((NUMERIC)1000);
	static const NUMERIC MSGID_LIMIT =
		(((NUMERIC)0x80) << ((sizeof(NUMERIC) - 1) * 8)) - 1;
	static const NUMERIC INVALID_MSGID = ((NUMERIC)0);
	typedef NUMERIC MSGID;
	msgid_generator() : m_msgid_seed(MSGID_START) {}
	MSGID new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_LIMIT, MSGID_START);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
	inline MSGID seed() const { return m_msgid_seed; }
	static inline bool valid(MSGID id) { return (id != INVALID_MSGID); }
	static inline int compare_msgid(MSGID msgid1, MSGID msgid2) {
		if (msgid1 < msgid2) {
			/* if diff of msgid2 and msgid1, then msgid must be turn around */
			return ((msgid2 - msgid1) < (MSGID_LIMIT / 2)) ? -1 : 1;
		}
		else if (msgid1 > msgid2){
			return ((msgid1 - msgid2) <= (MSGID_LIMIT / 2)) ? 1 : -1;
		}
		return 0;
	}
	/* for qsort */
	static int compare_func(MSGID id1, MSGID id2) {
		return msgid_generator::compare_msgid(id1, id2);
	}
	/* for STL like sort algorithm */
	struct compare_class {
		int operator () (MSGID id1, MSGID id2) {
			return msgid_generator::compare_msgid(id1, id2);
		}
	};
};
}
}

#endif
