/***************************************************************
 * lua.cpp : lua VM wrapper
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
#include "lua.h"
#include "server.h"
#include "serializer.h"
#include <luajit-2.0/luajit.h>

#define lua_error_check(vm, cond, ...)	if (!(cond)) {				\
	char __b[256]; snprintf(__b, sizeof(__b), __VA_ARGS__);			\
	lua_pushfstring(vm, "error %s(%d):%s", __FILE__, __LINE__, __b);\
	lua_error(vm);													\
}


namespace yue {
namespace module {
namespace ll {
/* lua::static variables */
server *lua::module::m_server = NULL;
U32 lua::ms_mode = lua::RPC_MODE_NORMAL;

const char lua::kernel_table[] 				= "__kernel";
const char lua::index_method[] 				= "__index";
const char lua::newindex_method[] 			= "__newindex";
const char lua::call_method[] 				= "__call";
const char lua::gc_method[] 				= "__gc";
const char lua::pack_method[] 				= "__pack";
const char lua::unpack_method[] 			= "__unpack";
const char lua::rmnode_metatable[] 			= "__rmnode_mt";
const char lua::rmnode_sync_metatable[] 	= "__rmnode_s_mt";
const char lua::thread_metatable[] 			= "__thread_mt";
const char lua::future_metatable[] 			= "__future_mt";
const char lua::error_metatable[] 			= "__error_mt";
const char lua::module_name[] 				= "yue";



/* lua::method */
char lua::method::prefix_NOTIFICATION[] 	= "notify_";
char lua::method::prefix_CLIENT_CALL[] 		= "client_";
char lua::method::prefix_TRANSACTIONAL[] 	= "tr_";
char lua::method::prefix_QUORUM[] 			= "quorum_";
char lua::method::prefix_TIMED[] 			= "timed_";


/******************************************************************************************/
/* future */
struct future {
	lua::coroutine *m_co;
	yue::yielded_context m_y;
	U8 m_state, padd[3];
	enum {
		INIT,
		TIMED_INIT,
		SET_CALLBACK,
		SET_TIMED_CALLBACK,
		RECV_RESPONSE,
		RECV_TIMED_RESPONSE,
		CALLBACKED,
		FINISH,
	};
public:	/* lua_Function */
	static inline int callback(lua::VM vm) {
		/* first operator () called already? (that is, already remote peer respond?) */
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		int top = lua_gettop(ft->m_co->vm());
		switch(ft->m_state) {
		case INIT:
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_CALLBACK);
			lua_settop(ft->m_co->vm(), 0);	/* reset stack (because sent arg still on stack) */
			lua_xmove(vm, ft->m_co->vm(), 1);
			//lua::dump_stack(ft->m_co->vm());
			break;
		case TIMED_INIT:
			/* TODO: when rpc command not sent, should we put function value to upvalue or metatable? */
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_TIMED_CALLBACK);
			lua_settop(ft->m_co->vm(), 0);	/* reset stack (because sent arg still on stack) */
			lua_xmove(vm, ft->m_co->vm(), 1);
			break;
		case RECV_RESPONSE:
			lua_xmove(vm, ft->m_co->vm(), 1);
			/* if top > 0, it means RPC call (and thus already sent args are rewinded)
			 * already_respond, current stack order is,
			 * [respond arg1],[respond arg2],...,[respond argN],[callback func]
			 * so, we need to put [callback func] on the top of stack
			 * [callback func],[respond arg1],...,[respond argN].
			 * fortunately, we have lua_insert API which exactly do as above. */
			lua_insert(ft->m_co->vm(), 1);
			/* NOTE: in that case, callback called before invoking future.will finished. */
			int r;
			if ((r = lua_resume(ft->m_co->vm(), top)) == LUA_YIELD) {
				ft->set_status(CALLBACKED);
			}
			else if (r != 0) {	/* finished but error */
				lua_pushstring(vm, lua_tostring(ft->m_co->vm(), -1));
				ft->fin(); lua_error(vm);
			}
			else {	/* successfully finished */
				ft->fin();
			}
			break;
		case RECV_TIMED_RESPONSE:
			for (; top > 1; top--) {
				if (fiber::exec_error == ft->pop_obj_and_run_fiber(vm, -1)) {
					lua_error(vm);
				}
				ASSERT(lua_isfunction(vm, 1));
			}
			lua_settop(ft->m_co->vm(), 0);
			lua_xmove(vm, ft->m_co->vm(), 1);
			break;
		default:
			lua_pushfstring(vm, "invalid state: %u", ft->m_state);
			ft->fin(); lua_error(vm);
			break;
		}
		return 0;
	}
	static future *init(lua::VM vm, lua &ll, bool timed) {
		int top = lua_gettop(vm);
		future *ft = reinterpret_cast<future *>(
			lua_newuserdata(vm, sizeof(future))
		);
		lua_getglobal(vm, lua::future_metatable);
		lua_setmetatable(vm, -2);

		/* create coroutine to execute callback */
		fiber::handler h(*ft);
		ft->m_y.set(h);
		if (timed) {
			ft->m_state = TIMED_INIT;
			/* block removed from yield list */
			ft->m_y.set_removable(false);
		}
		else {
			ft->m_state = INIT;
		}
		lua_error_check(vm, (ft->m_co = ll.create(&(ft->m_y))), "create coroutine");

		/* prevent top object (future *ft) from GC-ing */
		ASSERT(lua_isuserdata(vm, -1) && ft == lua_touserdata(vm, -1));
		lua::refer(vm, ft);
		//lua::refer(ft->m_co->vm(), ft) is not work because m_co->vm() has not resumed yet.
		/* future shift to bottom of stack
			(others copy into coroutine which created by future as above) */
		lua_insert(vm, 1);

		/* copy rpc args to ft->m_co (and it removes after
		 * 1. future.callback is called
		 * 2. rpc response received) */
		lua_xmove(vm, ft->m_co->vm(), top);

		//TRACE("=========== future ptr = %p\n", ft);
		return ft;
	}
	static int gc(lua::VM vm) {
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		TRACE("lua future::gc:%p\n", ft);
		return 0;
	}
