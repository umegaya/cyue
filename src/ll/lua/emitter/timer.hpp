/***************************************************************
 * timer.h : lightweight timer emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct timer : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create_timer);
		lua_setfield(vm, -2, "yue_timer_new");
		lua_pushcfunction(vm, find);
		lua_setfield(vm, -2, "yue_timer_find");
		lua_pushcfunction(vm, create_taskgrp);
		lua_setfield(vm, -2, "yue_taskgrp_new");
		lua_pushcfunction(vm, create_task);
		lua_setfield(vm, -2, "yue_task_new");
		return NBR_OK;
	}
	static int create_timer(VM vm) {
		emittable *p = NULL;
		if (lua_gettop(vm) == 3) {
			p = base::sv()->create_timer(lua_tostring(vm, -3), lua_tonumber(vm, -2), lua_tonumber(vm, -1));
		}
		else if (lua_gettop(vm) == 2) {
			p = server::create_timer(lua_tonumber(vm, -2), lua_tonumber(vm, -1));
		}
		else {
			lua_error_check(vm, false, "wrong number of argument (%d)", lua_gettop(vm));
		}
		lua_error_check(vm, p, "fail to create %s", "timer");
		lua_pushlightuserdata(vm, p);
		return 1;
	}
	static int create_taskgrp(VM vm) {
		emittable *p = NULL;
		if (lua_gettop(vm) == 4) {
			p = base::sv()->create_taskgrp(
				lua_tostring(vm, -4), lua_tonumber(vm, -3),
				lua_tonumber(vm, -2), (int)(lua_tonumber(vm, -1) * 1000 * 1000));
		}
		else {
			lua_error_check(vm, false, "wrong number of argument (%d)", lua_gettop(vm));
		}
		lua_error_check(vm, p, "fail to create %s", "taskgrp");
		lua_pushlightuserdata(vm, p);
		return 1;
	}
	static int create_task(VM vm) {
		emittable *p = NULL;
		if (lua_gettop(vm) == 3) {
			handler::timerfd *tfd = reinterpret_cast<handler::timerfd *>(lua_touserdata(vm, -3));
			lua_error_check(vm, tfd, "taskgrp object not given");
			p = server::create_task(tfd, lua_tonumber(vm, -2), lua_tonumber(vm, -1));
		}
		else {
			lua_error_check(vm, false, "wrong number of argument (%d)", lua_gettop(vm));
		}
		lua_error_check(vm, p, "fail to create %s", "task");
		lua_pushlightuserdata(vm, p);
		return 1;
	}
	static int find(VM vm) {
		emittable *p = base::sv()->find_timer(lua_tostring(vm, -1));
		if (p) { lua_pushlightuserdata(vm, p); }
		else { lua_pushnil(vm); }
		return 1;
	}
};
}
