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
#include "app.h"
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
//server *lua::module::m_server = NULL;
U32 lua::ms_mode = lua::RPC_MODE_NORMAL;
lua::accept_watcher lua::m_w;

const char lua::kernel_table[] 				= "__kernel";
const char lua::index_method[] 				= "__index";
const char lua::newindex_method[] 			= "__newindex";
const char lua::call_method[] 				= "__call";
const char lua::gc_method[] 				= "__gc";
const char lua::len_method[]				= "__len";
const char lua::pack_method[] 				= "__pack";
const char lua::unpack_method[] 			= "__unpack";
const char lua::actor_metatable[] 			= "__actor_mt";
const char lua::rmnode_metatable[] 			= "__rmnode_mt";
const char lua::rmnode_sync_metatable[] 	= "__rmnode_s_mt";
const char lua::thread_metatable[] 			= "__thread_mt";
const char lua::future_metatable[] 			= "__future_mt";
const char lua::error_metatable[] 			= "__error_mt";
const char lua::sock_metatable[] 			= "__sock_mt";
const char lua::module_name[] 				= "yue";
const char lua::ldname[]					= "_LD";
const char lua::watcher[]					= "_WCH";
const char lua::tick_callback[]				= "tick";



/* lua::method */
char lua::method::prefix_NOTIFICATION[] 	= "notify_";
char lua::method::prefix_CLIENT_CALL[] 		= "client_";
char lua::method::prefix_TRANSACTIONAL[] 	= "tr_";
char lua::method::prefix_QUORUM[] 			= "quorum_";
char lua::method::prefix_TIMED[] 			= "timed_";



