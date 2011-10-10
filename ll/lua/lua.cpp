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
const char lua::kernel_table[] = "__kernel";
const char lua::index_method[] = "__index";
const char lua::newindex_method[] = "__newindex";
const char lua::call_method[] = "__call";
const char lua::gc_method[] = "__gc";
const char lua::pack_method[] = "__pack";
const char lua::unpack_method[] = "__unpack";
const char lua::codec_module_name[] = "__codec";
const char lua::rmnode_metatable[] = "__rmnode_mt";
const char lua::rmnode_sync_metatable[] = "__rmnode_s_mt";
const char lua::thread_metatable[] = "__thread_mt";
const char lua::future_metatable[] = "__future_mt";
const char lua::module_name[] = "yue";
bool lua::ms_sync_mode = false;



/* lua::method */
char lua::method::prefix_NOTIFICATION[] = "notify_";
char lua::method::prefix_CLIENT_CALL[] = "client_";
char lua::method::prefix_TRANSACTIONAL[] = "tr_";
char lua::method::prefix_QUORUM[] = "quorum_";

/* future */
/* hmm... future implementation is little bit complex :< */
struct future {
	lua::coroutine *m_co;
	yue::yielded_context m_y;
	U8 m_state;
	enum {
		INIT,
		SET_CALLBACK,
		RECV_RESPONSE,
		CALLBACKED,
		FINISH,
	};
	static int callback(lua::VM vm) {
		/* first operator () called already? (that is, already remote peer respond?) */
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		int top = lua_gettop(ft->m_co->vm());
		if (ft->m_state == RECV_RESPONSE) {
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
				ft->m_state = CALLBACKED;
			}
			else if (r != 0) {	/* finished but error */
				lua_pushstring(vm, lua_tostring(ft->m_co->vm(), -1));
				ft->fin(); lua_error(vm);
			}
			else {	/* successfully finished */
				ft->fin();
			}
		}
		else if (ft->m_state == INIT) {
			TRACE("setcallback: reset stack\n");
			ft->m_state = SET_CALLBACK;
			lua_settop(ft->m_co->vm(), 0);	/* reset stack (because sent arg still on stack) */
			lua_xmove(vm, ft->m_co->vm(), 1);
		}
		else {
			lua_pushfstring(vm, "invalid state: %u", ft->m_state);
			ft->fin(); lua_error(vm);
		}
		return 0;
	}
	static future *init(lua::VM vm, lua &ll) {
		int top = lua_gettop(vm);
		future *ft = reinterpret_cast<future *>(
			lua_newuserdata(vm, sizeof(future))
		);
		lua_getglobal(vm, lua::future_metatable);
		lua_setmetatable(vm, -2);

		/* create coroutine to execute callback */
		fiber::handler h(*ft);
		ft->m_y.set(h);
		ft->m_state = INIT;
		lua_error_check(vm, (ft->m_co = ll.create(&(ft->m_y))), "create coroutine");

		/* prevent top object from GC-ing */
		ft->m_co->refer();

		/* future shift to bottom of stack
			(others copy into coroutine which created by future as above) */
		lua_insert(vm, 1);

		/* copy rpc args to ft->m_co (and it removes after
		 * 1. future.callback is called
		 * 2. rpc response received) */
		lua_xmove(vm, ft->m_co->vm(), top);
		return ft;
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
				m_state = RECV_RESPONSE;
				return fiber::exec_yield;
			case SET_CALLBACK:
				lua::dump_stack(m_co->vm());
				ASSERT(lua_isfunction(m_co->vm(), 1));
				lua_pushboolean(m_co->vm(), !o.is_error());
				if ((r = m_co->unpack_stack(o)) < 0) {
					ASSERT(false); return fiber::exec_error;
				}
				m_state = CALLBACKED;
				return m_co->resume(r + 1);/* case 2) */
			case CALLBACKED:
				return m_co->resume(o);
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
		m_state = FINISH;
		m_co->unref();
		if (m_co) { m_co->ll().destroy(m_co); }
	}
};


