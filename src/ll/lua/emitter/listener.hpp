/***************************************************************
 * listener.h : connection listener emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct listener : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_listener_new");
		return NBR_OK;
	}
	static int create(VM vm) {
		object o, *po = NULL;
		if (lua_gettop(vm) > 2) {
			lua_error_check(vm, coroutine::get_object_from_table(vm, 3, o) >= 0, "fail to get object from stack");
			po = &o;
		}
		emittable *e = base::sv()->listen(lua_tostring(vm, 1), po);
		lua_error_check(vm, e, "fail to create listener (%s)",
			lua_tostring(vm, 1));
		lua_pushlightuserdata(vm, e);
		return 1;
	}
};
}
