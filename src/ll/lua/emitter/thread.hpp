/***************************************************************
 * thread.h : inter-thread rpc emitter
 * 2012/09/28 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct thread : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_thread_new");
		lua_pushcfunction(vm, call);
		lua_setfield(vm, -2, "yue_thread_call");
		lua_pushcfunction(vm, count);
		lua_setfield(vm, -2, "yue_thread_count");
		lua_pushcfunction(vm, find);
		lua_setfield(vm, -2, "yue_thread_find");
		return NBR_OK;
	}
	static int create(VM vm) {
		int timeout_sec = 1;
		const char *pgname = lua_tostring(vm, 1);	//default pgname same as worker name
		if (lua_gettop(vm) >= 3) {
			if (lua_isstring(vm, 3)) { pgname = lua_tostring(vm, 3); }
			if (lua_isnumber(vm, 4)) { timeout_sec = lua_tointeger(vm, 4); }
		}
 		emittable *e = server::launch(lua_tostring(vm, 1), lua_tostring(vm, 2), timeout_sec, pgname);
		lua_error_check(vm, e, "fail to create thread (%s,[%s])", lua_tostring(vm, 1), lua_tostring(vm, 2));
		lua_pushlightuserdata(vm, e);
		return 1;
	}
	static int call(VM vm) {
		server::thread *th = reinterpret_cast<server::thread*>(lua_touserdata(vm, 1));
		lua_error_check(vm, th, "%s unavailable emitter", "call");
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		U32 flags = (U32)(lua_tointeger(vm, 2)), timeout = 0;
		if (flags & base::TIMED) {
			timeout = (U32)(lua_tonumber(vm, 4) * 1000 * 1000);
			lua_remove(vm, 4);
		}
		coroutine::args arg(co, 3, timeout);
		lua_error_check(vm, yue::serializer::INVALID_MSGID != rpc::call(*(th->svr()), arg), "callproc");
		return co->yield();
	}
	static int count(VM vm) {
		lua_pushinteger(vm, server::thread_count());
		return 1;
	}
	static int find(VM vm) {
		server::thread *t = server::get_thread(lua_tostring(vm, -1));
		if (t) { lua_pushlightuserdata(vm, t); }
		else { lua_pushnil(vm); }
		return 1;
	}
};
}