public:	/* internal methods */
	inline void set_status(int newstate) {
		if (m_state != FINISH) {
			m_state = newstate;
		}
	}
	inline int run_fiber(lua::VM vm, object &o) {
		PROCEDURE(callproc) *p = new (o) PROCEDURE(callproc)(c_nil(), o);
		if (!p) { ASSERT(false); return fiber::exec_error; }
		int r = p->rval().init(*(m_co->ll().attached()), p);
		if (r < 0) { ASSERT(false); return fiber::exec_error; }
		switch((r = p->rval().co()->resume(vm, o))) {
		case fiber::exec_error:	/* unrecoverable error happen */
			lua_xmove(vm, p->rval().co()->vm(), 1);	/* copy error into caller VM */
			p->fin(true); break;
		case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
			p->fin(false); break;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			break;
		default:
			ASSERT(false);
			return fiber::exec_error;
		}
		return r;
	}
	inline int pop_obj_and_run_fiber(lua::VM cb_vm, int cb_index) {
		lua::VM vm = m_co->vm();
		object *pargs = reinterpret_cast<object *>(
			lua_touserdata(vm, -1)
		);
		if (!pargs) { ASSERT(false); return fiber::exec_error; }
		/* after below copy, pargs is ok to free its memory
		 * because sbuf which belongs pargs move into args. */
		object args = *pargs;
		/* so it can be popped from stack. */
		lua_pop(vm, 1);
		lua_pushvalue(cb_vm, cb_index);	/* copy callback function to top */
		int r = run_fiber(cb_vm, args);
		/* then args is no more necessary, so free sbuf inside of it */
		args.fin();
		return r;
	}
	inline int store_response_on_stack(object &response) {
		lua::VM vm = m_co->vm();
		int r, top = lua_gettop(vm);
		object *packed = reinterpret_cast<object *>(
			lua_newuserdata(vm, sizeof(object))
		);
		lua_pushboolean(vm, !response.is_error());
		if ((r = m_co->unpack_stack(response)) < 0) {
			/* callback with error */
			lua_pop(vm, 1);
			lua_pushboolean(vm, false);
			lua_pushstring(vm, "response unpack error");
		}
		/* pack stack except object * on stack bottom */
		if ((r = lua::coroutine::get_object_from_stack(vm, top + 2, *packed)) < 0) {
			ASSERT(false);
			return r;
		}
		/* shrink object */
		lua_settop(vm, top + 1);
		return NBR_OK;
	}
	inline int resume(fabric &f, object &o) {
		int r;
		ASSERT(o.is_response());
		if (m_co->ll().attached() == &f) {
			/* 1) future::will() not called yet. 
				if so, after will called, immediately start coroutine.
			 * 			(see implementation of future::will())
			 * 2) already future::will() called 
				and lua function (or cfunction) put on top of stack.*/
			switch(m_state) {
			case INIT:
				TRACE("resume: reset stack\n");
				lua_settop(m_co->vm(), 0);
				lua_pushboolean(m_co->vm(), !o.is_error());
				if (m_co->unpack_stack(o) < 0) { /* case 1) */
					/* callback with error */
					lua_pop(m_co->vm(), 1);
					lua_pushboolean(m_co->vm(), false);
					lua_pushstring(m_co->vm(), "response unpack error");
				}
				set_status(RECV_RESPONSE);
				return fiber::exec_yield;
			case TIMED_INIT: {
				TRACE("resume: reset stack\n");
				lua_settop(m_co->vm(), 0);
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				set_status(RECV_TIMED_RESPONSE);
				return fiber::exec_yield;
			}
			case SET_CALLBACK: {
				//lua::dump_stack(m_co->vm());
				ASSERT(lua_isfunction(m_co->vm(), 1));
				lua_pushboolean(m_co->vm(), !o.is_error());
				if ((r = m_co->unpack_stack(o)) < 0) {
					/* callback with error */
					lua_pop(m_co->vm(), 1);
					lua_pushboolean(m_co->vm(), false);
					lua_pushstring(m_co->vm(), "response unpack error");
				}
				set_status(CALLBACKED);
				return m_co->resume(r + 1);/* case 2) */
			}
			case SET_TIMED_CALLBACK: {
				ASSERT(lua_gettop(m_co->vm()) == 1);
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				/* create new fiber and run */
				r = pop_obj_and_run_fiber(m_co->vm(), 1);
				ASSERT( (
							r == fiber::exec_finish &&
							lua_gettop(m_co->vm()) == 1 &&
							lua_isfunction(m_co->vm(), 1)
						)
						|| (r != fiber::exec_finish));
				return r;
			}
			case RECV_TIMED_RESPONSE: {
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				break;
			}
			case CALLBACKED: {
				return m_co->resume(o);
			}
			default:
				ASSERT(false);
				break;
			}
		}
		else {
			fiber_handler fh(*this);
			return m_co->ll().attached()->delegate(fh, o);
		}
		return fiber::exec_error;
	}
	int operator () (fabric &f, object &o) {
		switch(resume(f, o)) {
		case fiber::exec_error:	/* unrecoverable error happen */
		case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
			fin();
			return NBR_OK;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			return NBR_OK;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	}
	void fin() {
		//TRACE("===========================future %p called fin\n", this);
		if (!m_y.removable()) {
			fabric::yielded_fibers().erase(m_y.msgid());
		}
		m_state = FINISH;
		lua::unref(m_co->vm(), this);
		if (m_co) {
			m_co->ll().destroy(m_co);
		}
	}
};



/******************************************************************************************/
/* timer */
struct timer {
	fabric *m_fbr;
	yue::timer m_t;
	static inline int init(lua::VM vm) {
		ASSERT(lua_isfunction(vm, -1) && lua_isnumber(vm, -2) && lua_isnumber(vm, -3));
		timer *t = reinterpret_cast<timer *>(lua_newuserdata(vm, sizeof(timer)));/*4*/
		lua_pushlightuserdata(vm, t);	/* t will be key also (5) */
		/* ref timer callback function */
		lua_insert(vm, -3);		/* insert key to the position of callback function */
		lua_insert(vm, -3);		/* insert key to the position of callback function */
		lua::dump_stack(vm);	/* now stack layout should be num,num,userdata,userdata,function */
		lua_settable(vm, LUA_REGISTRYINDEX);/* reg[userdata] = function. then will be num,num,userdata */

		yue::util::functional<int (yue::timer)> h(*t);
		TRACE("setting: %lf, %lf\n", lua_tonumber(vm, -3), lua_tonumber(vm, -2));
		if (!(t->m_t = lua::module::served()->set_timer(
			lua_tonumber(vm, -3), lua_tonumber(vm, -2), h))) {
			lua_pushfstring(vm, "create timer");
			lua_error(vm);
		}
		t->m_fbr = &(fabric::tlf());
		/* return t as timer object */
		return 1;
	}
	static inline int stop(lua::VM vm, timer *t) {
		/* unref timer callback function */
		lua_pushlightuserdata(vm, t);
		lua_pushnil(vm);
		lua_settable(vm, LUA_REGISTRYINDEX);
		/* stop timer (t is freed) */
		lua::module::served()->stop_timer(t->m_t);
		return 0;
	}
	int operator () (fabric &f, object &o) {
		/* o is not valid here */
		if (operator () (m_t) < 0) {
			lua *l = reinterpret_cast<lua *>(&(f.lang()));
			timer::stop(l->vm(), this);
			return fiber::exec_error;
		}
		return fiber::exec_finish;
	}
	int operator () (yue::timer t) {
		fabric *fbr = &(fabric::tlf());
		if (m_fbr == fbr) {
			fiber::rpcdata d;
			PROCEDURE(callproc) *p = new (c_nil()) PROCEDURE(callproc)(c_nil(), d);
			if (!p) {
				ASSERT(false);
				return NBR_EMALLOC;
			}
			int r = p->rval().init(*m_fbr, p);
			if (r < 0) {
				ASSERT(false);
				return NBR_EINVAL;
			}
			lua::VM vm = p->rval().co()->vm();
			lua_pushlightuserdata(vm, this);
			lua_gettable(vm, LUA_REGISTRYINDEX);
			ASSERT(lua_isfunction(vm, -1));
			lua_pushlightuserdata(vm, this);
			switch(p->rval().co()->resume(1)) {
			case fiber::exec_error:	/* unrecoverable error happen */
				p->fin(true); break;
			case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
				p->fin(false); break;
			case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
			case fiber::exec_delegate:	/* fiber send to another native thread. */
				break;
			default:
				ASSERT(false);
				return NBR_EINVAL;
			}
			return NBR_OK;
		}
		else {
			fiber_handler h(*this);
			m_fbr->delegate(h);
			return NBR_OK;
		}
	}
};