/******************************************************************************************/
/* callback_runner::session_delegator */
template <class T>
void session_delegator::impl<T>::delegate(fabric *fbr, args &a) {
	fiber::phandler h(*(reinterpret_cast<session_delegator::impl<T>*>(this)));
	session::session_event_message *smsg =
		session::alloc_event_message(a.s, a.st);
	if (!smsg) { ASSERT(false); return; }
	fbr->delegate(h, smsg);
	return;
}
template <class T>
int session_delegator::impl<T>::operator () (fabric &fbr, void *p) {
	session::session_event_message *smsg =
		reinterpret_cast<session::session_event_message *>(p);
	if (smsg->m_h.valid()) {
		session::free_event_message(smsg);
		return NBR_EINVAL;
	}
	reinterpret_cast<T *>(this)->operator ()(smsg->m_h.m_s, smsg->m_state);
	session::free_event_message(smsg);
	return NBR_OK;
}
template <class RUNNER, class ARG>
static inline int callback_runner(RUNNER &runner, ARG &a) {
	fabric *fbr = &(fabric::tlf());
	if (runner.attached() == fbr) {
		fiber::rpcdata d;
		PROCEDURE(callproc) *p = new (c_nil()) PROCEDURE(callproc)(c_nil(), d);
		if (!p) {
			ASSERT(false);
			return NBR_EMALLOC;
		}
		int r = p->rval().init(*fbr, p);
		if (r < 0) {
			ASSERT(false);
			return NBR_EINVAL;
		}
		VM vm = p->rval().co()->vm();
		int n_args = runner.setup(vm, a);
		if (n_args < 0) {
			ASSERT(n_args == NBR_ECANCEL);
			p->fin(false);
			return n_args;
		}
		switch(p->rval().co()->resume(n_args)) {
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
		runner.delegate(runner.attached(), a);
		return NBR_EPENDING;
	}
}



/* sub modules */
#include "future.h"
#include "timer.h"
#include "socket.h"
#include "utility.h"



/******************************************************************************************/
/* class lua */
int lua::method::init(VM vm, actor *a, const char *name, method *parent) {
	method *m = reinterpret_cast<method *>(
		lua_newuserdata(vm, sizeof(method))
	);
//	lua_newtable(vm);	/* child method object cache */
	const char *n = parse(name, m->m_attr); /* name is referred by VM so never freed */
	if (parent) {
		size_t nl = util::str::length(n), pl = util::str::length(parent->m_name);
		char *tmp, *buff;
		if (!(buff = reinterpret_cast<char *>(
			util::mem::alloc(nl + pl + 1 + 1)))) {//"." and "\0"
			lua_pushfstring(vm, "cannot allocate %u+%u byte", nl, pl);
			lua_error(vm);
		}
		tmp = buff;
		tmp += util::str::copy(tmp, parent->m_name);
		*tmp++ = '.';
		tmp += util::str::copy(tmp, n);
		TRACE("buff = %s\n", buff);
		m->m_attr |= ALLOCED;
		m->m_name = reinterpret_cast<const char*>(buff);
	}
	else {
		m->m_name = n;
	}
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
//	lua_setmetatable(vm, -2);	/* it will be metatable of child method object cache */
	lua_setmetatable(vm, -2);	/* child method object cache will be metatable of method object */
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
		PARSE(r, TIMED, attr, continue);
		if ((!(attr & (TIMED | NOTIFICATION)))) {
			PARSE(r, TRANSACTIONAL, attr, break);
			PARSE(r, QUORUM, attr, break);
		}
		break;
	}
	TRACE("parse:%s>%s:%08x\n", name, r, attr);
	return r;
}
int lua::method::index(VM vm) {
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	ASSERT(lua_isstring(vm, 2));
	method::init(vm, m->m_a, lua_tostring(vm, 2), m);
	return 1;
}
int lua::method::gc(VM vm) {
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	if (m->m_attr & ALLOCED) {
		util::mem::free(reinterpret_cast<void *>(
			const_cast<char *>(m->m_name)
		));
	}
	return 1;
}
class actor_accesible_fiber : public fiber {
	friend class lua::actor;
};
int lua::actor::push_address_from(VM vm, yielded_context *y) {
	if (y->type() == fabric::yielded::type_fiber) {
		actor_accesible_fiber *f =
			reinterpret_cast<actor_accesible_fiber *>(y->fb());
		switch (f->type()) {
		case fiber::from_stream:
		case fiber::from_datagram: {
			char uri[32];
			lua_pushstring(vm, f->stream_ref().m_s->uri(uri, sizeof(uri)));
			return 1;
		}
		case fiber::from_thread:
			lua_pushinteger(vm, server::get_thread_idx(f->thread_ref().m_l));
			return 1;
		default:
			break;
		}
	}
	return 0;
}
template <>
int lua::method::call<lua::session>(VM vm) {
	/* stack = method, a1, a2, ..., aN */
	coroutine *co = coroutine::to_co(vm);
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	session *s = m->m_a->get_session(); U32 timeout = 0/* means using default timeout */;
	/* if already  */
	lua_error_check(vm, s, "already closed");
	if (m->timed()) {
		/* when timed, stack = method,timeout,a1,a2,...,aN */
		timeout = ((U32)lua_tonumber(vm, 2) * 1000 * 1000);
		lua_remove(vm, 2);
	}
	if (m->has_future()) {
		/* TODO: remove this code */
		if (!s->valid()) {
			lua_error_check(vm,
				(s->sync_connect(fabric::tlf().tla()) >= 0), "session::connect");
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
		lua_error_check(vm, (s->reconnect(*co) >= 0), "reconnect");
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
	ASSERT(m->m_a && m->m_a->get_session());
	session *s = m->m_a->get_session();
	loop &la = *loop::tls();
	if (!s || !s->valid()) {
		lua_error_check(vm, (s->sync_connect(la) >= 0), "session::connect");
	}
	object o; int r;
	lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*s, a), "callproc");
	lua_error_check(vm, s->sync_write(la) >= 0, "session::send");
	lua_error_check(vm, s->sync_read(la, o) >= 0, "session::recv");
	lua_error_check(vm, ((r = co.unpack_response_to_stack(o)) >= 0), "invalid response");
	if (o.is_error()) { lua_error(vm); }
	return r;
}
template <>
int lua::method::call<server>(VM vm) {
	coroutine *co = coroutine::to_co(vm); U32 timeout = 0;
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	if (m->timed()) {
		/* when timed, stack = method,timeout,a1,a2,...,aN */
		timeout = ((U32)lua_tonumber(vm, 2) * 1000 * 1000);
		lua_remove(vm, 2);
	}
	if (m->has_future()) {
		future *ft = future::init(vm, co->ll(), m->timed());
		co = ft->m_co;
	}
	PROCEDURE(callproc)::args_and_cb<yielded_context> a(*(co->yldc()));
	a.m_co = co;
	a.m_timeout = timeout;
	session::loop_handle lh(m->m_a->m_la);
	lua_error_check(vm, INVALID_MSGID != yue::rpc::call(lh, a), "callproc");
	return m->has_future() ? 1 : co->yield();
}

/**/



