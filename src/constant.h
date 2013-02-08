/***************************************************************
 * constant.h : constant definitions
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license details.
 ****************************************************************/
#if !defined(__CONSTANT_H__)
#define __CONSTANT_H__

namespace yue {
namespace constant {
class fiber {
public:
	enum {
		exec_invalid,
		exec_error,	/* unrecoverable error happen */
		exec_finish,/* procedure finish (it should reply to caller actor) */
		exec_yield, /* procedure yields. (will resume again) */
	};
};
class emittable {
public:
	enum {
		BASE,
		LISTENER,
		TIMER,
		SIGNAL,
		SOCKET,
		WPOLLER,
		FILESYSTEM,
		FSWATCHER,
		TASK,
		THREAD,
		SIGNALHANDLER,
	};
};
struct error {
	struct entry {
		int code;
		const char *name;
	};
#if defined(DEFINE_ERROR)
#undef DEFINE_ERROR
#endif
#define DEFINE_ERROR(error, code) static struct entry error
#include "rpcerrors.inc"
#undef DEFINE_ERROR
};
}
}

#endif
