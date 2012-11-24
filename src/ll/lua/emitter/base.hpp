/***************************************************************
 * base.h : emittable base object
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct base {
	enum {
		ASYNC 		= 0x00000001,
		TIMED 		= 0x00000002,
		MCAST 		= 0x00000004,
		PROTECTED 	= 0x00000008,
	};
	static inline server *sv() {
		return server::tlsv();
	}
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_emitter_new");
		lua_pushcfunction(vm, refer);
		lua_setfield(vm, -2, "yue_emitter_refer");
		lua_pushcfunction(vm, unref);
		lua_setfield(vm, -2, "yue_emitter_unref");
		lua_pushcfunction(vm, bind);
		lua_setfield(vm, -2, "yue_emitter_bind");
		lua_pushcfunction(vm, wait);
		lua_setfield(vm, -2, "yue_emitter_wait");
		lua_pushcfunction(vm, close);
		lua_setfield(vm, -2, "yue_emitter_close");
		lua_pushcfunction(vm, open);
		lua_setfield(vm, -2, "yue_emitter_open");
		lua_pushcfunction(vm, emit);
		lua_setfield(vm, -2, "yue_emitter_emit");
		return NBR_OK;
	}
	static int create(VM vm) {
		emittable *p = server::emitter();
		lua_error_check(vm, p, "fail to allocate raw emitter");
		lua_pushlightuserdata(vm, p);
		return 1;
	}
	static int bind(VM vm) {
		/* set given callback to this emitter's namespace and call fiber::bind() */
		/* emittable_ptr, event_id, flags */
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "fail to get coroutine");
		emittable *ptr = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "bind");
		emittable::event_id id = (emittable::event_id)(lua_tointeger(vm, 2));
		U32 timeout = ((lua_gettop(vm) > 3) ? (U32)(lua_tonumber(vm, 4) * 1000 * 1000) : 0);
		/* bind permanently */
		TRACE("fiber::bind to %p %u\n", ptr, id);
		switch (lua_type(vm, 3)) {
		case LUA_TNUMBER: {
			U32 flags = (U32)(lua_tointeger(vm, 3));
			lua_error_check(vm, fiber::bind(id, ptr, flags, co->fb(), timeout) >= 0, "fail to bind");
		} break;
		case LUA_TSTRING: {
			const char *flags = lua_tostring(vm, 3);
			lua_error_check(vm, fiber::bind(id, ptr, flags, co->fb(), timeout) >= 0, "fail to bind");
		} break;
		default:
			lua_error_check(vm, false, "invalid flag type: %u", lua_type(vm, 3));
		}
		return co->yield();
	}
	static int wait(VM vm) {
		/* emittable_ptr, event_id, flags, timeout */
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "fail to get coroutine");
		emittable *ptr = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "wait");
		emittable::event_id id = (emittable::event_id)(lua_tointeger(vm, 2));
		U32 timeout = ((lua_gettop(vm) > 3) ? (U32)(lua_tonumber(vm, 4) * 1000 * 1000) : 0);
		ASSERT(timeout == 0 || timeout > 1000000);
		TRACE("fiber::wait to %p %u\n", ptr, id);
		switch (lua_type(vm, 3)) {
		case LUA_TNUMBER: {
			U32 flags = (U32)(lua_tointeger(vm, 3));
			lua_error_check(vm, co->fb()->wait(id, ptr, flags, timeout) >= 0, "fail to wait");
		} break;
		case LUA_TSTRING: {
			const char *flags = lua_tostring(vm, 3);
			lua_error_check(vm, co->fb()->wait(id, ptr, flags, timeout) >= 0, "fail to wait");
		} break;
		default:
			lua_error_check(vm, false, "invalid flag type: %u", lua_type(vm, 3));
		}
		return co->yield();
	}
	static int close(VM vm) {
		emittable *ptr = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "close");
		server::close(ptr);
		return 0;
	}
	static int unref(VM vm) {
		emittable *p = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, p, "%s unavailable emitter", "unref");
		UNREF_EMPTR(p);
		return 0;
	}
	static int refer(VM vm) {
		emittable *p = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, p, "%s unavailable emitter", "refer");
		REFER_EMPTR(p);
		return 0;
	}
	static int open(VM vm) {
		emittable *p = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, p, "%s unavailable emitter", "open");
		int r = server::open(p);
		lua_error_check(vm, (r >= 0 || r == NBR_EALREADY), "server::open fails %d", r);
		return 0;
	}
	static int emit(VM vm) {
		emittable *p = reinterpret_cast<emittable *>(lua_touserdata(vm, 1));
		lua_error_check(vm, p, "%s unavailable emitter", "emit");
		event::emit ev(p);
		coroutine::get_object_from_stack(vm, 2, ev.m_object);
		TRACE("emit: %s\n", (const char *)ev.m_object.elem(0));
		p->emit(event::ID_EMIT, ev);
		return 0;
	}
};
}