/******************************************************************************************/
/* class lua */
int lua::method::init(VM vm, actor *a, const char *name) {
	method *m = reinterpret_cast<method *>(
		lua_newuserdata(vm, sizeof(method))
	);
	m->m_name = parse(name, m->m_attr); /* name is referred by VM so never freed */
	m->m_a = a;
	/* meta table */
	switch(a->kind()) {
	case actor::THREAD:
		switch (lua::ms_mode) {
		case lua::RPC_MODE_NORMAL:
		case lua::RPC_MODE_SYNC:
		case lua::RPC_MODE_PROTECTED:
			lua_getglobal(vm, thread_metatable); break;
		} break;
	case actor::RMNODE:
		switch (lua::ms_mode) {
		case lua::RPC_MODE_NORMAL:
		case lua::RPC_MODE_PROTECTED:
			lua_getglobal(vm, rmnode_metatable); break;
		case lua::RPC_MODE_SYNC:
			lua_getglobal(vm, rmnode_sync_metatable); break;
		} break;
	}
	lua_setmetatable(vm, -2);
	return 1;
}
const char *lua::method::parse(const char *name, U32 &attr) {
#define PARSE(__r, __pfx, __attr, __next) { 									\
	if ((!(__attr & __pfx)) && 													\
		util::mem::cmp(__r, prefix_##__pfx, sizeof(prefix_##__pfx) - 1) == 0) { \
		__attr |= __pfx; 														\
		__r += (sizeof(prefix_##__pfx) - 1); 									\
		__next;																	\
	} 																			\
}
	attr = 0;
	const char *r = name;
	while(*r) {
		PARSE(r, NOTIFICATION, attr, continue);
		PARSE(r, CLIENT_CALL, attr, break);
		if ((!(attr & (CLIENT_CALL | NOTIFICATION)))) {
			PARSE(r, TRANSACTIONAL, attr, break);
			PARSE(r, QUORUM, attr, break);
			PARSE(r, TIMED, attr, break);
		}
		break;
	}
	TRACE("parse:%s>%s:%08x\n", name, r, attr);
	return r;
}
template <>
int lua::method::call<session>(VM vm) {
	/* stack = method, a1, a2, ..., aN */
	coroutine *co = coroutine::to_co(vm);
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	session *s = m->m_a->m_s; U32 timeout = 0/* means using default timeout */;
	if (m->has_future()) {
		/* TODO: remove this code */
		if (!s->valid()) {
			lua_error_check(vm,
				(s->sync_connect(fabric::tlf().tla()) >= 0), "session::connect");
		}
		if (m->timed()) {
			/* when timed, stack = method,timeout,a1,a2,...,aN */
			timeout = ((U32)lua_tonumber(vm, 2) * 1000 * 1000);
			lua_remove(vm, 2);
		}
		/* return future instead of yield & wait for reply */
		future *ft = future::init(vm, co->ll(), m->timed());
		co = ft->m_co;
	}
	if (s->valid()) {
		PROCEDURE(callproc)::args_and_cb<yielded_context> a(*(co->yldc()));
		a.m_co = co;
		a.m_timeout = timeout;
		lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*s, a), "callproc");
	}
	else {
		session::watcher sw(*co);
		lua_error_check(vm, (s->reconnect(sw) >= 0), "reconnect");
	}
	lua::dump_stack(vm);
	return m->has_future() ? 1 : co->yield();
}

int lua::method::sync_call(VM vm) {
	/* a1, a2, ..., aN */
//	dump_stack(vm);
	coroutine co(vm);
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	PROCEDURE(callproc)::args a;
	a.m_co = &co;
	ASSERT(m->m_a && m->m_a->m_s);
	session *s = m->m_a->m_s;
	local_actor &la = fabric::tlf().tla();
	if (!s->valid()) {
		lua_error_check(vm, (s->sync_connect(la) >= 0), "session::connect");
	}
	object o; int r;
	lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*s, a), "callproc");
	lua_error_check(vm, s->sync_write(la) >= 0, "session::send");
	lua_error_check(vm, s->sync_read(la, o) >= 0, "session::recv");
	lua_error_check(vm, ((r = co.unpack_stack(o)) >= 0), "invalid response");
	if (o.is_error()) { lua_error(vm); }
	return r;
}
template <>
int lua::method::call<local_actor>(VM vm) {
	coroutine *co = coroutine::to_co(vm); U32 timeout = 0;
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	if (m->has_future()) {
		if (m->timed()) {
			/* when timed, stack = method,timeout,a1,a2,...,aN */
			timeout = ((U32)lua_tonumber(vm, 2) * 1000 * 1000);
			lua_remove(vm, 2);
		}
		future *ft = future::init(vm, co->ll(), m->timed());
		co = ft->m_co;
	}
	PROCEDURE(callproc)::args_and_cb<yielded_context> a(*(co->yldc()));
	a.m_co = co;
	a.m_timeout = timeout;
	lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*(m->m_a->m_la), a), "callproc");
	return m->has_future() ? 1 : co->yield();
}

/**/



