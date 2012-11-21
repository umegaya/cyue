/***************************************************************
 * fiber.hpp : fiber creation/execution
 * 2012/11/19 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct fiber {
	static int create(VM vm) {
		yue::fiber *fb = server::tlsv()->fbr().create();
		lua_error_check(vm, fb, "fail to create fiber");
		lua_pushlightuserdata(vm, fb);
		return 1;
	}
	static int run(VM vm) {
		yue::fiber *fb = reinterpret_cast<yue::fiber *>(lua_touserdata(vm, 1));
		int top = lua_gettop(vm) - 1;//contains function and its args.
		lua_xmove(vm, fb->co()->vm(), top);
		fb->start(top - 1); //start with number of args (-1 for reduce function index)
		return 1;
	}
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_fiber_new");
		lua_pushcfunction(vm, run);
		lua_setfield(vm, -2, "yue_fiber_run");
		return NBR_OK;
	}
};
}
