/***************************************************************
 * signal.h : signal event emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct signal : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_signal_new");
		return NBR_OK;
	}
	static int create(VM vm) {
		emittable *p = server::signal(lua_tointeger(vm, -1));
		lua_error_check(vm, p, "fail to create signal for %d", 
			(int)lua_tointeger(vm, -1));
		lua_pushlightuserdata(vm, p);
		return 1;
	}
};
}
