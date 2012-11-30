/***************************************************************
 * timer.h : lightweight timer emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct timer : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_timer_new");
		return NBR_OK;
	}
	static int create(VM vm) {
		emittable *p = server::set_timer(lua_tonumber(vm, -2), lua_tonumber(vm, -1));
		lua_error_check(vm, p, "fail to create timer");
		lua_pushlightuserdata(vm, p);
		return 1;
	}
};
}