/* lua::module */
void lua::module::init(VM vm, server *srv) {
	/* yue module. */
//	module *m = reinterpret_cast<module *>(lua_newuserdata(vm, sizeof(module)));
//	m->m_server = srv;
	//m_server = srv;
	lua_newtable(vm);
	/* meta table */
//	lua_newtable(vm);
//	lua_pushcfunction(vm, index);
//	lua_setfield(vm, -2, index_method);
//	lua_setmetatable(vm, -2);
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
	/* API 'sleep' */
	lua_pushcfunction(vm, sleep);
	lua_setfield(vm, -2, "sleep");
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
	/* API 'exit' */
	lua_pushcfunction(vm, exit);
	lua_setfield(vm, -2, "exit");
	/* API 'listen' */
	lua_pushcfunction(vm, listen);
	lua_setfield(vm, -2, "listen");
	/* API 'peer' */
	lua_pushcfunction(vm, peer);
	lua_setfield(vm, -2, "peer");
	/* API 'error' */
	lua_pushcfunction(vm, error);
	lua_setfield(vm, -2, "error");
	/* API 'read' */
	lua_pushcfunction(vm, read);
	lua_setfield(vm, -2, "read");
	/* API 'write' */
	lua_pushcfunction(vm, write);
	lua_setfield(vm, -2, "write");
	/* API symbol 'ldname' */
	lua_pushstring(vm, lua::ldname);
	lua_setfield(vm, -2, "ldname");
	/* API symbol 'watcher' */
	lua_pushstring(vm, lua::watcher);
	lua_setfield(vm, -2, "watcher");
	/* API table 'tick' */
	lua_pushcfunction(vm, nop);
	lua_setfield(vm, -2, "tick");
	/* API table 'pack' */
	lua_newtable(vm);
	lua_pushcfunction(vm, method::pack);
	lua_setfield(vm, -2, "method");
	lua_pushcfunction(vm, actor::pack);
	lua_setfield(vm, -2, "actor");
	lua_setfield(vm, -2, "pack");
	/* API constant 'bootimage' */
	if (srv->bootstrap_source()) {
		lua_pushstring(vm, srv->bootstrap_source());
	}
	else {
		lua_pushnil(vm);
	}
	lua_setfield(vm, -2, "bootimage");
	/* API value 'thread_count' */
	lua_pushinteger(vm, served::thread_count());
	lua_setfield(vm, -2, "thread_count");
	/* API 'socket' */
	lua_pushcfunction(vm, sock::init);
	lua_setfield(vm, -2, "socket");
	/* API table 'registry' */
	lua_pushlightuserdata(vm, vm);
	lua_newtable(vm);
	lua_settable(vm, LUA_REGISTRYINDEX);
	lua_pushlightuserdata(vm, vm);
	lua_gettable(vm, LUA_REGISTRYINDEX);
	lua_setfield(vm, -2, "registry");
	/* API 'listeners */
	lua_pushcfunction(vm, listeners);
	lua_setfield(vm, -2, "listeners");
	/* API 'accepted' */
	lua_pushcfunction(vm, accepted);
	lua_setfield(vm, -2, "accepted");
	/* API table 'command_line' */
	lua_newtable(vm);
	for (int i = 0; i < served::argc(); i++) {
		lua_pushinteger(vm, i);
		lua_pushstring(vm, served::argv()[i]);
		lua_settable(vm, -3);
	}
	lua_setfield(vm, -2, "command_line");

	/* add submodule util */
	utility::init(vm);
	lua_setfield(vm, -2, "util");

	/* give global name 'yue' */
	lua_setfield(vm, LUA_GLOBALSINDEX, module_name);

	/* 3 common metatable for method object */
	method::init_metatable(vm, method::call<session>);
	lua_setfield(vm, LUA_GLOBALSINDEX, rmnode_metatable);
	method::init_metatable(vm, method::sync_call);
	lua_setfield(vm, LUA_GLOBALSINDEX, rmnode_sync_metatable);
	method::init_metatable(vm, method::call<server>);
	lua_setfield(vm, LUA_GLOBALSINDEX, thread_metatable);

	/* metatable for actor object */
	//actor::init_metatable(vm);
	//lua_setfield(vm, LUA_GLOBALSINDEX, actor_metatable);

	/* error metatable */
	lua_newtable(vm);
	lua_setfield(vm, LUA_GLOBALSINDEX, error_metatable);

	/* future metatable */
	lua_newtable(vm);
	lua_pushcfunction(vm, future::callback);
	lua_setfield(vm, -2, "callback");
	lua_pushvalue(vm, -1);	/* metatabl it self as index */
	lua_setfield(vm, -2, lua::index_method);
	lua_setfield(vm, LUA_GLOBALSINDEX, future_metatable);

	/* sock metatable */
	sock::init_metatable(vm);
	lua_setfield(vm, LUA_GLOBALSINDEX, sock_metatable);

}
int lua::module::peer(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	if (actor::push_address_from(vm, co->yldc()) > 0) {
		return accepted(vm);
	}
	else {
		lua_pushnil(vm);
		return 1;
	}
}
int lua::module::index(VM vm) {
	/* access like obj[key]. -1 : key, -2 : obj */
	//module *m = reinterpret_cast<module *>(lua_touserdata(vm, -2));
	lua_getmetatable(vm, -2);
	lua_pushvalue(vm, -2);	/* dup key */
	switch(lua_type(vm, -1)) {
	case LUA_TSTRING: {
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			session *s; const char *k = lua_tostring(vm, -2);
			lua_error_check(vm, (k && (s = served::spool().add_to_mesh(k, NULL))),
				"add_to_mesh");
			actor::init(vm, s);
			lua_pushvalue(vm, -3);	/* dup key (k) */
			lua_pushvalue(vm, -2);	/* dup actor */
			lua_settable(vm, -3);	/* set to module metatable. */
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
			lua_error_check(vm, (k && (s = served::spool().add_to_mesh(k, &obj))),
				"add_to_mesh");
			actor::init(vm, s);
			lua_pushvalue(vm, -3);	/* dup key (k) */
			lua_pushvalue(vm, -2);	/* dup actor */
			lua_settable(vm, -3);	/* set to module metatable. */
			/* now stack layout is actor, module (from top) */
			return 1;
		}
	} break;
	case LUA_TNUMBER: {
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			server *la; int k = (int)lua_tonumber(vm, -2);
			lua_error_check(vm, (la = served::get_thread(k)), "get_thread(%d)", k);
			actor::init(vm, la);
			lua_pushvalue(vm, -3);	/* dup key (k) */
			lua_pushvalue(vm, -2);	/* dup actor */
			lua_settable(vm, -3);	/* set module table. */
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
		s = served::spool().open(lua_tostring(vm, -1), NULL);
	} break;
	case 2: {
		const char *addr;
		lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
		if (lua_istable(vm, top)) {
			object obj;
			lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
					"obj from table");
			s = served::spool().open(addr, &obj);
		}
		else {
			s = served::spool().open(addr, NULL);
		}
	} break;
	}
	lua_error_check(vm, s, "create session");
	return actor::init(vm, s);
}
int lua::module::stop(VM vm) {
	served::app().die();
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
	VM co = fb->rval().co()->vm();
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
	return r;
}
int lua::module::exit(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	co->set_flag(lua::coroutine::FLAG_EXIT, true);
	return co->yield();
}
int lua::module::yield(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	lua::dump_stack(vm);
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
int lua::module::sleep(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	loop::timer_handle t;
	util::functional<int (loop::timer_handle)> h(*co);
	if (!(t = served::set_timer(lua_tonumber(vm, -1), 1.0f, h))) {
		lua_pushfstring(vm, "create timer");
		lua_error(vm);
	}
	return co->yield();
}
int lua::module::accepted(VM vm) {
	ASSERT(lua_gettop(vm) > 1);
	//TRACE("module::accepted\n");
	//lua::dump_stack(vm);
	switch(lua_type(vm, -1)) {
	case LUA_TSTRING: {
		lua_pushvalue(vm, -1);		//dup key for raw get
		lua_rawget(vm, -3);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			session *s; const char *k = lua_tostring(vm, -1);
			ASSERT(k);
			lua_error_check(vm, (k && (s = served::served_for(k))), "served_for");
			actor::init(vm, s);
			lua_pushvalue(vm, -2);	/* dup key */
			lua_pushvalue(vm, -2);	/* dup actor */
			//lua::dump_table(vm, -5);
			lua_settable(vm, -5);	/* set to module metatable. */
			/* now stack layout is actor, key, ..., module (from top) */
			return 1;
		}
		//TRACE("string key: use cache\n");
	} break;
	case LUA_TNUMBER: {
		lua_pushvalue(vm, -1);		//dup key for raw get
		lua_rawget(vm, -3);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			server *la; int k = (int)lua_tonumber(vm, -1);
			lua_error_check(vm, (la = served::get_thread(k)), "get_thread");
			actor::init(vm, la);
			lua_pushvalue(vm, -2);	/* dup key */
			lua_pushvalue(vm, -2);	/* dup actor */
			lua_settable(vm, -5);	/* set module table. */
			/* now stack layout is actor, key, ..., module (from top) */
			return 1;
		}
	} break;
	default:
		lua_rawget(vm, -2);
		break;
	}
	return 1;
}
int lua::module::listeners(VM vm) {
	if (!lua_isstring(vm, -1)) {
		lua_pushnil(vm);
		return 1;
	}
	lua_pushinteger(vm, served::get_listener_from(lua_tostring(vm, -1)));
	return 1;
}
int lua::module::command_line(VM vm) {
	lua_newtable(vm);
	for (int i = 0; i < served::argc(); i++) {
		lua_pushinteger(vm, i);
		lua_pushstring(vm, served::argv()[i]);
		lua_settable(vm, -2);
	}
	return 1;
}
int lua::module::configure(VM vm) {
	const char *k, *v;
	lua_error_check(vm,
		((v = lua_tostring(vm, -1)) && (k = lua_tostring(vm, -2))),
		"invalid parameter %p %p", k, v);
	lua_error_check(vm, served::cfg().configure(k, v) >= 0, "invalid config %s %s", k, v);
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
		lua_pushinteger(vm, served::listen(lua_tostring(vm, -1)));
	} break;
	case 2: {
		const char *addr;
		lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
		if (lua_istable(vm, top)) {
			object obj;
			lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
					"obj from table");
			lua_pushinteger(vm, served::listen(addr, &obj));
			obj.fin();
		}
		else {
			lua_pushinteger(vm, served::listen(addr, NULL));
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
		method::init(vm, a, k, NULL); /*5*/
		lua_pushvalue(vm, -4);	/* dup key */ /*7*/
		lua_pushvalue(vm, -2);	/* dup method object */ /*6*/
		lua_settable(vm, 3);	/* set method object to metatable */
	}
	return 1;
}
int lua::actor::fin(VM vm) {
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -1));
	switch(a->m_kind) {
	case actor::RMNODE:
#if defined(_DEBUG)
	{
		lua_getfield(vm, -1, "__addr");
		TRACE("actor(%s) finalized\n", lua_tostring(vm, -1));
		lua_pop(vm, 1);
	}
#endif
		if (a->m_w) { a->m_w->kill(); a->m_w = NULL; }
		break;
	case actor::THREAD:
		break;
	}
	lua::unref(vm, a);
	return 0;
}
int lua::actor::addr(VM vm) {
	actor *a = reinterpret_cast<actor *>(
		lua_touserdata(vm, -1)
	);
	if (!a) {
		ASSERT(false);
		lua_pushnil(vm);
		return 1;
	}
	char uri[32];
	switch(a->m_kind) {
	case RMNODE: {
		lua_pushstring(vm, a->m_rm.m_s->uri(uri, sizeof(uri)));
	} break;
	case THREAD:
		lua_pushinteger(vm, server::get_thread_idx(a->m_la));
		break;
	default:
		lua_pushnil(vm);
		ASSERT(false);
		break;
	}
	return 1;
}
int lua::actor::close(VM vm) {
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -1));
	if (!a) {
		ASSERT(false);
		return 0;
	}
	session *s = a->get_session();
	if (s) {
		s->close();
	}
	return 0;
}
int lua::actor::permit_access(VM vm) {
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -1));
	if (!a) {
		ASSERT(false);
		return 0;
	}
	session *s = a->get_session();
	if (s) {
		s->permit_access();
	}
	return 0;
}
int lua::actor::fd(VM vm) {
	actor *a = reinterpret_cast<actor *>(lua_touserdata(vm, -1));
	if (!a) {
		ASSERT(false);
		lua_pushnil(vm);
		return 1;
	}
	session *s = a->get_session();
	if (s) {
		lua_pushinteger(vm, s->fd());
	}
	else {
		ASSERT(false);
		lua_pushnil(vm);
	}
	return 1;
}
bool lua::actor::set(server *la) {
	m_ll = &(fabric::tlf().lang());
	m_la = la; m_kind = THREAD;
	return true;
}
bool lua::actor::set(session *s) {
	m_ll = &(fabric::tlf().lang());
	m_rm.m_s = s;
	m_rm.m_sn = s->serial();
	m_kind = RMNODE;
	if (!s->valid() || this->operator () (s, s->afd() != INVALID_FD ?
			session::SVESTABLISH :
			session::ESTABLISH)) {
		s->add_watcher(*this);
	}
	return true;
}

