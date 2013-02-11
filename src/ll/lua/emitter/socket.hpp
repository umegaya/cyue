/***************************************************************
 * socket.h : socket IO emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct socket : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, create);
		lua_setfield(vm, -2, "yue_socket_new");
		lua_pushcfunction(vm, call);
		lua_setfield(vm, -2, "yue_socket_call");
		lua_pushcfunction(vm, connected);
		lua_setfield(vm, -2, "yue_socket_connected");
		lua_pushcfunction(vm, connect);
		lua_setfield(vm, -2, "yue_socket_connect");
		lua_pushcfunction(vm, grant);
		lua_setfield(vm, -2, "yue_socket_grant");
		lua_pushcfunction(vm, authorized);
		lua_setfield(vm, -2, "yue_socket_authorized");
		lua_pushcfunction(vm, valid);
		lua_setfield(vm, -2, "yue_socket_valid");
		lua_pushcfunction(vm, address);
		lua_setfield(vm, -2, "yue_socket_address");
		lua_pushcfunction(vm, listener);
		lua_setfield(vm, -2, "yue_socket_listener");
		lua_pushcfunction(vm, closed);
		lua_setfield(vm, -2, "yue_socket_closed");
		lua_pushcfunction(vm, close_for_reconnection);
		lua_setfield(vm, -2, "yue_socket_close_for_reconnection");
		return NBR_OK;
	}
	static int create(VM vm) {
		object o, *po = NULL;
		if (lua_istable(vm, 2)) {
			lua_error_check(vm, coroutine::get_object_from_table(vm, 2, o) >= 0, "fail to get object from stack");
			po = &o;
		}
 		emittable *e = base::sv()->open(lua_tostring(vm, 1), po);
		lua_error_check(vm, e, "fail to create socket (%s)", lua_tostring(vm, 1));
		lua_pushlightuserdata(vm, e);
		return 1;
	}
	static int call(VM vm) {
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "call");
		U32 flags = (U32)(lua_tointeger(vm, 2)), timeout = 0;
		if (flags & base::TIMED) {
			timeout = (U32)(lua_tonumber(vm, 4) * 1000 * 1000);
			lua_remove(vm, 4);
		}
		TRACE("socket call: stack: (to %u)\n", timeout);
		lua::dump_stack(vm);
		coroutine::args arg(co, 3, timeout);
		lua_error_check(vm,
			yue::serializer::INVALID_MSGID != rpc::call(*ptr, arg),
			"callproc");
		return co->yield();
	}
	static int connected(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_return_error(vm, ptr, "Error", "%s unavailable emitter", "connected");
		lua_pushboolean(vm, ptr->valid());
		return 1;
	}
	static int connect(VM vm) {
		int r; 
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_return_error(vm, ptr, "Error", "%s unavailable emitter", "connect");
		if (ptr->valid()) {
			return 0;
		}
		coroutine *co = coroutine::to_co(vm);
		lua_return_error(vm, co, "Error", "to_co");
		U32 timeout = ((lua_gettop(vm) > 1) ? (U32)(lua_tointeger(vm, 2) * 1000 * 1000) : 0);
		/* server connection already start connect after LL-side object creation */
		if (ptr->is_server_conn()) {
			lua_return_error(vm, co->fb()->wait(event::ID_SESSION, ptr,
				(1 << handler::socket::WAITACCEPT), timeout) >= 0, "Error", "fail to bind");
			return co->yield();
		}
		else {
			if ((r = ptr->open_client_conn(ptr->skconf().timeout)) < 0) {
				lua_return_error(vm, r == NBR_EALREADY, "NetworkUnreachableError", "open connection error: %d", r);
			}
			TRACE((r == NBR_EALREADY ? "open client conn already\n" : "open client conn %d\n"), r);
			/* call once and do resume */
			lua_return_error(vm, co->fb()->wait(event::ID_SESSION, ptr,
				(1 << handler::socket::ESTABLISH), timeout) >= 0, "Error", "fail to bind");
			return co->yield();
		}
	}
	static int grant(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "grant");
		ptr->grant();
		return 0;
	}
	static int authorized(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "authorized");
		lua_pushboolean(vm, ptr->authorized());
		return 1;
	}
	static int valid(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "valid");
		lua_pushboolean(vm, ptr->valid());
		return 1;
	}
	static int address(VM vm) {
		char addr[256];
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "address");
		lua_pushstring(vm, ptr->addr().get(addr, sizeof(addr), ptr->t()));
		return 1;
	}
	static int listener(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "listener");
		emittable *p = ptr->accepter();
		if (p) {
			lua_pushlightuserdata(vm, p);
		}
		else {
			lua_pushnil(vm);
		}
		return 1;
	}
	static int closed(VM vm) {
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "closed");
		lua_pushboolean(vm, ptr->has_flag(handler::socket::F_FINALIZED));
		return 1;
	}
	static int close_for_reconnection(VM vm) {
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		handler::socket *ptr = reinterpret_cast<handler::socket *>(lua_touserdata(vm, 1));
		lua_error_check(vm, ptr, "%s unavailable emitter", "closed");
		U32 timeout = ((lua_gettop(vm) > 1) ? (U32)(lua_tointeger(vm, 2) * 1000 * 1000) : 0);
		ptr->close(false);
		lua_error_check(vm, co->fb()->wait(event::ID_SESSION, ptr,
			(1 << handler::socket::CLOSED), timeout) >= 0, "fail to bind");
		return co->yield();
	}
};
}
