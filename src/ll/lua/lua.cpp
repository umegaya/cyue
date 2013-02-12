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
#include "exlib/luajit/src/luajit.h"

#define lua_raise_error(vm, cond, error, ...)	if (!(cond)) {		\
	char __b[256]; snprintf(__b, sizeof(__b), __VA_ARGS__);			\
	lua_pushfstring(vm, "%s@%s(%d):%s", error, __FILE__, __LINE__, __b);	\
	lua_error(vm)													\
}
#define lua_return_error(vm, cond, error, ...)	if (!(cond)) {		\
	char __b[256]; snprintf(__b, sizeof(__b), __VA_ARGS__);			\
	lua_pushboolean(vm, false);										\
	lua_pushfstring(vm, "%s@%s(%d):%s", error, __FILE__, __LINE__, __b);	\
	return 2;														\
}


namespace yue {
namespace module {
namespace ll {

#include "misc.h"

/* lua::static variables */
const char lua::namespaces[] = "namespaces";
const char lua::objects[] = "objects";
const char lua::index_method[] = "__index";
const char lua::newindex_method[] = "__newindex";
const char lua::call_method[] = "__call";
const char lua::gc_method[] = "__gc";
const char lua::pack_method[] = "__pack";
const char lua::unpack_method[] = "__unpack";
const char lua::finalizer[] = "__finalizer";
const char lua::namespace_symbol[] = "__sym";

const char lua::coroutine::symbol_tick[] = "__tick";
const char lua::coroutine::symbol_signal[] = "__signal";
const char lua::coroutine::symbol_join[] = "__join";
const char lua::coroutine::symbol_accept[] = "__accept";
const char *lua::coroutine::symbol_socket[] = {
		NULL, 			//socket::HANDSHAKE,
		"__open",		//socket::WAITACCEPT,
		"__establish",	//socket::ESTABLISH,
		"__data", 		//socket::RECVDATA,
		"__close",		//socket::CLOSED,
	};

const char lua::bootstrap_source[] =
	/* boot strap lua source */
	"local yue = require('yue')\n"
	"local ok,r = pcall("
		"yue.args.boot and loadfile(yue.args.boot) or\n"
		"loadstring(\n"
			"[[local yue = require('yue')\n"
			"assert(yue.args.launch, 'launch or boot script must be specified')\n"
			"for i=1,yue.args.wc,1 do yue.thread('worker' .. i, yue.args.launch, 'main', yue.args.launch_timeout) end]]\n"
		", 'bootstrap')\n"
	")\n"
	"if not ok then print(r) end";

/* yue C API for lua */
static struct module {
	lua_State *m_vm;
	util::app m_app;
	server *m_server;
	util::thread m_thrd;
	server::thread m_main;
	module() : m_vm(NULL), m_app(true), m_server(NULL) {}
	inline bool initialized() { return m_vm; }
	inline void poll() { if (m_server) { m_server->poll(); } }
	inline void init(lua_State *vm) {
		lua_error_check(vm, !initialized(), "already initialized");
		m_vm = vm;
		int argc = 0;
		char *argv[] = { NULL };
		lua_error_check(vm, m_app.init<server>(argc, argv) >= 0, "fail to initialize app");
		lua_error_check(vm, util::thread::static_init(&m_thrd) >= 0, "fail to initialize thread");
		lua_error_check(vm, (m_server = new server), "fail to create server");
		lua_error_check(vm, util::init() >= 0, "fail to init (util)");
		m_main.set("libyue", "", 1, loop::USE_MAIN_EVENT_LOOP);
		server::launch_args args = { &m_main };
		lua_error_check(vm, (m_server->init(args) >= 0), "fail to init server");
		lua_error_check(vm, m_server->fbr().served(), "invalid state");
	}
	inline void fin() {
		m_app.die();
		m_app.join();
		if (m_server) {
			m_server->fin();
			util::fin();
			delete m_server;
			m_server = NULL;
		}
		m_app.fin<server>();
	}
} g_module;
extern "C" {
int luaopen_libyue(lua_State *vm) {
	g_module.init(vm);
	return 0;
}
void yue_poll() {
	g_module.poll();
}
yue_Fiber yue_newfiber(yue_FiberCallback cb) {
	fiber *fb = NULL;
	rpc::endpoint::callback endp(cb);
	if (!(fb = fabric::tlf().create(endp))) { goto error; }
	return reinterpret_cast<yue_Fiber>(fb);
error:
	ASSERT(false);
	if (fb) {
		fb->fin();
	}
	return NULL;
}
lua_State *yue_getstate(yue_Fiber f) {
	fiber *fb = reinterpret_cast<fiber*>(f);
	return fb->co()->vm();
}
void yue_deletefiber(yue_Fiber t) {
	ASSERT(t);
	reinterpret_cast<fiber*>(t)->fin();
}
int yue_run(yue_Fiber t, int n_arg) {
	if (!t) { ASSERT(false); return LUA_ERRERR; }
	fiber *fb = reinterpret_cast<fiber*>(t);
	return fb->resume(n_arg);
}
void yue_fin() {
	if (g_module.initialized()) {
		g_module.fin();
	}
}
bool yue_load_as_module() {
	return g_module.initialized();
}
int yueb_write(yue_Wbuf yb, const void *p, int sz) {
	util::pbuf *pbf = reinterpret_cast<util::pbuf *>(yb);
	if (pbf->reserve(sz) < 0) { return NBR_EMALLOC; }
	util::mem::copy(pbf->last_p(), p, sz);
	pbf->commit(sz);
	return pbf->last();
}
const void *yueb_read(yue_Rbuf yb, int *sz) {
	argument *a = reinterpret_cast<argument *>(yb);
	*sz = a->len();
	return a->operator const void *();
}
}

/******************************************************************************************/
/* class lua */
/* lua::coroutine */
int lua::coroutine::init(lua *l) {
	/* CAUTION: this based on the util::array implementation 
	initially set all memory zero and never re-initialize when reallocation done. */
	bool first = !m_exec;
	if (first && !(m_exec = lua_newcthread(l->vm(), 0))) {
		return NBR_EEXPIRE;
	}
	lua_pushthread(m_exec);
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
	ASSERT(lua_gettop(m_exec) == 0);
	return NBR_OK;
}

void lua::coroutine::free() {
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

void lua::coroutine::fin_with_context(int result) {
	/* may call from connect handler (because it not called from fabric) */
	fb()->respond(result);
	fin();
}

int lua::coroutine::resume(int r) {
	/* TODO: when exec_error we should reset coroutine state.
	 * without reset coroutine, it does not run correctly when reused. but how?
	 * (now we call coroutine::fin() and destroy coroutine object. but its not efficient way) */
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
/* before calling this function, this->m_exec's stack layout is like following:
 * [method name],[arg1],[arg2],...[argN]
 *  */
int lua::coroutine::pack_stack_as_rpc_args(serializer &sr, int start_index) {
	int top = lua_gettop(m_exec), r;
	if (top < start_index) {
		/* top < start_index means this function not returns any value */
		sr.push_array_len(0); return sr.len();
	}
	if ((r = pack_stack(m_exec, sr, start_index)) < 0) { return r; }
	sr.push_array_len(top - start_index);
	for (int i = (start_index + 1); i <= top; i++) {
		if ((r = pack_stack(m_exec, sr, i)) < 0) { return r; }
	}
	/* shrink stack */
	lua_settop(m_exec, 1);
	return sr.len();
}

int lua::coroutine::pack_stack_as_response(VM vm, int start_index, serializer &sr) {
	int top = lua_gettop(vm), r;
	lua::dump_stack(vm);
	if (start_index > top) {
		/* start_id > top means this function not returns any value */
		sr.push_array_len(0); return sr.len();
	}
	sr.push_array_len(top + 1 - start_index);
	for (int i = start_index; i <= top; i++) {
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
	TRACE("pack_stack type = %u\n", lua_type(vm, stkid));
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
int lua::coroutine::unpack_response_to_stack(const object &o) {
	int r, al, top = lua_gettop(m_exec);
	if (o.is_error()) {
		TRACE("resp:error!\n");
		lua_pushboolean(m_exec, false);	//indicate error
		if ((r = unpack_stack(m_exec, o.error())) < 0) { goto error; }
		lua::dump_stack(m_exec);
		return 2;
	}
	else {
		TRACE("resp:success!\n");
		lua_pushboolean(m_exec, true);	//indicate success
		al = o.resp().size();
		for (int i = 0; i < al; i++) {
			if ((r = unpack_stack(m_exec, o.resp().elem(i))) < 0) { goto error; }
		}
		lua::dump_stack(m_exec);
		return al + 1;
	}
error:
	lua_settop(m_exec, top);
	return r;
}

int lua::coroutine::unpack_stack(VM vm, const argument &o) {
	size_t i; int r;
	TRACE("pack_stack kind = %u\n", o.kind());
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
const char *lua::version() {
	return LUAJIT_VERSION;
}
int lua::static_init() {
	return NBR_OK;
}
int lua::init(const util::app &a, server *sv)
{
	int r, module_mode = 0;
	/* basic lua initialization. */
	/* for lua-module mode and lua module initialized by main vm, use main vm as lua module's main vm */
	if (sv && sv == g_module.m_server) {
		if (!(m_vm = g_module.m_vm)) {
			return NBR_ESYSCALL;
		}
		module_mode = 1;
	}
	/* otherwise create own VM (server mode or new thread in lua-module mode. */
	else if (!(m_vm = lua_newvm(allocator, this))) {
		return NBR_ESYSCALL;
	}
	lua_settop(m_vm, 0);
	/* set panic callback */
	lua_atpanic(m_vm, panic);
	lua_CFunction libs[] = {
		luaopen_base,
		luaopen_package,
		luaopen_ffi,
		luaopen_jit,
		luaopen_bit,
		luaopen_io,
		luaopen_string,
		luaopen_table,
		luaopen_debug,
	};
	if (!module_mode) {
		for (int i = 0; i < (int)countof(libs); i++) {
			lua_pushcfunction(m_vm, libs[i]);
			if (0 != lua_pcall(m_vm, 0, 0, 0)) {
				TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
				return NBR_ESYSCALL;
			}//*/
		}
	}
	/* load package.loaded table */
	lua_getglobal(m_vm, "package");
	lua_getfield(m_vm, -1, "loaded");
	/* init yue module table */
	lua_newtable(m_vm);
	/* put command line args to yue.args */
	lua_createtable(m_vm, a.argc(), 0);
	for (int i = 1/* ignore first argument (executable name) */; i < a.argc(); i++) {
		lua_pushinteger(m_vm, i);
		lua_pushstring(m_vm, a.argv()[i]);
		lua_settable(m_vm, -3);
	}
	lua_setfield(m_vm, -2, "args");
	/* put link to lua_State ptr */
	lua_pushlightuserdata(m_vm, m_vm);
	lua_setfield(m_vm, -2, "state");
	/* init object map */
	if ((r = init_objects_map(m_vm)) < 0) { return r; }
	/* init emittable objects */
	if ((r = init_emittable_objects(m_vm, sv)) < 0) { return r; }
	/* init C constant */
	if ((r = init_constants(m_vm)) < 0) { return r; }
	/* init misc */
	misc::init(m_vm);
	lua_setfield(m_vm, -2, "util");
	/* run mode */
#if defined(_DEBUG)
	lua_pushstring(m_vm, "debug");
#else
	lua_pushstring(m_vm, "release");
#endif
	lua_setfield(m_vm, -2, "mode");
	lua_pushcfunction(m_vm, poll);
	/* init client yue API */
	lua_setfield(m_vm, -2, "yue_poll");
	lua_pushcfunction(m_vm, alive);
	lua_setfield(m_vm, -2, "yue_alive");
	lua_pushcfunction(m_vm, die);
	lua_setfield(m_vm, -2, "yue_die");
	/* create finalizer  */
	lua_newuserdata(m_vm, sizeof(void *));
	lua_newtable(m_vm);
	lua_pushcfunction(m_vm, finalize);
	lua_setfield(m_vm, -2, "__gc");
	lua_setmetatable(m_vm, -2);
	lua_setfield(m_vm, -2, "fzr");
	/* put yue module to package.loaded.libyue */
	lua_setfield(m_vm, -2, "libyue");
	return NBR_OK;
}

int lua::init_objects_map(VM vm) {
	/* yue.objects */
	lua_createtable(vm, loop::maxfd(), 0);
	lua_pushvalue(vm, -1);
	lua_setfield(vm, LUA_REGISTRYINDEX, lua::objects);
	lua_setfield(vm, -2, lua::objects);
	/* yue.namespaces */
	lua_createtable(vm, loop::maxfd(), 0);
	lua_pushvalue(vm, -1);
	lua_setfield(vm, LUA_REGISTRYINDEX, lua::namespaces);
	lua_setfield(vm, -2, lua::namespaces);
	return NBR_OK;
}

#if defined(__NBR_OSX__) || defined(__NBR_IOS__)
#define USE_BSD_SIGNAL_LIST
#endif
    
int lua::init_constants(VM vm) {
	/* constant table */
	lua_newtable(m_vm);
	#define ADD_SIGNAL_CONST(name)	lua_pushinteger(m_vm, name); lua_setfield(m_vm, -2, #name)
	ADD_SIGNAL_CONST(SIGHUP);
	ADD_SIGNAL_CONST(SIGINT);
	ADD_SIGNAL_CONST(SIGQUIT);
	ADD_SIGNAL_CONST(SIGILL);
	ADD_SIGNAL_CONST(SIGTRAP);
	ADD_SIGNAL_CONST(SIGABRT);
	ADD_SIGNAL_CONST(SIGIOT);
	ADD_SIGNAL_CONST(SIGBUS);
	ADD_SIGNAL_CONST(SIGFPE);
	ADD_SIGNAL_CONST(SIGKILL);
	ADD_SIGNAL_CONST(SIGUSR1);
	ADD_SIGNAL_CONST(SIGSEGV);
	ADD_SIGNAL_CONST(SIGUSR2);
	ADD_SIGNAL_CONST(SIGPIPE);
	ADD_SIGNAL_CONST(SIGALRM);
	ADD_SIGNAL_CONST(SIGTERM);
#if defined(USE_BSD_SIGNAL_LIST)
	ADD_SIGNAL_CONST(SIGURG);
#else
	ADD_SIGNAL_CONST(SIGSTKFLT);
#endif
	ADD_SIGNAL_CONST(SIGCHLD);
	ADD_SIGNAL_CONST(SIGCONT);
	ADD_SIGNAL_CONST(SIGSTOP);
	ADD_SIGNAL_CONST(SIGTSTP);
	ADD_SIGNAL_CONST(SIGTTIN);
	ADD_SIGNAL_CONST(SIGTTOU);
	ADD_SIGNAL_CONST(SIGURG);
	ADD_SIGNAL_CONST(SIGXCPU);
	ADD_SIGNAL_CONST(SIGXFSZ);
	ADD_SIGNAL_CONST(SIGVTALRM);
	ADD_SIGNAL_CONST(SIGPROF);
	ADD_SIGNAL_CONST(SIGWINCH);
	ADD_SIGNAL_CONST(SIGIO);
#if !defined(USE_BSD_SIGNAL_LIST)
	ADD_SIGNAL_CONST(SIGPOLL);
	ADD_SIGNAL_CONST(SIGPWR);
#endif
	ADD_SIGNAL_CONST(SIGSYS);
#if !defined(USE_BSD_SIGNAL_LIST)
	ADD_SIGNAL_CONST(SIGUNUSED);
	ADD_SIGNAL_CONST(SIGRTMIN);
#endif
	lua_newtable(m_vm);
#if defined(DEFINE_ERROR)
#undef DEFINE_ERROR
#endif
#define DEFINE_ERROR(__error, __code) {	\
	lua_pushinteger(m_vm, __code);		\
	lua_setfield(m_vm, -2, #__error);	\
}
#include "../../rpcerrors.inc"
#undef DEFINE_ERROR
	lua_setfield(m_vm, -2, "rpcerrors");
	lua_setfield(m_vm, -2, "const");

	/* feature constant (to know which spec are enable */
	lua_newtable(m_vm);
	lua_pushstring(m_vm,
#if defined(__ENABLE_TIMER_FD__)
		"timerfd"
#else
#if defined(USE_KQUEUE_TIMER)
		"kqueue"
#else
		"timer"
#endif
#endif
	);
	lua_setfield(m_vm, -2, "timer");
	lua_setfield(m_vm, -2, "feature");
	return NBR_OK;
}

#include "emitter/base.hpp"
#include "emitter/signal.hpp"
#include "emitter/timer.hpp"
#include "emitter/socket.hpp"
#include "emitter/listener.hpp"
#include "emitter/fs.hpp"
#include "emitter/thread.hpp"
#include "emitter/peer.hpp"
#include "emitter/fiber.hpp"

int lua::init_emittable_objects(VM vm, server *sv) {
	emitter::base::init(vm);
	emitter::signal::init(vm);
	emitter::timer::init(vm);
	emitter::socket::init(vm);
	emitter::listener::init(vm);
	emitter::fs::init(vm);
	emitter::thread::init(vm);
	emitter::peer::init(vm);
	emitter::fiber::init(vm);
	if (sv) {
		lua_pushlightuserdata(vm, sv->thrd());
	}
	else {
		lua_pushnil(vm);
	}
	lua_setfield(vm, -2, "thread");
	lua_pushcfunction(vm, peer);
	lua_setfield(vm, -2, "yue_peer");
	return NBR_OK;
}
int lua::peer(VM vm) {
	coroutine *co = coroutine::to_co(vm);
	lua_error_check(vm, co, "to_co");
	switch (co->fb()->endp().type()) {
	case rpc::endpoint::type_datagram:
		emitter::peer::create(vm, 
			co->fb()->endp().datagram_ref().s(), 
			co->fb()->endp().datagram_ref().addr());
		lua_pushlightuserdata(vm, co->fb()->endp().datagram_ref().s());
		lua_pushstring(vm, "datagram");
		return 3;
	case rpc::endpoint::type_stream:
		lua_pushlightuserdata(vm, co->fb()->endp().stream_ref().s());
		lua_pushvalue(vm, -1);
		lua_pushstring(vm, "stream");
		return 3;
	case rpc::endpoint::type_local:
		lua_pushlightuserdata(vm, co->fb()->endp().local_ref().svr()->thrd());
		lua_pushvalue(vm, -1);
		lua_pushstring(vm, "thread");
		return 3;
	case rpc::endpoint::type_nop:
	case rpc::endpoint::type_callback:
	default:
		lua_pushnil(vm);
		return 1;
	}
}
int lua::poll(VM vm) {
	yue_poll();
	return 0;
}
int lua::alive(VM vm) {
	lua_pushboolean(vm, loop::app().alive());
	return 1;
}
int lua::die(VM vm) {
	loop::app().die();
	return 0;
}
int lua::finalize(VM vm) {
	yue_fin();
	return 0;
}

/* receive code or filename and if store_result == NULL, execute it 
	if store_result not NULL, then xmove loaded function to it.
*/
int lua::eval(const char *ptr, coroutine *store_result)
{
	int (*loader)(VM, const char *) = NULL;
	const char *assume;
	FILE *fp = fopen(ptr, "r");
	if (fp) {
		fclose(fp);
		loader = luaL_loadfile;
		assume = "Lua file";
	}
	else {
		loader = luaL_loadstring;
		assume = "Lua code";
	}
	/* first assume ptr is filename, try to open. */
	if (loader(m_vm, ptr) != 0) {	/* 1:srcfile func */
		TRACE("assume <%s> is %s : error <%s>\n", ptr, assume, lua_tostring(m_vm, -1));
		ASSERT(false);
		return NBR_ESYSCALL;
	}
	if (store_result) {
		lua_xmove(m_vm, store_result->vm(), 1);
	}
	else {
		//2012/10/02 iyatomi from here, valgrind report memory leak in _dlerror_run but it seems to be bug?
		//(http://stackoverflow.com/questions/1542457/memory-leak-reported-by-valgrind-in-dlopen)
		//so initially I ignore this memory leak (there is not so much eval used,
		// so it never cause practical problem)
		if (lua_pcall(m_vm, 0, 0, 0) != 0) {            /* 1->removed */
			TRACE("lua_pcall : error <%s>\n", lua_tostring(m_vm, -1));
			ASSERT(false);
			return NBR_ESYSCALL;
		}
	}
	return NBR_OK;
}

void lua::static_fin() {
	misc::static_fin();
}
void lua::fin()
{
	if (m_vm) {
	TRACE("call finalizer %p\n", m_vm);
		lua_getglobal(m_vm, "package");
		lua_getfield(m_vm, -1, "loaded");
		lua_getfield(m_vm, -1, "libyue");
		lua_getfield(m_vm, -1, finalizer);
		lua_pcall(m_vm, 0, 0, 0);
		if (!yue_load_as_module()) {
	TRACE("lua_close %p\n", m_vm);
			lua_close(m_vm);
		}
		m_vm = NULL;
	}
	misc::fin();
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
		printf("table[%u:", lua_type(vm, -2));
		switch(lua_type(vm, -2)) {
		case LUA_TNIL: 		printf("nil"); break;
		case LUA_TNUMBER:	printf("%lf", lua_tonumber(vm, -2)); break;
		case LUA_TBOOLEAN:	printf(lua_toboolean(vm, -2) ? "true" : "false"); break;
		case LUA_TSTRING:	printf("%s", lua_tostring(vm, -2));break;
		case LUA_TTABLE:	printf("table:%p", lua_topointer(vm, -2)); break;
		case LUA_TFUNCTION: printf("function:%p", lua_topointer(vm, -2)); break;
		case LUA_TUSERDATA: printf("userdata:%p", lua_touserdata(vm,-2)); break;
		case LUA_TTHREAD:	printf("thread"); break;
		case LUA_TLIGHTUSERDATA:	printf("%p", lua_touserdata(vm,-2)); break;
		case 10:			printf("cdata?"); break;
		default:
			//we never use it.
			ASSERT(false);
			return;
		}
		printf("]=%u:", lua_type(vm, -1));
		switch(lua_type(vm, -1)) {
		case LUA_TNIL: 		printf("nil"); break;
		case LUA_TNUMBER:	printf("%lf", lua_tonumber(vm, -1)); break;
		case LUA_TBOOLEAN:	printf(lua_toboolean(vm, -1) ? "true" : "false"); break;
		case LUA_TSTRING:	printf("%s", lua_tostring(vm, -1));break;
		case LUA_TTABLE:	printf("table:%p", lua_topointer(vm, -1)); break;
		case LUA_TFUNCTION: printf("function:%p", lua_topointer(vm, -1)); break;
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
