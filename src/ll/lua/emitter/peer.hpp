/***************************************************************
 * socket.h : socket IO emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct peer : public base {
	static int init(VM vm) {
		lua_pushcfunction(vm, call);
		lua_setfield(vm, -2, "yue_peer_call");
		lua_pushcfunction(vm, close);
		lua_setfield(vm, -2, "yue_peer_close");
		return NBR_OK;
	}
	static int create(VM vm, handler::socket *s, const net::address &addr) {
		server::peer *p = server::open_peer(s, addr);
		lua_pushlightuserdata(vm, p);
		return 1;
	}
	static int close(VM vm) {
		server::peer *ptr = reinterpret_cast<server::peer *>(lua_touserdata(vm, 1));
		server::close_peer(ptr);
		return 0;
	}
	static int call(VM vm) {
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		server::peer *ptr = reinterpret_cast<server::peer *>(lua_touserdata(vm, 1));
		U32 flags = (U32)(lua_tointeger(vm, 2)), timeout = 0, start = 3;
		if (flags & base::TIMED) {
			timeout = (U32)(lua_tointeger(vm, 3));
			start++;
		}		
		coroutine::args arg(co, start, timeout);
		lua_error_check(vm, yue::serializer::INVALID_MSGID != rpc::call(*ptr, arg), "callproc");
		return co->yield();
	}
};
}