int lua::method::init(VM vm, actor *a, const char *name) {
	method *m = reinterpret_cast<method *>(
		lua_newuserdata(vm, sizeof(method))
	);
	m->m_name = parse(name, m->m_attr); /* name is referred by VM so never freed */
	m->m_a = a;
	/* meta table */
	switch(a->kind()) {
	case actor::THREAD:
		lua_getglobal(vm, thread_metatable); break;
	case actor::RMNODE:
		if (lua::ms_sync_mode) {
			lua_getglobal(vm, rmnode_sync_metatable);
		}
		else {
			lua_getglobal(vm, rmnode_metatable);
		} break;
	}
	lua_setmetatable(vm, -2);
	return 1;
}
const char *lua::method::parse(const char *name, U32 &attr) {
#define PARSE(__r, __pfx, __attr) { 											\
	if (util::mem::cmp(__r, prefix_##__pfx, sizeof(prefix_##__pfx) - 1) == 0) { \
		__attr |= __pfx; 														\
		__r += (sizeof(prefix_##__pfx) - 1); 									\
		continue;																\
	} 																			\
}
	attr = 0;
	const char *r = name;
	while(true) {
		PARSE(r, NOTIFICATION, attr);
		PARSE(r, CLIENT_CALL, attr);
		PARSE(r, TRANSACTIONAL, attr);
		PARSE(r, QUORUM, attr);
		break;
	}
	TRACE("parse:%s>%s:%08x\n", name, r, attr);
	return r;
}
template <>
int lua::method::call<session>(VM vm) {
	/* a1, a2, ..., aN */
	coroutine *co = coroutine::to_co(vm);
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	TRACE("call<session> %p %p\n", co, m);
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	session *s = m->m_a->m_s;
	if (m->notification()) {
		/* TODO: remove this code */
		if (!s->valid()) {
			lua_error_check(vm,
				(s->sync_connect(fabric::tlf().tla()) >= 0), "session::connect");
		}
		/* return future instead of yield & wait for reply */
		future *ft = future::init(vm, co->ll());
		co = ft->m_co;
	}
	if (s->valid()) {
		PROCEDURE(callproc)::args_and_cb<yielded_context> a(*(co->yldc()));
		a.m_co = co;
		lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*s, a), "callproc");
	}
	else {
		session::watcher sw(*co);
		lua_error_check(vm, (s->reconnect(sw) >= 0), "reconnect");
	}
	lua::dump_stack(vm);
	return m->notification() ? 1 : co->yield();
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
	return r;
}
template <>
int lua::method::call<local_actor>(VM vm) {
	coroutine *co = coroutine::to_co(vm);
	method *m = reinterpret_cast<method *>(lua_touserdata(vm, 1));
	lua_error_check(vm, co && m, "to_co or method %p %p", co, m);
	if (m->notification()) {
		future *ft = future::init(vm, co->ll());
		co = ft->m_co;
	}
	PROCEDURE(callproc)::args_and_cb<yielded_context> a(*(co->yldc()));
	a.m_co = co;
	lua_error_check(vm, INVALID_MSGID != yue::rpc::call(*(m->m_a->m_la), a), "callproc");
	return co->yield();
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
	/* API 'sync_mode' */
	lua_pushcfunction(vm, sync_mode);
	lua_setfield(vm, -2, "sync_mode");
	/* API 'newthread' */
	lua_pushcfunction(vm, newthread);
	lua_setfield(vm, -2, "newthread");
	/* API 'resume' */
	lua_pushcfunction(vm, resume);
	lua_setfield(vm, -2, "resume");
	/* API 'yield' */
	lua_pushcfunction(vm, yield);
	lua_setfield(vm, -2, "yield");
	/* API 'listen' */
	lua_pushcfunction(vm, listen);
	lua_setfield(vm, -2, "listen");
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
	lua_pushcfunction(vm, future::callback);
	lua_setfield(vm, -2, "callback");
	lua_pushvalue(vm, -1);
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
			lua_error_check(vm, (k && (s = m->served()->spool().add_to_mesh(k))),
				"add_to_mesh");
			actor::init(vm, s);
			lua_pushvalue(vm, -1);	/* dup actor */
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
			lua_error_check(vm, (la = m->served()->get_thread(k)),
				"get_thread");
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
	lua_error_check(vm, (lua_isstring(vm, -1)), "type error");
	session *s = served()->spool().open(lua_tostring(vm, -1));
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
	lua_State *co = yue_newthread(NULL);
	if (co) {
		lua_xmove(vm, co, 1);	/* copy called function */
		lua_pushthread(co);
		lua_xmove(co, vm, 1);	/* copy back coroutine object */
	}
	else {
		lua_pushnil(vm);
	}
	return 1;
}
int lua::module::resume(VM vm) {
	return yue_resume(vm);
}
int lua::module::yield(VM vm) {
	lua::coroutine *co = lua::coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	return co->yield();
}
int lua::module::sync_mode(VM vm) {
	lua_error_check(vm, (lua_isboolean(vm, -1)), "type error");
	ms_sync_mode = lua_toboolean(vm, -1);
	return 0;
}
int lua::module::listen(VM vm) {
	lua_error_check(vm, (lua_isstring(vm, -1)), "type error");
	served()->listen(lua_tostring(vm, -1));
	return 0;
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
		/* register thread object to registry so that never collected by GC. */
		lua_pushvalue(m_exec, -1);
		lua_pushlightuserdata(m_exec, this);
		lua_settable(m_exec, LUA_REGISTRYINDEX);
		/* add this pointer to registry so that can find this ptr(this)
		 * from m_exec faster */
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
	else {
		lua_error_check(m_exec, false, "state = %u\n", state);
	}
	return false;
}