/* lua::module */
void lua::module::init(VM vm, server *srv) {
	/* yue module. */
//	module *m = reinterpret_cast<module *>(lua_newuserdata(vm, sizeof(module)));
//	m->m_server = srv;
	m_server = srv;
	lua_newtable(vm);
	/* meta table */
	lua_newtable(vm);
	lua_pushcfunction(vm, index);
	lua_setfield(vm, -2, index_method);
//	lua_pushvalue(vm, -1);	/* use metatable itself as newindex */
//	lua_setfield(vm, -2, newindex_method);
	lua_setmetatable(vm, -2);
	/* API 'connect' */
	lua_pushcfunction(vm, connect);
	lua_setfield(vm, -2, "connect");
	/* API 'poll' */
	lua_pushcfunction(vm, poll);
	lua_setfield(vm, -2, "poll");
	/* API 'stop' */
	lua_pushcfunction(vm, stop);
	lua_setfield(vm, -2, "stop");
	/* API 'timer' */
	lua_pushcfunction(vm, timer);
	lua_setfield(vm, -2, "timer");
	/* API 'stop_timer' */
	lua_pushcfunction(vm, stop_timer);
	lua_setfield(vm, -2, "stop_timer");
	/* API 'sync_mode' */
	lua_pushcfunction(vm, mode);
	lua_setfield(vm, -2, "mode");
	/* API 'newthread' */
	lua_pushcfunction(vm, newthread);
	lua_setfield(vm, -2, "newthread");
	/* API 'resume' */
	lua_pushcfunction(vm, resume);
	lua_setfield(vm, -2, "resume");
	/* API 'configure' */
	lua_pushcfunction(vm, configure);
	lua_setfield(vm, -2, "configure");
	/* API 'yield' */
	lua_pushcfunction(vm, yield);
	lua_setfield(vm, -2, "yield");
	/* API 'listen' */
	lua_pushcfunction(vm, listen);
	lua_setfield(vm, -2, "listen");
	/* API 'error' */
	lua_pushcfunction(vm, error);
	lua_setfield(vm, -2, "error");
	/* API 'read' */
	lua_pushcfunction(vm, read);
	lua_setfield(vm, -2, "read");
	/* API 'write' */
	lua_pushcfunction(vm, write);
	lua_setfield(vm, -2, "write");
	/* API 'protect' */
	const char src[] = "return function(p)"
		"local c = { conn = p }"
		"setmetatable(c, {"
			"__index = function(t, k)"
				"return function(...)"
					"local r = {t.conn[k](...)}"
					"if not yue.core.error(r[1]) then return unpack(r)"
					"else error(r[1]) end"
				"end"
			"end"
		"})"
		"return c"
	"end";
	luaL_loadbuffer(vm, src, sizeof(src) - 1, NULL);
	lua_pcall(vm, 0, 1, 0);
	lua_setfield(vm, -2, "protect");

	/* add submodule util */
	utility::init(vm);
	lua_setfield(vm, -2, "util");

	/* give global name 'yue' */
	lua_setfield(vm, LUA_GLOBALSINDEX, module_name);

	/* 2 common metatable for method object */
	lua_newtable(vm);
	lua_pushcfunction(vm, method::call<session>);
	lua_setfield(vm, -2, lua::call_method);
	lua_setfield(vm, LUA_GLOBALSINDEX, rmnode_metatable);

	lua_newtable(vm);
	lua_pushcfunction(vm, method::sync_call);
	lua_setfield(vm, -2, lua::call_method);
	lua_setfield(vm, LUA_GLOBALSINDEX, rmnode_sync_metatable);

	lua_newtable(vm);
	lua_pushcfunction(vm, method::call<local_actor>);
	lua_setfield(vm, -2, lua::call_method);
	lua_setfield(vm, LUA_GLOBALSINDEX, thread_metatable);

	lua_newtable(vm);
	lua_setfield(vm, LUA_GLOBALSINDEX, error_metatable);

	lua_newtable(vm);
	lua_pushcfunction(vm, future::callback);
	lua_setfield(vm, -2, "callback");
	lua_pushvalue(vm, -1);	/* metatabl it self as index */
	lua_setfield(vm, -2, lua::index_method);
	lua_setfield(vm, LUA_GLOBALSINDEX, future_metatable);

}
int lua::module::index(VM vm) {
	/* access like obj[key]. -1 : key, -2 : obj */
	module *m = reinterpret_cast<module *>(lua_touserdata(vm, -2));
	lua_getmetatable(vm, -2);
	lua_pushvalue(vm, -2);	/* dup key */
	switch(lua_type(vm, -1)) {
	case LUA_TSTRING: {
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			session *s; const char *k = lua_tostring(vm, -2);
			lua_error_check(vm, (k && (s = m->served()->spool().add_to_mesh(k, NULL))),
				"add_to_mesh");
			actor::init(vm, s);
			lua_pushvalue(vm, -1);	/* dup actor */
			lua_settable(vm, -2);	/* set to module metatable. */
			/* now stack layout is actor, module (from top) */
			return 1;
		}
	} break;
	case LUA_TTABLE: {
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);	/* 3 */
			lua_pushinteger(vm, 1); /* 4  */
			lua_gettable(vm, -3); /* 4 = key[1] */
			session *s; const char *k = lua_tostring(vm, -1);
			lua_pushinteger(vm, 2);	/* 5 */
			lua_gettable(vm, -4);	/* 5 = key[2] = option */
			object obj;
			lua_error_check(vm,
				(lua::coroutine::get_object_from_table(vm, lua_gettop(vm), obj) >= 0),
				"obj from table");
			lua_pop(vm, 2);	/* 3, pop key[1] and key[2] */
			lua_error_check(vm, (k && (s = m->served()->spool().add_to_mesh(k, &obj))),
				"add_to_mesh");
			actor::init(vm, s);
			lua_pushvalue(vm, -1);	/* 6 dup actor */
			lua_settable(vm, -2);	/* set to module metatable. */
			/* now stack layout is actor, module (from top) */
			return 1;
		}
	} break;
	case LUA_TNUMBER: {
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			local_actor *la; int k = (int)lua_tonumber(vm, -2);
			lua_error_check(vm, (la = m->served()->get_thread(k)), "get_thread");
			actor::init(vm, la);
			lua_pushvalue(vm, -1);	/* dup actor */
			lua_settable(vm, -2);	/* set module table. */
			/* now stack layout is actor, module (from top) */
			return 1;
		}
	} break;
	default:
		lua_rawget(vm, -2);
	}
	return 1;
}
int lua::module::connect(VM vm) {
	int top; session *s;
	switch((top = lua_gettop(vm))) {
	case 1: {
		lua_error_check(vm, (lua_isstring(vm, -1)), "type error");
		s = served()->spool().open(lua_tostring(vm, -1), NULL);
	} break;
	case 2: {
		const char *addr;
		lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
		if (lua_istable(vm, top)) {
			object obj;
			lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
					"obj from table");
			s = served()->spool().open(addr, &obj);
		}
		else {
			s = served()->spool().open(addr, NULL);
		}
	} break;
	}
	lua_error_check(vm, s, "create session");
	return actor::init(vm, s);
}
int lua::module::stop(VM vm) {
	served()->die();
	return 0;
}
int lua::module::poll(VM vm) {
	yue_poll();
	return 0;
}
int lua::module::newthread(VM vm) {
	fiber::rpcdata d;
	PROCEDURE(callproc) *p = new(c_nil()) PROCEDURE(callproc)(c_nil(), d);
	VM co;
	if (p) {
		if (p->rval().init(fabric::tlf(), p) < 0) { goto error; }
		if (!(co = p->rval().co()->vm())) { goto error; }
		lua_xmove(vm, co, 1);	/* copy called function */
		lua_pushlightuserdata(vm, p);
		return 1;
	}
error:
	ASSERT(false);
	if (p) { p->fin(true); }
	lua_pushnil(vm);
	return 1;
}
int lua::module::resume(VM vm) {
	TRACE("yue_resume: top = %u\n", lua_gettop(vm));
	lua::dump_stack(vm);
	PROCEDURE(callproc) *fb = reinterpret_cast<PROCEDURE(callproc)*>(
		lua_touserdata(vm, 1)
	);
	int r;
	lua::VM co = fb->rval().co()->vm();
	switch((r = fb->rval().co()->resume(vm))) {
	case fiber::exec_error:	/* unrecoverable error happen */
		TRACE("resume error");
		lua_xmove(co, vm, 1);	/* copy error into caller VM */
		fb->fin(true);
		lua_error(vm);
		ASSERT(false);/* never reach here */
	case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
		TRACE("resume finish:%d\n", lua_gettop(co));
		if (fb->rval().co()->has_flag(lua::coroutine::FLAG_EXIT)) {
			fb->fin(true);
			return 0;
		}
		lua::dump_stack(co);
		if ((r = (lua_gettop(co) - 1)) > 0) {	/* avoid first [called func] copied to caller vm */
			lua_xmove(co, vm, r);
		}
		fb->fin(false);
		return r;
	case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
	case fiber::exec_delegate:	/* fiber send to another native thread. */
		return 0;
	default:
		lua_error_check(vm, false, "resume result error:%d", r);
		ASSERT(false);/* never reach here */
	}
}
int lua::module::yield(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	co->set_flag(lua::coroutine::FLAG_EXIT, true);
	return co->yield();
}
int lua::module::timer(VM vm) {
	return yue::module::ll::timer::init(vm);
}
int lua::module::stop_timer(VM vm) {
	yue::module::ll::timer *t = 
		reinterpret_cast<yue::module::ll::timer *>(
			lua_touserdata(vm, 1)
		);
	return yue::module::ll::timer::stop(vm, t);
}
int lua::module::configure(VM vm) {
	const char *k, *v;
	lua_error_check(vm,
		((v = lua_tostring(vm, -1)) && (k = lua_tostring(vm, -2))),
		"invalid parameter %p %p", k, v);
	lua_error_check(vm, served()->cfg().configure(k, v) >= 0, "invalid config %s %s", k, v);
	return 0;
}
int lua::module::mode(VM vm) {
	const char *mode;
	lua_error_check(vm, (mode = lua_tostring(vm, -1)), "type error");
	if (util::str::cmp("sync", mode) == 0) {
		lua::ms_mode = lua::RPC_MODE_SYNC;
	}
	else if (util::str::cmp("protect", mode) == 0) {
		lua::ms_mode = lua::RPC_MODE_PROTECTED;
	}
	else {
		lua::ms_mode = lua::RPC_MODE_NORMAL;
	}
	return 0;
}
int lua::module::listen(VM vm) {
	int top;
	switch((top = lua_gettop(vm))) {
	case 1: {
		lua_error_check(vm, (lua_isstring(vm, -1)), "type error");
		lua_pushinteger(vm, served()->listen(lua_tostring(vm, -1)));
	} break;
	case 2: {
		const char *addr;
		lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
		if (lua_istable(vm, top)) {
			object obj;
			lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
					"obj from table");
			lua_pushinteger(vm, served()->listen(addr, &obj));
		}
		else {
			lua_pushinteger(vm, served()->listen(addr, NULL));
		}
	} break;
	}
	return 1;
}