fabric *lua::actor::attached() { return ll().attached(); }

int lua::actor::setup(VM vm, session_delegator::args &a) {
//	TRACE("actor::setup %p %u\n", a.s, a.st);
	lua_pushlightuserdata(vm, this);	//1
	lua_pushvalue(vm, -1);	//2
	lua_gettable(vm, LUA_REGISTRYINDEX);//2
	lua_getmetatable(vm, -1);	//3
	ASSERT(lua_istable(vm, -1));
	switch (a.st) {
	case session::WAITACCEPT:
		lua_getfield(vm, -1, "__accepted");//4
		break;
	case session::ESTABLISH:
		if (a.s->skip_server_accept()) {
			lua_getfield(vm, -1, "__accepted");//4
		}
		else {
			return NBR_ECANCEL;
		}
		break;
	case session::CLOSED:
		lua_getfield(vm, -1, "__closed");//4
		break;
	default:
		return NBR_ECANCEL;
	}
	if (lua_isnil(vm, -1)) {
		return NBR_ECANCEL;
	}
	lua_pushvalue(vm, -3);
	return 1;
}
bool lua::actor::operator () (session *s, int state) {
	session_delegator::args a = { s, state };
	callback_runner(*this, a);
	return true; //r != NBR_EPENDING && r != NBR_ECANCEL;
}



