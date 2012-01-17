/***************************************************************
 * handler.hpp : abstruct interface of socket handler
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__HANDLER_HPP__)
#define __HANDLER_HPP__

#include "handler.h"


namespace yue {
namespace handler {
#if defined(NON_VIRTUAL)
#define DISPATCH(type, klass, funcname, ...) case type: {	\
	return reinterpret_cast<klass *>(this)->funcname(__VA_ARGS__);	\
} break;
#define DISPATCH2(type, klass, funcname, ...) case type: {	\
	reinterpret_cast<klass *>(this)->funcname(__VA_ARGS__);	\
} break;
#define DISPATCH3(type, klass) case type: {	\
	reinterpret_cast<klass *>(this)->~klass();	\
} break;
DSCRPTR base::on_open(U32 &f, transport **t) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_open, f, t)
	DISPATCH(TIMER, timerfd, on_open, f, t)
	DISPATCH(SIGNAL, signalfd, on_open, f, t)
	DISPATCH(SESSION, session, on_open, f, t)
	DISPATCH(WPOLLER, write_poller, on_open, f, t)
	default: ASSERT(false); return INVALID_FD;
	}
}
void base::on_close() {
	switch(type()) {
	DISPATCH2(LISTENER, listener, on_close)
	DISPATCH2(TIMER, timerfd, on_close)
	DISPATCH2(SIGNAL, signalfd, on_close)
	DISPATCH2(SESSION, session, on_close)
	DISPATCH2(WPOLLER, write_poller, on_close)
	default: ASSERT(false); break;
	}
}
base::result base::on_read(loop &l, poller::event &e) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_read, l, e)
	DISPATCH(TIMER, timerfd, on_read, l, e)
	DISPATCH(SIGNAL, signalfd, on_read, l, e)
	DISPATCH(SESSION, session, on_read, l, e)
	DISPATCH(WPOLLER, write_poller, on_read, l, e)
	default: ASSERT(false); return destroy;
	}
}
base::result base::on_write(poller &p, DSCRPTR fd) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_write, p, fd)
	DISPATCH(TIMER, timerfd, on_write, p, fd)
	DISPATCH(SIGNAL, signalfd, on_write, p, fd)
	DISPATCH(SESSION, session, on_write, p, fd)
	DISPATCH(WPOLLER, write_poller, on_write, p, fd)
	default: ASSERT(false); return destroy;
	}
}
#else
base::~base() {}
DSCRPTR base::on_open(U32 &, transport **) {
	return INVALID_FD;
}
void base::on_close() {
	return;
}
base::result base::on_read(loop &, poller::event &) {
	return destroy;
}
base::result base::on_write(poller &, DSCRPTR) {
	return destroy;
}
#endif
}
}


#endif