/* lua::actor */
int lua::actor::index(VM vm) {
	/* access like obj[key]. -1 : key, -2 : obj */
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -2));
	lua_getmetatable(vm, -2);	/* 3 */
	lua_pushvalue(vm, -2);	/* dup key */ /*4*/
	lua_rawget(vm, -2);		/* get object from metatable */ /*4*/
	if (lua_isnil(vm, -1)) {
		const char *k = lua_tostring(vm, -3);
		method::init(vm, a, k); /*5*/
		lua_pushvalue(vm, -4);	/* dup key */ /*7*/
		lua_pushvalue(vm, -2);	/* dup method object */ /*6*/
		lua_settable(vm, 3);	/* set method object to metatable */
	}
	return 1;
}
int lua::actor::gc(VM vm) {
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -1));
	switch(a->m_kind) {
	case actor::RMNODE:
		TRACE("actor::gc session %p closed\n", a->m_s);
		a->m_s->close(); break;
	case actor::THREAD:
		break;
	}
	return 0;
}



/* lua::coroutine */
int lua::coroutine::init(yielded_context *y, lua *l) {
	bool first = !m_exec;
	if (first && !(m_exec = lua_newcthread(l->vm(), 0))) {
		return NBR_EEXPIRE;
	}
	lua_pushthread(m_exec);
	/* Isolation: currently off */
	//	lua_getfield(m_exec, LUA_REGISTRYINDEX, fb->wid());
//	if (!lua_istable(m_exec, -1)) { ASSERT(0); return NBR_ENOTFOUND; }
//	lua_setfenv(m_exec, -2);
	if (first) {
		lua_pop(l->vm(), 1);	/* remove thread on main VM's stack */
		/* add this pointer to registry so that can find this ptr(this)
		 * from m_exec faster */
		lua_pushvalue(m_exec, -1);
		lua_pushlightuserdata(m_exec, this);
		lua_settable(m_exec, LUA_REGISTRYINDEX);
		/* register thread object to registry so that never collected by GC. */
		refer();
		ASSERT(to_co(m_exec) == this);
	}
	/* reset stack */
	lua_settop(m_exec, 0);
	m_ll = l;
	m_y = y;
	ASSERT(m_y);
	ASSERT(lua_gettop(m_exec) == 0);
	return NBR_OK;
}

void lua::coroutine::free() {
	m_ll->destroy(this);
}

void lua::coroutine::fin() {
	if (m_exec) {
		/* remove key-value pair of this and m_exec from registry
		 * (now cannot found from this from m_exec) */
		lua_pushthread(m_exec);
		lua_pushnil(m_exec);
		lua_settable(m_exec, LUA_REGISTRYINDEX);
		ASSERT(!to_co(m_exec));
		/* remove m_exec from registry (so it will gced) */
		unref();
		/* then garbage collector collect this object and dispose,
		 * we just forget current m_exec pointer value. */
		m_exec = NULL;
	}
}

bool lua::coroutine::operator () (session *s, int state) {
	TRACE("coro:op() %p, %u, %p\n", s, state, m_exec);
	lua::dump_stack(m_exec);
	if (state == session::ESTABLISH) {
		PROCEDURE(callproc)::args_and_cb<yielded_context> a(*yldc());
		a.m_co = this;
		lua_error_check(m_exec, INVALID_MSGID != yue::rpc::call(*s, a), "callproc");
	}
	return false;
}

int lua::coroutine::resume(int r) {
	/* TODO: when exec_error we should reset coroutine state.
	 * without reset coroutine, it does not run correctly when reused. but how?
	 * (now we call coroutine::fin() and destroy coroutine object. but its not efficient way) */
	if (lua_gettop(m_exec) == 4 && lua_tointeger(m_exec, 2) == 100 && lua_tointeger(m_exec, 4) == 10) {
		ASSERT(false);
	}
	if ((r = lua_resume(m_exec, r)) == LUA_YIELD) {
		/* this coroutine uses yield as long-jump (global exit) */
		if (has_flag(lua::coroutine::FLAG_EXIT)) { return fiber::exec_finish; }
		return fiber::exec_yield;	/* successfully suspended */
	}
	else if (r != 0) {	/* error happen */
		fprintf(stderr,"fiber failure %d <%s>\n",r,lua_tostring(m_exec, -1));
		return fiber::exec_error;
	}
	else {	/* successfully finished */
		return fiber::exec_finish;
	}
}

int lua::coroutine::get_object_from_table(VM vm, int stkid, object &obj) {
	serializer sr; pbuf pbf;int r;
	if (pbf.reserve(256) < 0) { return NBR_EMALLOC; }
	ASSERT(lua_istable(vm, stkid));
	sr.start_pack(pbf);
	if ((r = coroutine::pack_stack(vm, sr, stkid)) < 0) { return r; }
	pbf.commit(r);
	if ((r = sr.unpack(pbf)) != serializer::UNPACK_SUCCESS) {
		return NBR_EFORMAT;
	}
	obj = sr.result();
	return NBR_OK;
}

int lua::coroutine::get_object_from_stack(VM vm, int start_id, object &obj) {
	serializer sr; pbuf pbf;int r;
	if (pbf.reserve(256) < 0) { return NBR_EMALLOC; }
	sr.start_pack(pbf);
	if ((r = coroutine::pack_stack(vm, start_id, sr)) < 0) { return r; }
	pbf.commit(r);
	if ((r = sr.unpack(pbf)) != serializer::UNPACK_SUCCESS) {
		return NBR_EFORMAT;
	}
	obj = sr.result();
	return NBR_OK;
}


/* pack */
int lua::coroutine::pack_stack(serializer &sr) {
	int top = lua_gettop(m_exec), r;
	if (top <= 0) {
		ASSERT(false); sr.pushnil(); return sr.len();
	}
	method *m = reinterpret_cast<method *>(lua_touserdata(m_exec, 1));
	if (!m) {
		ASSERT(false); sr.pushnil(); return sr.len();
	}
	sr.push_array_len(top);
	verify_success(sr << m->m_name);
	for (int i = 2; i <= top; i++) {
		if ((r = pack_stack(m_exec, sr, i)) < 0) { return r; }
	}
	return sr.len();
}

int lua::coroutine::pack_stack(VM vm, int start_id, serializer &sr) {
	int top = lua_gettop(vm), r;
	if (top == 0 || start_id > top) {
		ASSERT(false); sr.pushnil(); return sr.len();
	}
	sr.push_array_len(top + 1 - start_id);
	for (int i = start_id; i <= top; i++) {
		if ((r = pack_stack(vm, sr, i)) < 0) { return r; }
	}
	return sr.len();
}

#define catch_nop {}
#define retry(exp)   retry_catch(exp, catch_nop)
#define retry_catch(exp, __catch) { 				\
	size_t sz = 1024;					\
	while ((r = (exp)) < 0) {				\
		if (sr.expand_buffer(sz) < 0)	{ 		\
			__catch; ASSERT(false); return r; }	\
		sz <<= 1;					\
	}							\
}

