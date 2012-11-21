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
DSCRPTR base::fd() {
	switch(type()) {
	DISPATCH(LISTENER, listener, fd)
	DISPATCH(TIMER, timerfd, fd)
	DISPATCH(SIGNAL, signalfd, fd)
	DISPATCH(SOCKET, socket, fd)
	DISPATCH(WPOLLER, write_poller, fd)
	DISPATCH(FILESYSTEM, fs, fd)
	DISPATCH(FSWATCHER, fs::watcher, fd)
	default: ASSERT(false); return INVALID_FD;
	}
}
transport *base::t() {
	switch(type()) {
	DISPATCH(LISTENER, listener, t)
	DISPATCH(TIMER, timerfd, t)
	DISPATCH(SIGNAL, signalfd, t)
	DISPATCH(SOCKET, socket, t)
	DISPATCH(WPOLLER, write_poller, t)
	DISPATCH(FILESYSTEM, fs, t)
	DISPATCH(FSWATCHER, fs::watcher, t)
	default: ASSERT(false); return NULL;
	}
}
void base::close() {
	switch(type()) {
	DISPATCH2(LISTENER, listener, close)
	DISPATCH2(TIMER, timerfd, close)
	DISPATCH2(SIGNAL, signalfd, close)
	DISPATCH2(SOCKET, socket, close)
	DISPATCH2(WPOLLER, write_poller, close)
	DISPATCH2(FILESYSTEM, fs, close)
	DISPATCH2(FSWATCHER, fs::watcher, close)
	default: ASSERT(false); break;
	}
}
DSCRPTR base::on_open(U32 &f) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_open, f)
	DISPATCH(TIMER, timerfd, on_open, f)
	DISPATCH(SIGNAL, signalfd, on_open, f)
	DISPATCH(SOCKET, socket, on_open, f)
	DISPATCH(WPOLLER, write_poller, on_open, f)
	DISPATCH(FILESYSTEM, fs, on_open, f)
	DISPATCH(FSWATCHER, fs::watcher, on_open, f)
	default: ASSERT(false); return INVALID_FD;
	}
}
void base::on_close() {
	switch(type()) {
	DISPATCH2(LISTENER, listener, on_close)
	DISPATCH2(TIMER, timerfd, on_close)
	DISPATCH2(SIGNAL, signalfd, on_close)
	DISPATCH2(SOCKET, socket, on_close)
	DISPATCH2(WPOLLER, write_poller, on_close)
	DISPATCH2(FILESYSTEM, fs, on_close)
	DISPATCH2(FSWATCHER, fs::watcher, on_close)
	default: ASSERT(false); break;
	}
}
base::result base::on_read(loop &l, poller::event &e) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_read, l, e)
	DISPATCH(TIMER, timerfd, on_read, l, e)
	DISPATCH(SIGNAL, signalfd, on_read, l, e)
	DISPATCH(SOCKET, socket, on_read, l, e)
	DISPATCH(WPOLLER, write_poller, on_read, l, e)
	DISPATCH(FILESYSTEM, fs, on_read, l, e)
	DISPATCH(FSWATCHER, fs::watcher, on_read, l, e)
	default: ASSERT(false); return destroy;
	}
}
base::result base::on_write(poller &p) {
	switch(type()) {
	DISPATCH(LISTENER, listener, on_write, p)
	DISPATCH(TIMER, timerfd, on_write, p)
	DISPATCH(SIGNAL, signalfd, on_write, p)
	DISPATCH(SOCKET, socket, on_write, p)
	DISPATCH(WPOLLER, write_poller, on_write, p)
	DISPATCH(FILESYSTEM, fs, on_write, p)
	DISPATCH(FSWATCHER, fs::watcher, on_write, p)
	default: ASSERT(false); return destroy;
	}
}
#else
base::~base() {}
DSCRPTR base::on_open(U32 &) {
	return fd();
}
void base::on_close() {
	return;
}
base::result base::on_read(loop &, poller::event &) {
	return destroy;
}
base::result base::on_write(poller &) {
	return destroy;
}
DSCRPTR base::fd() {
	return INVALID_FD;
}
transport *base::t() {
	return NULL;
}
void base::close() {
	sched_close();
}
#endif
void base::sched_close() {
	task::io t(this, task::io::CLOSE);
	server::tlsv()->que().mpush(t);
}
void base::sched_read(DSCRPTR fd) {
	poller::event ev;
	poller::init_event(ev, fd);
	task::io t(this, ev, task::io::READ_AGAIN);
	server::tlsv()->que().mpush(t);
}
void base::sched_unref() {
	fabric::task t(this, true);
	server::tlsv()->fque().mpush(t);
}
}
}


#endif


