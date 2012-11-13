/***************************************************************
 * peer.hpp : fiber endpoint
 * 2012/09/28 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
struct peer {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_thread_new");
		lua_pushcfunction(vm, call);
		lua_setfield(vm, -2, "yue_thread_call");
		return NBR_OK;
	}
	static int create(VM vm) {
		int timeout_sec = (lua_gettop(vm) >= 3 ? lua_tointeger(vm, 3) : 1);
 		emittable *e = server::launch(lua_tostring(vm, 1), lua_tostring(vm, 2), timeout_sec);
		lua_error_check(vm, e, "fail to create thread (%s,[%s])", lua_tostring(vm, 1), lua_tostring(vm, 2));
		create_from_ptr(vm, e);
		return 1;
	}
	static int call(VM vm) {
		int r; base::args a;
		const char *method;
		get_args(vm, a);
		method = lua_tostring(vm, 3);
		if (!method || ((r = base::invoke<base>(vm, method, a, false)) < 0)) {
			server::thread *th = reinterpret_cast<server::thread *>(a.m_p);
			coroutine *co = coroutine::to_co(vm);
			lua_error_check(vm, co, "to_co");
			coroutine::args arg(co, a.m_timeout);
			lua_error_check(vm, yue::serializer::INVALID_MSGID != rpc::call(*(th->svr()), arg), "callproc");
			return co->yield();
		}
		return r;
	}
};