int lua::coroutine::pack_stack(VM vm, serializer &sr, int stkid) {
	int r;
	ASSERT(stkid >= 0);
	//TRACE("pack_stack type = %u\n", lua_type(vm, stkid));
	switch(lua_type(vm, stkid)) {
	case LUA_TNIL: 		retry(sr.pushnil()); break;
	case LUA_TNUMBER:	retry(sr << lua_tonumber(vm, stkid)); break;
	case LUA_TBOOLEAN:	retry(sr << (lua_toboolean(vm, stkid) ? true : false));break;
	case LUA_TSTRING:	
		retry(sr.push_string(lua_tostring(vm, stkid), lua_objlen(vm, stkid)));
		break;
	case LUA_TTABLE: /* = map {...} */
		if ((r = call_custom_pack(vm, sr, stkid)) < 0) {
			if (r == NBR_ENOTFOUND) {
				/* pack metamethod not found. normal table pack */
				verify_success(pack_table(vm, sr, stkid));
				break;
			}
			return r;
		} break;
	case LUA_TFUNCTION:     /* = array ( LUA_TFUNCTION, binary_chunk ) */
		if (lua_iscfunction(vm, stkid)) {
			retry(sr.pushnil()); break;
		}
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		stkid is popped. so we can assure target function to dump
		is already on the top of stack here. */
		verify_success(pack_function(vm, sr, stkid));
		break;
	case LUA_TUSERDATA:
		if ((r = call_custom_pack(vm, sr, stkid)) < 0) {
			if (r == NBR_ENOTFOUND) {
				/* pack metamethod not found. no way to pack it. */
				ASSERT(false); retry(sr.pushnil());
			}
			return r;
		} break;
	case LUA_TLIGHTUSERDATA:
	case LUA_TTHREAD:
	default:
		//we never use it.
		ASSERT(false);
		return NBR_EINVAL;
	}
	return sr.len();
}

int lua::coroutine::pack_table(VM vm, serializer &sr, int stkid) {
	int tblsz = 0;
	TRACE("nowtop=%d\n", lua_gettop(vm));
	lua_pushnil(vm);        /* push first key */
	while(lua_next(vm, stkid)) {
		tblsz++;
		lua_pop(vm, 1);
	}
	TRACE("tblsz=%d\n", tblsz);
	sr.push_map_len(tblsz);
	lua_pushnil(vm);        /* push first key (idiom, i think) */
	while(lua_next(vm, stkid)) {    /* put next key/value on stack */
		int top = lua_gettop(vm);       /* use absolute stkid */
		pack_stack(vm, sr, top - 1);        /* pack table key */
		pack_stack(vm, sr, top);    /* pack table value */
		lua_pop(vm, 1); /* destroy value */
	}
	return NBR_OK;
}

int lua::coroutine::pack_function(VM vm, serializer &sr, int stkid) {
	ASSERT(stkid >= 0);
	TRACE("pack func: ofs=%d\n", sr.len());
	lua_pushvalue(vm, stkid);
	pbuf pbf;
	/* TODO: write sr.pbuf directly */
	int r = lua_dump(vm, lua::writer::callback, &pbf);
	lua_pop(vm, 1);
	if (r != 0) { return NBR_EINVAL; }
	if ((r = sr.expand_buffer(pbf.last())) < 0) { return r; }
	verify_success(sr.push_array_len(2));
	verify_success(sr << ((U8)LUA_TFUNCTION));
	verify_success(sr.push_raw(pbf.p(), pbf.last()));
	TRACE("pack func: after ofs=%d\n", sr.len());
	return NBR_OK;
}

int lua::coroutine::call_custom_pack(VM vm, serializer &sr, int stkid) {
	ASSERT(stkid >= 0);
	lua_getfield(vm, stkid, lua::pack_method);	//1
	if (lua_isfunction(vm, -1)) {
		lua_pushvalue(vm, stkid);				//2
		ASSERT(lua_istable(vm, -1) || lua_isuserdata(vm, -1));
		/* TODO: write sr.pbuf directly */
		int r; pbuf pbf;
		lua_pushlightuserdata(vm, &pbf);		//3
		if (lua_pcall(vm, 2, 1, 0) != 0) {		//1
			TRACE("pack userdata fails (%s)\n", lua_tostring(vm, -1));
			return NBR_ESYSCALL;
		}
		if ((r = sr.expand_buffer(pbf.last())) < 0) { return r; }
		verify_success(sr.push_array_len(3));
		verify_success(sr << ((U8)LUA_TUSERDATA));
		verify_success(sr << lua_tostring(vm, -1));
		verify_success(sr.push_raw(pbf.p(), pbf.last()));
		lua_pop(vm, 1);							//0
		return NBR_OK;
	}
	return NBR_ENOTFOUND;
}

/* unpack */
int lua::coroutine::unpack_stack(const object &o) {
	int r, al, top = lua_gettop(m_exec);
	if (o.is_error()) {
		if ((r = unpack_stack(m_exec, o.error())) < 0) { goto error; }
		/* lua_error(m_exec); *//* it causes crush on finderrfunc inside luajit.
		I think if we call lua_error from code where not called from lua VM,
		such an error happen. */
		/* so how can we propagate error to caller coroutine? */
		lua_getglobal(m_exec, lua::error_metatable);
		lua_setmetatable(m_exec, -2);
		return 1;
	}
	if (o.is_response()) {
		al = o.resp().size();
		for (int i = 0; i < al; i++) {
			if ((r = unpack_stack(m_exec, o.resp().elem(i))) < 0) { goto error; }
		}
		return al;
	}
	else {
		ASSERT(o.is_request());
		al = o.alen();
		ASSERT(al > 0);
		if ((r = unpack_stack(m_exec, o.arg(0))) < 0) { goto error; }
		lua_gettable(m_exec, LUA_GLOBALSINDEX);	/* TODO: should we use environment index? */
		for (int i = 1; i < al; i++) {
			if ((r = unpack_stack(m_exec, o.arg(i))) < 0) { goto error; }
		}
		return al - 1;
	}
error:
	lua_settop(m_exec, top);
	return r;
}

int lua::coroutine::unpack_stack_with_vm(VM main_co, const data &o) {
	int r, al, top = lua_gettop(m_exec);
	al = o.size();
	ASSERT(al > 0);
	lua_xmove(main_co, vm(), 1);
	for (int i = 0; i < al; i++) {
		if ((r = unpack_stack(m_exec,
			reinterpret_cast<const argument &>(o.elem(i)))) < 0) { goto error; }
	}
	return al;
error:
	lua_settop(m_exec, top);
	return r;
}

int lua::coroutine::unpack_stack_with_vm(VM main_vm) {
	TRACE("yue_resume: top = %u\n", lua_gettop(main_vm));
	ASSERT(lua_isuserdata(main_vm, 1));
	VM co = vm();
	ASSERT(lua_gettop(co) == 1 && lua_isfunction(co, 1));
	lua_pushvalue(co, 1);
	lua_remove(main_vm, 1);
	int r = lua_gettop(main_vm);
	if (r > 0) {
		lua_xmove(main_vm, co, r);
	}
	TRACE("yue_resume: co stack %d\n", r);
	return r;
}

