/***************************************************************
 * loop.h : abstract main loop of worker thread
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__LOOP_H__)
#define __LOOP_H__

#include "selector.h"
#include "queue.h"
#include "syscall.h"
#include "handler.h"
#include "parking.h"
#include "thread.h"
#include "timerfd.h"
#include "signalfd.h"
#include "wpoller.h"
#include "fs.h"
#include "sync.h"
#include "task.h"

namespace yue {
namespace util {
class app;
}
class loop {
public:
	typedef handler::base basic_handler;
	typedef basic_handler::result result;
	typedef yue::handler::signalfd signalfd;
	typedef yue::handler::timerfd timerfd;
	typedef yue::handler::fs fs;
	typedef net::parking parking;
	typedef net::sync_poller sync_poller;
	typedef timerfd::task *timer_handle;
	typedef yue::handler::write_poller wpoller;
	typedef const util::app launch_args;
	static const U32 TASK_EXPAND_UNIT_SIZE = 4096;
	typedef util::queue<task::io, TASK_EXPAND_UNIT_SIZE> taskqueue;
	static void nop(loop &, poller::event &) {};
protected:
	taskqueue m_que;
	static int test;
	static util::app *m_a;
	static class loop *m_l;
	static poller m_p;
	static int ms_maxfd;
	static basic_handler **ms_h;
	static signalfd m_signal;
	static timerfd m_timer;
	static fs m_fs;
	static wpoller m_wp;
	static parking m_parking;
	static sync_poller m_sync;
	static poller::timeout ms_timeout;
public: /* public interface to io operation */
	static inline int maxfd() { return ms_maxfd; }
	static inline poller::timeout &timeout() { return ms_timeout; }
	static inline util::app &app() { return *m_a; }
	static inline poller &p() { return m_p; }
	static inline timerfd &timer() { return m_timer; }
	static inline fs &filesystem() { return m_fs; }
	static inline signalfd &sig() { return m_signal; }
	static inline wpoller &wp() { return m_wp; }
	static inline basic_handler **hl() { return ms_h; }
	static inline basic_handler &from(DSCRPTR fd) { return *(hl()[fd]); }
	static inline parking &pk() { return m_parking; }
	static inline sync_poller &sync() { return m_sync; }
	static inline loop *tls() { return m_l ? m_l : util::thread::current_tls<loop>(); }
	inline taskqueue &que() { return m_que; }
public:	/* public interface to app class */
	static int static_init(util::app &a);
	static void static_fin();
	static void fin_handlers();
	static void process_signal(int sig);
	int init(launch_args &a);
	void fin();
	void run(launch_args &a);
	inline void poll();
public: /* event emitters */
	static inline int open(basic_handler &h);
	static inline int close(basic_handler &h);
	inline void read(poller::event &ev);
	inline void read(basic_handler &h, poller::event &ev);
	inline void write(poller::event &ev);
	inline void write(basic_handler &h);
	inline int handshake(poller::event &ev, transport *t);
};
}
/* loop inline implement */
#include "loop.hpp"
/* task handler implement */
#include "task.hpp"
#endif