/* lua::accept_watcher */
fabric *lua::accept_watcher::attached() { return &(fabric::tlf()); } /* always processed */
int lua::accept_watcher::setup(VM vm, session_delegator::args &a) {
	if (a.s->afd() != INVALID_FD && a.s->afd() != a.s->fd()/* == happen udp server port */) {
		lua::module::registry(vm, attached()->lang().vm());
		lua_getfield(vm, -1, lua::module_name);
		//lua::dump_table(vm, lua_gettop(vm));
		char uri[32];
		lua_pushstring(vm, a.s->uri(uri, sizeof(uri)));
		/* we can assume these accept watcher only calls once even if multiple VM thread run.
		 * because in such a case client never invoke RPC until this watcher finished execute
		 * (and call __permit_access()) and after that client call RPC, this session already
		 * turned to established and so another thread never receive notification that session state
		 * turned to be SVWAITACCEPT. so is same as close watcher.
		 * so if you want to store some data that related accept connection in lua side,
		 * you should do it in accept watcher by using yue.core.each_thread.
		 * */
		if (a.st == session::SVWAITACCEPT || a.st == session::CLOSED || a.st == session::FINALIZED) {
			lua::module::accepted(vm);
			lua_pushinteger(vm, a.s->afd());
			lua_pushinteger(vm, a.st);
			lua_getglobal(vm, lua::watcher);
			ASSERT(lua_isfunction(vm, -1));
			lua_replace(vm, -5);
			return 3;
		}
	}
	return NBR_ECANCEL;
}
bool lua::accept_watcher::operator () (session *s, int state) {
	session_delegator::args a = { s, state };
	callback_runner(*this, a);
	return true;
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
		if (m_w) { m_w->kill(); m_w = NULL; }
	}
}