int lua::coroutine::unpack_stack(VM vm, const argument &o) {
	size_t i; int r;
	switch(o.kind()) {
	case rpc::datatype::NIL:
		lua_pushnil(vm);
		break;
	case rpc::datatype::BOOLEAN:
		lua_pushboolean(vm, (bool)o);
		break;
	case rpc::datatype::ARRAY:
		/* various lua types are passed as lua array */
		/* [ type:int(LUA_T*), arg1, arg2, ..., argN ] */
		switch((int)(o.elem(0))) {
		case LUA_TFUNCTION:
			/* [LUA_TFUNCTION, func code:blob] */
			if ((r = unpack_function(vm, o.elem(1))) < 0) { return r; }
			break;
		case LUA_TUSERDATA:
			/* [LUA_TUSERDATA, string: module, packed object:blob] */
			if ((r = call_custom_unpack(vm, o)) < 0) { return r; }
			break;
		default:/* error object */
			if (((int)(o.elem(0))) < 0) {
				lua_createtable(vm, o.size(), 0);
				for (i = 0; i < o.size(); i++) {
					lua_pushnumber(vm, (i + 1));
					unpack_stack(vm, o.elem(i));
					lua_settable(vm, -3);
				}
				return NBR_OK;
			}
			ASSERT(false);
			return NBR_EINVAL;
		}
		break;
	case rpc::datatype::MAP:
		lua_createtable(vm, o.size(), 0);
		for (i = 0; i < o.size(); i++) {
			unpack_stack(vm, o.key(i));
			unpack_stack(vm, o.val(i));
			lua_settable(vm, -3);
		}
		break;
	case rpc::datatype::BLOB:
		/* last null character should be removed */
		lua_pushlstring(vm, o, o.len() - 1);
		break;
//	case rpc::datatype::STRING:
//		lua_pushstring(vm, o);
//		break;
	case rpc::datatype::DOUBLE:
		lua_pushnumber(vm, (double)o);
		break;
	case rpc::datatype::INTEGER:
		lua_pushinteger(vm, (int)o);
		break;
	default:
		ASSERT(false);	/* cannot come here */
		return NBR_EINVAL;
	}
	return NBR_OK;
}

