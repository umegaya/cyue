/***************************************************************
 * http.h : http receiver fsm and sender implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__RUNNER_H__)
#define __RUNNER_H__
namespace yue {
class fabric;
namespace module {
namespace ll {
struct session_delegator {
	typedef handler::session session;
	struct args {
		session *s;
		int st;
	};
	template <class T>
	struct impl {
		void delegate(class fabric *fbr, args &a);
		int operator () (class fabric &, void *);
	};
};
}
}
}

#endif

