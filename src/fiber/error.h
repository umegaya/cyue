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
		enum {
			DATATYPE_STRING,
			DATATYPE_LANG_ERROR,
		};
		S32 m_errno;
		U32 m_msgid;
		U8 m_type, padd[3];
		union {
			fiber *m_fb;
			char m_msg[256];
		};
		inline error(int err, MSGID msgid, fiber *eo) { set(err, msgid, eo); }
		inline error(int err, MSGID msgid, const char *fmt, ...) { 
			va_list v;
			va_start(v, fmt);
			set(err, msgid, fmt, v);
			va_end(v);
		}
		inline void set_errno(int err, MSGID msgid) {
			m_errno = err; m_msgid = msgid;
		}
		inline const error &set(int err, MSGID msgid, fiber *fb) {
			m_type = DATATYPE_LANG_ERROR;
			set_errno(err, msgid);
			m_fb = fb;
			return *this;
		}
		inline const error &set(int err, MSGID msgid, const char *fmt, va_list v) {
			m_type = DATATYPE_STRING;
			set_errno(err, msgid);
			vsnprintf(m_msg, sizeof(m_msg), fmt, v);
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