int lua::coroutine::unpack_function(VM vm, const argument &d) {
	if (lua_load(vm, lua::reader::callback, 
		const_cast<void *>(
			reinterpret_cast<const void *>(&d)
		), "rf") < 0) {
		TRACE("lua_load error <%s>\n", lua_tostring(vm, -1));
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

int lua::coroutine::call_custom_unpack(VM vm, const argument &d) {
	int orgtop = lua_gettop(vm);
	lua_getglobal(vm, "require");	//1
	lua_pushstring(vm, d.elem(1));	//2
	if (lua_pcall(vm, 1, 1, 0) != 0) {	//1
		TRACE("unpack userdata fails 1 (%s)\n", lua_tostring(vm, -1));
		goto error;
	}
	lua_getfield(vm, -1, lua::unpack_method);	//2
	if (lua_isfunction(vm, -1)) {
		lua_pushlightuserdata(vm, const_cast<void *>(
			reinterpret_cast<const void *>(&(d.elem(2)))
		));				//3
		if (lua_pcall(vm, 1, 1, 0) != 0) {			//2
			TRACE("unpack userdata fails 2 (%s)\n", lua_tostring(vm, -1));
			goto error;
		}
		lua_remove(vm, -2);	/* remove module table */	//1
		return NBR_OK;
	}
error:
	lua_settop(vm, orgtop);
	lua_pushnil(vm);
	return NBR_OK;
}


/* lua */
extern "C" {
static lua_State *g_vm = NULL;
static lua *g_lib = NULL;
static server *g_server = NULL;
void output_logo(FILE *f) {
	fprintf(f, "__  ____ __ __    ____  \n");
	fprintf(f, "\\ \\ \\  // / \\ \\  / ___\\ \n");
	fprintf(f, " \\ \\/ / | | | | / /     \n");
	fprintf(f, "  \\  /  | | | | ~~~~~~~~    version %s(%s)\n", "0.1.0", LUAJIT_VERSION);
	fprintf(f, " _/ /   \\ \\_/ / \\ \\___  \n");
	fprintf(f, " \\_/     \\___/   \\____/  \n");
	fprintf(f, "it's brilliant on the cloud\n\n");
	fprintf(f, "(c)2009 - 2011 Takehiro Iyatomi(iyatomi@gmail.com)\n");

}
int luaopen_yue(lua_State *vm) {
	if (g_vm) { ASSERT(false); return -1; }
	g_vm = vm;
	lua_error_check(vm, (g_server = new server), "fail to create server");
	lua_error_check(vm, (g_server->init(NULL) >= 0), "fail to init server");
	lua_error_check(vm, (g_server->start() >= 0), "fail to start server");
	g_lib = &(fabric::tlf().lang());
	lua::ms_mode = lua::RPC_MODE_SYNC;
	ASSERT(fabric::served());
	output_logo(stdout);
	return 0;
}
void yue_poll() {
	lua::module::served()->poll();
}
#if defined(__USE_OLD_FIBER)
struct yue_libhandler {
	lua::coroutine *m_co;
	yue_ThreadCB m_cb;
	yielded_context m_ctx;
	static int nop(lua_State *, int) { return 0; }
	inline yue_libhandler(yue_ThreadCB cb) :
		m_cb(cb), m_ctx(fiber::handler(*this)) {}
	int operator () (fabric &f, object &o) {
		int r;
		switch((r = m_co->resume(o))) {
		case fiber::exec_error: case fiber::exec_finish:
			/* procedure finish or error happen (it should reply to caller actor) */
			m_cb(m_co->vm(), r != fiber::exec_error ? NBR_OK : NBR_EINVAL);
			break;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
			return NBR_OK;
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			return NBR_OK;
		default:
			ASSERT(false);
		}
		if (m_co) { m_co->free(); }
		TRACE("yue_lh: ptr delete : %p\n", this);
		delete this;
		return NBR_OK;
	}
	static inline yue_libhandler *org_ptr_from(yielded_context *yc) {
		return (yue_libhandler *)(((U8 *)yc) 
			- sizeof(lua::coroutine*) 
			- sizeof(void (*)(lua_State*,bool)));
	}
};
#endif
struct _yue_Fiber {
	PROCEDURE(callproc) *m_t;
	yue_FiberCallback m_cb;
	fiber::handler m_h;
	_yue_Fiber(yue_FiberCallback cb) : m_t(NULL), m_cb(cb), m_h(*this) {}
	inline int operator () (fabric &fbr, object &o) {
		if (m_cb) { 
			return m_cb(
				reinterpret_cast<yue_Fiber>(this), 
				o.is_error() ? false : true); 
		}
		else { 
			delete this; 
			return NBR_OK; 
		}
	}

};
yue_Fiber yue_newfiber(yue_FiberCallback cb) {
#if !defined(__USE_OLD_FIBER)
	int r;
	_yue_Fiber *t = new _yue_Fiber(cb);
	fiber::rpcdata d;
	t->m_t = new(c_nil()) PROCEDURE(callproc)(t->m_h, d);
	if (!t->m_t) { goto error; }
	r = t->m_t->rval().init(fabric::tlf(), t->m_t);
	if (r < 0) { goto error; }
	return reinterpret_cast<yue_Fiber>(t);
error:
	ASSERT(false);
	if (t) {
		if (t->m_t) { t->m_t->fin(true); }
		delete t;
	}
	return NULL;
#else
	yue_libhandler *yh = new yue_libhandler(cb ? cb : yue_libhandler::nop);
	if (!yh) { ASSERT(false); return NULL; }
	if ((yh->m_co = g_lib->create(&(yh->m_ctx)))) {
		return yh->m_co->vm();
	}
	delete yh; return NULL;
#endif
}
lua_State *yue_getstate(yue_Fiber f) {
#if !defined(__USE_OLD_FIBER)
	_yue_Fiber *fb = reinterpret_cast<_yue_Fiber*>(f);
	return fb->m_t->rval().co()->vm();
#else
	return t;
#endif
}
void yue_deletefiber(yue_Fiber t) {
#if !defined(__USE_OLD_FIBER)
	ASSERT(t);
	delete reinterpret_cast<_yue_Fiber*>(t);
#endif
}
int yue_run(yue_Fiber t, int n_arg) {
#if !defined(__USE_OLD_FIBER)
	if (!t) { ASSERT(false); return LUA_ERRERR; }
	_yue_Fiber *th = reinterpret_cast<_yue_Fiber*>(t);
	switch(th->m_t->rval().co()->resume(n_arg)) {
	case fiber::exec_error:		/* unrecoverable error happen */
		if (th->m_cb) { th->m_cb(t, false); }
		th->m_t->fin(true);
		if (!th->m_cb) { delete th; }
		return LUA_ERRRUN;
	case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
		if (th->m_cb) { th->m_cb(t, true); }
		th->m_t->fin(false);
		if (!th->m_cb) { delete th; }
		return 0;
	case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
	case fiber::exec_delegate:	/* fiber send to another native thread. */
		return LUA_YIELD;
	default:
		ASSERT(false);
		return LUA_ERRERR;
	}
#else
	return LUA_ERRERR;
#endif
}
int yue_resume(lua_State *vm) {
#if !defined(__USE_OLD_FIBER)
	ASSERT(false);
#else
	TRACE("yue_resume: top = %u\n", lua_gettop(vm));
	lua::dump_stack(vm);
	lua_State *co = lua_tothread(vm, 1);
	ASSERT(lua_gettop(co) == 1 && lua_isfunction(co, 1));
	lua_pushvalue(co, 1);
	lua_remove(vm, 1);
	int r, n_arg = lua_gettop(vm);
	lua_xmove(vm, co, n_arg);
	TRACE("yue_resume: co stack\n");
	lua::dump_stack(co);
	/* stack layout: [called func][called func][arg 1]...[arg N]
	 * and after resume finished, [called func],[retval 1]...[retval M] */
	if ((r = lua_resume(co, n_arg)) != LUA_YIELD) {
		if (r != 0) {
			lua_xmove(co, vm, 1);
			lua_error(vm);
		}
		/* avoid first [called func] copied to caller vm */
		if ((r = (lua_gettop(co) - 1)) > 0) {
			lua_xmove(co, vm, r);
		}
		return r;
	}
	TRACE("yue_resume: after resume (%p:%d)\n", co, r);
	lua::dump_stack(co);
	TRACE("yue_resume: end\n");
	return 0;
#endif
}
int yueb_write(yue_Wbuf *yb, const void *p, int sz) {
	pbuf *pbf = reinterpret_cast<pbuf *>(yb);
	if (pbf->reserve(sz) < 0) { return NBR_EMALLOC; }
	util::mem::copy(pbf->last_p(), p, sz);
	pbf->commit(sz);
	return pbf->last();
}
const void *yueb_read(yue_Rbuf *yb, int *sz) {
	argument *a = reinterpret_cast<argument *>(yb);
	*sz = a->len();
	return a->operator const void *();
}
}
int lua::init(const char *bootstrap, int max_rpc_ongoing)
{
	/* initialize fiber system */
	if (!m_pool.init(max_rpc_ongoing, -1, opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_alloc.init(max_rpc_ongoing, -1, opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_smp.init(100000, -1, opt_expandable)) {
		return NBR_EMALLOC;
	}
	/* because always use single-thread mode for yue module, tlf is always valid. */
	m_attached = &(fabric::tlf());
	/* basic lua initialization. m_vm = g_vm is ugly hack to using entire system as lua module...
	 * for lua-module mode, there is only one instance of lua class (because lua class created
	 * corresponding native thread and on lua-module mode, only 1 thread created) */
	if (!(m_vm = g_vm) && !(m_vm = lua_newvm(allocator, this))) {
		return NBR_ESYSCALL;
	}
	lua_settop(m_vm, 0);
	/* set panic callback */
	lua_atpanic(m_vm, panic);
	/* load basic library */
	lua_pushcfunction(m_vm, luaopen_base);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}//*/
	/* load package library */
	lua_pushcfunction(m_vm, luaopen_package);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}//*/

	/* create yue module */
	module::init(m_vm, attached()->served());

	/* load kernel script: (if exists) */
	return bootstrap ? load_module(bootstrap) : NBR_OK;
}

/* load module */
int lua::load_module(const char *srcfile)
{
	/* loadfile only load file into lua stack (thus soon it loses)
	 * so need to call this chunk. */
	if (luaL_loadfile(m_vm, srcfile) != 0) {	/* 1:srcfile func */
		TRACE("luaL_loadfile : error <%s>\n", lua_tostring(m_vm, -1));
		ASSERT(false);
		return NBR_ESYSCALL;
	}
	if (lua_pcall(m_vm, 0, 0, 0) != 0) {            /* 1->removed */
		TRACE("lua_pcall : error <%s>\n", lua_tostring(m_vm, -1));
		ASSERT(false);
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

void lua::fin()
{
	if (m_vm) {
		lua_close(m_vm);
		m_vm = NULL;
	}
	m_pool.fin();
	m_alloc.fin();
	m_smp.fin();
	m_attached = NULL;
}

int lua::panic(VM vm)
{
	fprintf(stderr, "lua: panic: <%s>\n", lua_tostring(vm, -1));
	ASSERT(false);
	return 0;
}

void lua::dump_stack(VM vm) {
	TRACE("vm:%p\n", vm);
	for (int i = 1; i <= lua_gettop(vm); i++) {
		TRACE("index[%u]=%u(", i, lua_type(vm, i));
		switch(lua_type(vm, i)) {
		case LUA_TNIL: 		TRACE("nil"); break;
		case LUA_TNUMBER:	TRACE("%lf", lua_tonumber(vm, i)); break;
		case LUA_TBOOLEAN:	TRACE(lua_toboolean(vm, i) ? "true" : "false"); break;
		case LUA_TSTRING:	TRACE("%s", lua_tostring(vm, i));break;
		case LUA_TTABLE:	TRACE("table"); break;
		case LUA_TFUNCTION: TRACE("function"); break;
		case LUA_TUSERDATA: TRACE("userdata"); break;
		case LUA_TTHREAD:	TRACE("thread"); break;
		case LUA_TLIGHTUSERDATA:	TRACE("%p", lua_touserdata(vm, i)); break;
		default:
			//we never use it.
			ASSERT(false);
			return;
		}
		TRACE(")\n");
	}
}

int lua::copy_table(VM vm, int from, int to, int type)
{
	//TRACE("copy_table(%u) : %d -> %d\n", type, from, to);
	int cnt = 0;
	ASSERT(lua_istable(vm, from) && lua_istable(vm, to));
	lua_pushnil(vm);        /* push first key (idiom, i think) */
	while(lua_next(vm, from)) {     /* put next key/value on stack */
		if (type > 0 && lua_type(vm, -1) != type) { continue; }
		const char *k = lua_tostring(vm, -2);
		if (!k) { continue; }
		// TRACE("add element[%s]:%u:%u\n", k, lua_type(vm, -1), lua_gettop(vm));
		lua_setfield(vm, to, k);
		cnt++;
	}
	return cnt;
}

void lua::dump_table(VM vm, int index)
{
	if (index < 0 && index > -lua_gettop(vm)) {
		index = (lua_gettop(vm) + index + 1);
	}
	ASSERT(index > 0 || index <= -10000);      /* should give positive index(because minus index
				changes its meaning after lua_pushnil below) */
	lua_pushnil(vm);	/* push first key (idiom, i think) */
	printf("table ptr = %p\n", lua_topointer(vm, index));
	if (lua_topointer(vm, index) == NULL) { return; }
	while(lua_next(vm, index)) {     /* put next key/value on stack */
		printf("table[%s]=%u\n", lua_tostring(vm, -2), lua_type(vm, -1));
		lua_pop(vm, 1);
	}
}

}
}
}
