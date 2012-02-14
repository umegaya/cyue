/***************************************************************
 * timerfd.cpp : event IO timer handling (can efficiently handle 10k timers)
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "timerfd.h"
#include "loop.h"
#include "server.h"

namespace yue {
namespace handler {
void timerfd::timer_callback(union sigval v) {
	loop::timer().process_one_shot(v.sival_int);
}
}
}
