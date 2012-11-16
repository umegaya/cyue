/***************************************************************
 * fiber.hpp : fiber creation/execution
 * 2012/11/19 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
struct fiber {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_fiber_new");
		lua_pushcfunction(vm, run);
		lua_setfield(vm, -2, "yue_fiber_run");
		return NBR_OK;
	}
	static int create(VM vm) {
		fiber *fb = server::tlsv()->fbr().create();
		lua_error_check(vm, co, "fail to create fiber");
		lua_pushlightuserdata(vm, fb);
		return 1;
	}
	static int run(VM vm) {
		fiber *fb = reinterpret_cast<fiber *>(lua_touserdata(vm));
		int top = lua_gettop(vm);
		lua_xmove(vm, fb->co()->vm(), top);
		fb->start(top);
		return 1;
	}
};
}