int lua::coroutine::resume(int r) {
	/* TODO: when exec_error we should reset coroutine state so that
	 * it runs correctly when is reused for next execution but how? */
	lua::dump_stack(m_exec);
	if ((r = lua_resume(m_exec, r)) == LUA_YIELD) {
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

int lua::coroutine::pack_stack(VM vm, serializer &sr, int stkid) {
	int r;
	ASSERT(stkid >= 0);
	TRACE("pack_stack type = %u\n", lua_type(vm, stkid));
	switch(lua_type(vm, stkid)) {
	case LUA_TNIL: 		sr.pushnil(); break;
	case LUA_TNUMBER:	sr << lua_tonumber(vm, stkid); break;
	case LUA_TBOOLEAN:	sr << (lua_toboolean(vm, stkid) ? true : false); break;
	case LUA_TSTRING:	sr.push_string(lua_tostring(vm, stkid), lua_objlen(vm, stkid));
		break;
	case LUA_TTABLE: { /* = map {...} */
		int tblsz = 0;
		r = lua_gettop(vm);     /* preserve current stack size to r */
		TRACE("nowtop=%d\n", r);
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
		/* it should be back to original stack height */
		if (r != lua_gettop(vm)) {
			ASSERT(false);
			lua_settop(vm, r);      /* recover stack size */
		}
		} break;
	case LUA_TFUNCTION:     /* = array ( LUA_TFUNCTION, binary_chunk ) */
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		stkid is popped. so we can assure target function to dump
		is already on the top of stack here. */
		if (pack_function(vm, sr, stkid) < 0) { return NBR_ELENGTH; }
		break;
	case LUA_TUSERDATA: {
		sr.push_array_len(3);
		sr << ((U8)LUA_TUSERDATA);
		if (pack_userdata(vm, sr, stkid) < 0) { return NBR_EFORMAT; }
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:
	default:
		//we never use it.
		ASSERT(false);
		return NBR_EINVAL;
	}
	return sr.len();
}

int lua::coroutine::pack_function(VM vm, serializer &sr, int stkid) {
	ASSERT(stkid >= 0);
	TRACE("pack func: ofs=%d\n", sr.len());
	lua_pushvalue(vm, stkid);
	pbuf pbf;
	int r = lua_dump(vm, lua::writer::callback, &pbf);
	lua_pop(vm, 1);
	if (r != 0) { return NBR_EINVAL; }
	verify_success(sr.push_array_len(2));
	verify_success(sr << ((U8)LUA_TFUNCTION));
	verify_success(sr.push_raw(pbf.p(), pbf.last()));
	TRACE("pack func: after ofs=%d\n", sr.len());
	return NBR_OK;
}

int lua::coroutine::pack_userdata(VM vm, serializer &sr, int stkid) {
	ASSERT(stkid >= 0);
	const char *module;
	int orgtop = lua_gettop(vm), r = NBR_ENOTFOUND;
	lua_getglobal(vm, "require");	//1
	if (lua_isnil(vm, -1)) {
		goto error;
	}
	lua_getfield(vm, stkid, lua::codec_module_name);	//2
	if ((module = lua_tostring(vm, -1))) {
		if (lua_pcall(vm, 1, 1, 0) != 0) {		//1
			TRACE("pack userdata fails (%s)\n", lua_tostring(vm, -1));
			r = NBR_ECBFAIL; goto error;
		}
		lua_getfield(vm, stkid, lua::pack_method);	//2
		if (lua_isfunction(vm, -1)) {
			lua_pushvalue(vm, stkid);				//3
			pbuf pbf;
			lua_pushlightuserdata(vm, &pbf);		//4
			if (lua_pcall(vm, 2, 0, 0) != 0) {		//1
				TRACE("pack userdata fails (%s)\n", lua_tostring(vm, -1));
				r = NBR_ECBFAIL; goto error;
			}
			verify_success(sr.push_array_len(3));
			verify_success(sr << ((U8)LUA_TUSERDATA));
			verify_success(sr << module);
			verify_success(sr.push_raw(pbf.p(), pbf.last()));
			lua_settop(vm, orgtop);
			return NBR_OK;
		}
	}
error:
	lua_settop(vm, orgtop);
	sr.pushnil();
	return r;
}

/* unpack */
int lua::coroutine::unpack_stack(const object &o) {
	int r, al, top = lua_gettop(m_exec);
	if (o.is_error()) {
		if ((r = unpack_stack(m_exec, o.error())) < 0) { goto error; }
		lua_error(m_exec);
		return NBR_ESYSCALL;
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
		lua_gettable(m_exec, LUA_GLOBALSINDEX);
		for (int i = 1; i < al; i++) {
			if ((r = unpack_stack(m_exec, o.arg(i))) < 0) { goto error; }
		}
		return al - 1;
	}
error:
	lua_settop(m_exec, top);
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
			/* [LUA_TUSERDATA, packed object:blob] */
			if ((r = unpack_userdata(vm, o.elem(1))) < 0) { return r; }
			break;
		default:/* error object */
			if (((int)(o.elem(0))) < 0) {
				lua_createtable(vm, o.size(), 0);
				for (i = 0; i < o.size(); i++) {
					lua_pushnumber(vm, (i + 1));
					unpack_stack(vm, o.val(i));
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

int lua::coroutine::unpack_userdata(VM vm, const argument &d) {
	int orgtop = lua_gettop(vm);
	lua_getglobal(vm, "require");	//1
	if (lua_isnil(vm, -1)) { goto error; }
	lua_getglobal(vm, d.elem(1));	//2
	if (lua_isnil(vm, -1)) { goto error; }
	if (lua_pcall(vm, 1, 1, 0) != 0) {	//1
		TRACE("unpack userdata fails (%s)\n", lua_tostring(vm, -1));
		goto error;
	}
	lua_getfield(vm, -1, lua::unpack_method);	//2
	if (lua_isfunction(vm, -1)) {
		lua_pushlightuserdata(vm, const_cast<void *>(
			reinterpret_cast<const void *>(&d)
		));				//3
		if (lua_pcall(vm, 1, 1, 0) != 0) {			//2
			TRACE("unpack userdata fails (%s)\n", lua_tostring(vm, -1));
			goto error;
		}
		lua_remove(vm, 1);	/* remove module table */	//1
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
	lua::ms_sync_mode = true;
	ASSERT(fabric::served());
	output_logo(stdout);
	return 0;
}
void yue_poll() {
	lua::module::served()->poll();
}
struct yue_libhandler {
	lua::coroutine *m_co;
	void (*m_cb)(lua_State *, bool);
	yielded_context m_ctx;
	static void nop(lua_State *, bool) {}
	inline yue_libhandler(void (*cb)(lua_State *, bool)) :
		m_cb(cb), m_ctx(fiber::handler(*this)) {}
	int operator () (fabric &f, object &o) {
		int r;
		if (o.is_error()) {
			m_cb(m_co->vm(), false);
			goto finalize;
		}
		switch((r = m_co->resume(o))) {
		case fiber::exec_error: case fiber::exec_finish:
			/* procedure finish or error happen (it should reply to caller actor) */
			m_cb(m_co->vm(), r != fiber::exec_error);
			break;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
			return NBR_OK;
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			return NBR_OK;
		default:
			ASSERT(false);
		}
	finalize:
		if (m_co) {
			if (r == fiber::exec_error) { m_co->fin(); }
			m_co->free();
		}
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
lua_State *yue_newthread(void (*cb)(lua_State *, bool)) {
	yue_libhandler *yh = new yue_libhandler(cb ? cb : yue_libhandler::nop);
	if (!yh) { ASSERT(false); return NULL; }
	if ((yh->m_co = g_lib->create(&(yh->m_ctx)))) {
		return yh->m_co->vm();
	}
	delete yh; return NULL;
}
int yue_resume(lua_State *vm) {
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
	*sz = a->elem(2).len();
	return a->elem(2).operator const void *();
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
	}
	/* load package library */
	lua_pushcfunction(m_vm, luaopen_package);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}

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