void lua::coroutine::fin_with_context(int result) {/* may call from connect handler
	(because it not called from fabric) */
	if (m_y) {
		m_y->on_respond(result, *(ll().attached()));
		m_y->fin(result == constant::fiber::exec_error);
	}
	fin();
}

int lua::coroutine::operator () (fabric &fbr, void *p) {
	session::session_event_message *smsg = 
		reinterpret_cast<session::session_event_message *>(p);
	if (smsg->m_h.valid()) {
		session::free_event_message(smsg);
		return NBR_EINVAL;
	}
	this->operator ()(smsg->m_h.m_s, smsg->m_state);
	session::free_event_message(smsg);
	return NBR_OK;
}

bool lua::coroutine::operator () (session *s, int state) {
	TRACE("coro:op() %p, %u, %p\n", s, state, m_exec);
	if (ll().attached() != &(fabric::tlf())) {
		fiber::phandler h(*this);
		session::session_event_message *smsg = 
			session::alloc_event_message(s, state);
		if (!smsg) { ASSERT(false); return false; }
		ll().attached()->delegate(h, smsg);
		m_w = NULL;
		return false;
	}
	//lua::dump_stack(m_exec);
	int r = fiber::exec_invalid;
	if (state == session::FINALIZED) {
		m_w = NULL;
	}
	if (has_flag(FLAG_CONNECT_RAW_SOCK)) {
		if (state == session::ESTABLISH) {
			set_flag(FLAG_CONNECT_RAW_SOCK, false);
			r = sock::try_connect(s, this, true);
		}
		else if (state == session::CLOSED) {
			set_flag(FLAG_CONNECT_RAW_SOCK, false);
			lua_pushnil(m_exec);
			r = resume(1);
		}
	}
	else if (has_flag(FLAG_READ_RAW_SOCK)) {
		if (state == session::RECVDATA) {
			set_flag(FLAG_READ_RAW_SOCK, false);
			//r = sock::read_cb(s, this, true);
			r = resume(0);
		}
	}
	else if (state == session::ESTABLISH) {
		PROCEDURE(callproc)::args_and_cb<yielded_context> a(*yldc());
		a.m_co = this;
		r = (INVALID_MSGID != yue::rpc::call(*s, a) ?
			fiber::exec_yield : fiber::exec_error);
	}
	if (r == fiber::exec_error || r == fiber::exec_finish) {
		fin_with_context(r);
	}
	if (r == fiber::exec_invalid) {
		return true;
	}
	else {
		m_w = NULL;
		return false;
	}
}

