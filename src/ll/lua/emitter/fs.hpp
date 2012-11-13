/***************************************************************
 * fs.h : filesystem emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct fs : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_fs_new");
		create_event_table(vm);
		lua_setfield(vm, -2, "yue_fs_event_flags");
		return NBR_OK;
	}
	static int create(VM vm) {
		emittable *e = server::fs_watch(lua_tostring(vm, 1), lua_tostring(vm, 2));
		lua_error_check(vm, e, "fail to create fs watcher (%s,%s)",
			lua_tostring(vm, 1), lua_tostring(vm, 2));
		lua_pushlightuserdata(vm, e);
		return 1;
	}
	static int create_event_table(VM vm) {
		lua_newtable(vm);
		return NBR_OK;
	}
};
}
