/***************************************************************
 * socket.h : socket IO emitter
 * 2012/09/01 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
namespace emitter {
struct peer : public base {
#if 0
	struct box {
		server::peer *m_peer;
		box(server::peer *p) : m_peer(p) {}
		void *operator new (size_t, void *p) { return p; }
	};
	static int init(VM vm) {
		lua_pushcfunction(vm, call);
		lua_setfield(vm, -2, "yue_peer_call");
		return NBR_OK;
	}
	static int create(VM vm, handler::socket *s, const net::address &addr) {
		server::peer *p = server::open_peer(s, addr);
		lua_error_check(vm, p, "fail to create peer");
		new (lua_newuserdata(vm, sizeof(box))) box(p);
		return 1;
	}
	static inline int bind(VM vm, args &a) {
		const char *event = lua_tostring(vm, 4);
		U32 flag = socket::event_mask_from(event);
		lua_error_check(vm, flag != 0, "no event hooked (%s)", event);
		lua_getfield(vm, LUA_REGISTRYINDEX, lua::namespaces);	/* get namespaces table */
		lua_pushvalue(vm, 2);	/* push emitter pointer lightuserdata */
		lua_gettable(vm, -2);	/* load namespace of this emitter */
		for (int i = (1 + handler::socket::HANDSHAKE); i <= handler::socket::CLOSED; i++) {
			if (flag & (1 << i)) {
				lua_pushvalue(vm, 5);	/* load callback function on stack top */
				lua_setfield(vm, -2, handler::socket::state_symbols[i - 1]);	/* put function to namespace */
			}
		}
		/* bind permanently */
		lua_error_check(vm,
			fiber::bind(event::ID_SESSION, a.to_ptr<box>()->m_peer->s(), flag) >= 0,
			"fail to bind");
		return 0;
	}
	static inline int wait(VM vm, args &a) {
		const char *event = lua_tostring(vm, 4);
		U32 flag = socket::event_mask_from(event);
		lua_error_check(vm, flag != 0, "no event hooked (%s)", event);
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		lua_error_check(vm, co->fb()->wait(
			event::ID_SESSION, a.to_ptr<box>()->m_peer->s(), flag, a.m_timeout) >= 0,
			"fail to bind");
		return co->yield();
	}
	static int close(VM vm, args &a) {
		server::close_peer(a.to_ptr<box>()->m_peer);
		return 0;
	}
	static int call(VM vm) {
		int r; base::args a;
		const char *method;
		get_args(vm, a);
		method = lua_tostring(vm, 3);
		if (!method || ((r = base::invoke<peer>(vm, method, a, false)) < 0)) {
			coroutine *co = coroutine::to_co(vm);
			lua_error_check(vm, co, "to_co");
			coroutine::args arg(co, a.m_timeout);
			lua_error_check(vm, yue::serializer::INVALID_MSGID != rpc::call(
				*(a.to_ptr<box>()->m_peer->s()), arg),
				"callproc");
			return co->yield();
		}
		return r;
	}
#endif
};
}