int lua::coroutine::resume(int r) {
	/* TODO: when exec_error we should reset coroutine state.
	 * without reset coroutine, it does not run correctly when reused. but how?
	 * (now we call coroutine::fin() and destroy coroutine object. but its not efficient way) */
//	if (lua_gettop(m_exec) == 4 && lua_tointeger(m_exec, 2) == 100 && lua_tointeger(m_exec, 4) == 10) {
//		ASSERT(false);
//	}
	if ((r = lua_resume(m_exec, r)) == LUA_YIELD) {
		/* this coroutine uses yield as long-jump (global exit) */
		if (has_flag(lua::coroutine::FLAG_EXIT)) { return fiber::exec_finish; }
		return fiber::exec_yield;	/* successfully suspended */
	}
	else if (r != 0) {	/* error happen */
		TRACE("fiber failure %d <%s>\n",r,lua_tostring(m_exec, -1));
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
	if ((r = sr.unpack(sr.pack_buffer())) != serializer::UNPACK_SUCCESS) {
		return NBR_EFORMAT;
	}
	obj = sr.result();
	return NBR_OK;
}

int lua::coroutine::get_object_from_stack(VM vm, int start_id, object &obj) {
	serializer sr; pbuf pbf;int r;
	if (pbf.reserve(256) < 0) { return NBR_EMALLOC; }
	sr.start_pack(pbf);
	if ((r = coroutine::pack_stack_as_response(vm, start_id, sr)) < 0) { return r; }
	if ((r = sr.unpack(sr.pack_buffer())) != serializer::UNPACK_SUCCESS) {
		return NBR_EFORMAT;
	}
	obj = sr.result();
	return NBR_OK;
}


/* pack */
int lua::coroutine::pack_stack_as_rpc_args(serializer &sr) {
	int top = lua_gettop(m_exec), r;
	if (top <= 0) {
		/* top == 0 means this function not returns any value */
		sr.push_array_len(0); return sr.len();
	}
	method *m = reinterpret_cast<method *>(lua_touserdata(m_exec, 1));
	if (!m) {
		ASSERT(false); sr.push_array_len(0); return sr.len();
	}
	sr.push_array_len(top);
	verify_success(sr.push_raw(m->m_name, util::str::length(m->m_name)));
	for (int i = 2; i <= top; i++) {
		if ((r = pack_stack(m_exec, sr, i)) < 0) { return r; }
	}
	return sr.len();
}

int lua::coroutine::pack_stack_as_response(VM vm, int start_id, serializer &sr) {
	int top = lua_gettop(vm), r;
	if (top == 0 || start_id > top) {
		/* top == 0 means this function not returns any value */
		ASSERT(top == 0); sr.push_array_len(0); return sr.len();
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
	ASSERT(stkid > 0);
	//TRACE("pack_stack type = %u\n", lua_type(vm, stkid));
	switch(lua_type(vm, stkid)) {
	case LUA_TNIL: 		retry(sr.pushnil()); break;
	case LUA_TNUMBER:	retry(sr << lua_tonumber(vm, stkid)); break;
	case LUA_TBOOLEAN:	retry(sr << (lua_toboolean(vm, stkid) ? true : false));break;
	case LUA_TSTRING:	
		retry(sr.push_raw(lua_tostring(vm, stkid), lua_objlen(vm, stkid)));
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
		ASSERT(lua_isfunction(vm, stkid));
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
//	TRACE("--- b4 count table --- (top:%u)\n", lua_gettop(vm);
//	lua::dump_stack(vm);
//	TRACE("---------------------\n");
	lua_pushnil(vm);        /* push first key */
	while(lua_next(vm, stkid)) {
		tblsz++;
		lua_pop(vm, 1);
	}
//	TRACE("--- b4 pack table ---(%u)\n", tblsz);
//	lua::dump_stack(vm);
//	TRACE("---------------------\n");
	sr.push_map_len(tblsz);
	lua_pushnil(vm);        /* push first key (idiom, i think) */
	while(lua_next(vm, stkid)) {    /* put next key/value on stack */
//		TRACE("--- dur pack table ---\n");
//		lua::dump_stack(vm);
//		TRACE("---------------------\n");
		int top = lua_gettop(vm);       /* use absolute stkid */
		pack_stack(vm, sr, top - 1);        /* pack table key */
		pack_stack(vm, sr, top);    /* pack table value */
		lua_pop(vm, 1); /* destroy value */
	}
//	TRACE("--- aft pack table ---\n");
//	lua::dump_stack(vm);
//	TRACE("---------------------\n");
	return NBR_OK;
}

int lua::coroutine::pack_function(VM vm, serializer &sr, int stkid) {
	ASSERT(stkid >= 0);
	TRACE("pack func: ofs=%d\n", sr.len());
	/* because lua_dump only dumps function on top of stack, so we copy function to top
	 * (stkid => top) */
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
//	TRACE("pack method?%s\n", lua_isfunction(vm, -1) ? "found" : "not found");
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
		verify_success(sr.push_string(lua_tostring(vm, -1)));
		verify_success(sr.push_raw(pbf.p(), pbf.last()));
		lua_pop(vm, 1);							//0
		return NBR_OK;
	}
	lua_pop(vm, 1);								//0
	return NBR_ENOTFOUND;
}

/* unpack */
int lua::coroutine::unpack_request_to_stack(const object &o, const fiber_context &c) {
	int r, al, top = lua_gettop(m_exec);
	if (!c.m_authorized) { r = NBR_ERIGHT; goto error; }
	ASSERT(o.is_request());
	al = o.alen();
	ASSERT(al > 0);
	lua_getglobal(m_exec, lua::ldname);
	/* pack method name */
	if ((r = unpack_stack(m_exec, o.arg(0))) < 0) { goto error; }
	/* pack caller fd */
	lua_pushinteger(m_exec, c.m_fd);
	if (lua_pcall(m_exec, 2, 1, 0) != 0) { goto error; }
	if (lua_isnil(m_exec, -1)) { r = NBR_ENOTFOUND; goto error; }
	ASSERT(lua_isfunction(m_exec, -1));
//		lua_gettable(m_exec, LUA_GLOBALSINDEX);	/* TODO: should we use environment index? */
	for (int i = 1; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.arg(i))) < 0) { goto error; }
	}
	return al - 1;
error:
	lua_settop(m_exec, top);
	lua_pushinteger(m_exec, r);
	return r;
}
int lua::coroutine::unpack_response_to_stack(const object &o) {
	int r, al, top = lua_gettop(m_exec);
	if (o.is_error()) {
		TRACE("resp:error!\n");
		if ((r = unpack_stack(m_exec, o.error())) < 0) { goto error; }
		lua::dump_stack(m_exec);
		/* lua_error(m_exec); *//* it causes crush on finderrfunc inside luajit.
		I think if we call lua_error from code where not called from lua VM,
		such an error happen. */
		/* so how can we propagate error to caller coroutine? */
		if (lua_isuserdata(m_exec, -1) || lua_istable(m_exec, -1)) {
			lua_getglobal(m_exec, lua::error_metatable);
			lua_setmetatable(m_exec, -2);
			return 1;
		}
		else {
			ASSERT(false);
			return 1;
		}
	}
	else {
		al = o.resp().size();
		for (int i = 0; i < al; i++) {
			if ((r = unpack_stack(m_exec, o.resp().elem(i))) < 0) { goto error; }
		}
		return al;
	}
error:
	lua_settop(m_exec, top);
	return r;
}

