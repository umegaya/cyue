/***************************************************************
 * error.h : rpc error object
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__ERROR_H__)
#define __ERROR_H__

namespace yue {
class fiber;
namespace rpc {
	struct error {
		static const int E_APP_RUNTIME = -10000;
		S32 m_errno;
		U32 m_msgid;
		fiber *m_fb;
		inline error(int err, MSGID msgid, fiber *eo) { set(err, msgid, eo); }
		inline void set_errno(int err, MSGID msgid) {
			m_errno = err; m_msgid = msgid;
		}
		inline const error &set(int err, MSGID msgid, fiber *fb) {
			set_errno(err, msgid);
			m_fb = fb;
			return *this;
		}
		inline int pack(serializer &sr) const {
			return sr.pack_error(m_msgid, *this);
		}
		inline int operator () (serializer &sr);
	};
}
}

#endif
