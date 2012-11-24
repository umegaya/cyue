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
		fiber *m_fb;
		inline error(fiber *fb) : m_fb(fb) {}
		inline int pack(serializer &sr) const;
		inline int operator () (serializer &sr);
	};
}
}

#endif