/* used by future */
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

/* used by timer */
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
	case rpc::datatype::BLOB: {
		//int l = o.len();
		/* last null character should be removed */
		//if (((const char *)(o))[l - 1] == '\0') { l--; }
		lua_pushlstring(vm, o, o.len());
	} break;
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
	if (lua_pcall(vm, 1, LUA_MULTRET, 0) != 0) {	//1
		TRACE("unpack userdata fails 1 (%s)\n", lua_tostring(vm, -1));
		goto error;
	}
	lua_getfield(vm, -1, lua::unpack_method);	//2
	if (lua_isfunction(vm, -1)) {
		TRACE("unpack function found\n");
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
static util::app g_app;
static server *g_server = NULL;
void output_logo(FILE *f) {
	fprintf(f, "__  ____ __ __    ____  \n");
	fprintf(f, "\\ \\ \\  // / \\ \\  / ___\\ \n");
	fprintf(f, " \\ \\/ / | | | | / /     \n");
	fprintf(f, "  \\  /  | | | | ~~~~~~~~    version %s(%s)\n", "0.3.2", LUAJIT_VERSION);
	fprintf(f, " _/ /   \\ \\_/ / \\ \\___  \n");
	fprintf(f, " \\_/     \\___/   \\____/  \n");
	fprintf(f, "it's brilliant on the cloud\n\n");
	fprintf(f, "(c)2011 - 2012 Takehiro Iyatomi(iyatomi@gmail.com)\n");

}
int luaopen_libyue(lua_State *vm) {
	if (g_vm) { ASSERT(false); return -1; }
	g_vm = vm;
	lua_error_check(vm, (g_server = new server), "fail to create server");
	lua_error_check(vm, util::init() >= 0, "fail to init (util)");
	lua_error_check(vm, (g_server->static_init(g_app, 1, 0, NULL) >= 0),
		"fail to init server (static)");
	lua_error_check(vm, (g_server->init(g_app) >= 0), "fail to init server");
	g_lib = &(g_server->fbr().lang());
	lua::ms_mode = lua::RPC_MODE_SYNC;
	ASSERT(g_server->fbr().served());
	output_logo(stdout);
	return 0;
}
void yue_poll() {
	g_server->poll();
}
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
}
lua_State *yue_getstate(yue_Fiber f) {
	_yue_Fiber *fb = reinterpret_cast<_yue_Fiber*>(f);
	return fb->m_t->rval().co()->vm();
}
void yue_deletefiber(yue_Fiber t) {
	ASSERT(t);
	delete reinterpret_cast<_yue_Fiber*>(t);
}
int yue_run(yue_Fiber t, int n_arg) {
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
int lua::static_init() {
	/* init accept watcher */
	handler::monitor::add_static_watcher(m_w);
	return utility::static_init();
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
	/* load package ffi */
	lua_pushcfunction(m_vm, luaopen_ffi);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}//*/
	/* load package string */
	lua_pushcfunction(m_vm, luaopen_string);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}//*/
	/* load package string */
	lua_pushcfunction(m_vm, luaopen_table);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}//*/

	/* create yue module */
	module::init(m_vm, attached()->served());

	/* add global tick function */
	util::functional<int (loop::timer_handle)> h(timer::tick);
	if (!module::served::set_timer(0.0f, 1.0f, h)) {
		return NBR_ESYSCALL;
	}

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

void lua::static_fin() {
	utility::static_fin();
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
	utility::fin();
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
		case LUA_TTABLE:	TRACE("table:%p", lua_topointer(vm, i)); break;
		case LUA_TFUNCTION: TRACE("function"); break;
		case LUA_TUSERDATA: TRACE("userdata:%p", lua_touserdata(vm, i)); break;
		case LUA_TTHREAD:	TRACE("thread"); break;
		case LUA_TLIGHTUSERDATA:	TRACE("%p", lua_touserdata(vm, i)); break;
		case 10:			TRACE("cdata?"); break;
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
		lua_pop(vm, 1);
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
		printf("table[%s]=", lua_tostring(vm, -2));
		switch(lua_type(vm, -1)) {
		case LUA_TNIL: 		printf("nil"); break;
		case LUA_TNUMBER:	printf("%lf", lua_tonumber(vm, -1)); break;
		case LUA_TBOOLEAN:	printf(lua_toboolean(vm, -1) ? "true" : "false"); break;
		case LUA_TSTRING:	printf("%s", lua_tostring(vm, -1));break;
		case LUA_TTABLE:	printf("table"); break;
		case LUA_TFUNCTION: printf("function"); break;
		case LUA_TUSERDATA: printf("userdata:%p", lua_touserdata(vm,-1)); break;
		case LUA_TTHREAD:	printf("thread"); break;
		case LUA_TLIGHTUSERDATA:	printf("%p", lua_touserdata(vm,-1)); break;
		case 10:			printf("cdata?"); break;
		default:
			//we never use it.
			ASSERT(false);
			return;
		}
		lua_pop(vm, 1);
		printf("\n");
	}
}

}
}
}
