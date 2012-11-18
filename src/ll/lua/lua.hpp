/***************************************************************
 * lua.hpp : implementation of lua related inline functions
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
namespace yue {
namespace module {
namespace ll {
/* lua */
inline lua::coroutine *lua::create(fiber *fb) {
	coroutine *co = fb->co();
	if (!co) { return NULL; }
	if (co->init(this) < 0) {
		destroy(co); return NULL;
	}
	return co;
}

/* lua::coroutine */
inline int lua::coroutine::operator () (loop::timer_handle) {
	fb()->resume();
	return NBR_ECANCEL;
}

inline int lua::coroutine::args::pack(serializer &sr) const {
	args *a = const_cast<args*>(this);
	int r = sr.pack_request(a->m_msgid, *this);
	if (r >= 0) {
		/* now, packet is under packed, not sent. */
		/* should yield here because if once packet is sent, it is possible that
		 * reply is received before fabric::yield call is finished. */
		if (yue::fabric::yield(m_co->fb(), args::m_msgid, m_timeout) < 0) {
			return NBR_ESHORT;
		}
	}
	return r;
}
/* emit handler (start) */
template <class PROC>
inline int lua::coroutine::load_proc(event::base &ev, PROC proc) {
	lua_getfield(m_exec, LUA_REGISTRYINDEX, lua::namespaces);
	lua_pushlightuserdata(m_exec, ev.ns_key());
	lua_gettable(m_exec, -2);
	if (!lua_istable(m_exec, -1)) { ASSERT(false); return NBR_ENOTFOUND; }
	push_procname(m_exec, proc);
	lua_gettable(m_exec, -2);
	/* if result is nil, no callback specified. */
	if (lua_isnil(m_exec, -1)) { TRACE("load_proc not found\n"); return NBR_ENOTFOUND; }
	return 1;	/* callback function, emittable */
}
inline int lua::coroutine::load_object(event::base &ev) {
	lua_getfield(m_exec, LUA_REGISTRYINDEX, lua::objects);
	lua_pushlightuserdata(m_exec, ev.ns_key());
	lua_gettable(m_exec, -2);
	lua_remove(m_exec, -2);	//remove objects table from stack
	lua::dump_stack(m_exec);
	if (!lua_istable(m_exec, -1)) { ASSERT(false); return NBR_ENOTFOUND; }
	return 1;
}
inline int lua::coroutine::start(event::session &ev) {
	int r;
	TRACE("co:start ev:session %p\n", ev.ns_key());
	if ((r = load_proc<const char *>(ev, symbol_socket[ev.m_state])) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::proc &ev) {
	int r, al;
	object &o = ev.m_object;
	al = o.alen();
	if (!fb()->endp().authorized()) {
		lua_pushfstring(m_exec, "unauthorized endpoint");
		r = NBR_ERIGHT; goto end;
	}
	if ((r = load_proc<const argument &>(ev, o.cmd())) < 0) { goto end; }
	for (int i = 0; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.arg(i))) < 0) { goto end; }
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(al);
}
inline int lua::coroutine::start(event::emit &ev) {
	int r, al;
	object &o = ev.m_object;
	al = o.size();
	ASSERT(al > 0);
	if ((r = load_proc<const argument &>(ev, o.elem(0))) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
	for (int i = 1; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.elem(i))) < 0) { goto end; }
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(al);
}
inline int lua::coroutine::start(event::timer &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_tick)) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::signal &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_signal)) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::listener &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_accept)) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
	lua_pushlightuserdata(m_exec, ev.accepted_key());
end:
	ASSERT(r >= 0);
	lua::dump_stack(m_exec);
	return r < 0 ? constant::fiber::exec_error : resume(2);
}
inline int lua::coroutine::start(event::fs &ev) {
	int r;
	if ((r = load_proc<const char *>(ev,
		handler::fs::symbol_from(ev.m_notify->flags()))) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
	lua_pushstring(m_exec, ev.m_notify->path());
end:
	return r < 0 ? constant::fiber::exec_error : resume(2);
}
inline int lua::coroutine::start(event::thread &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_join)) < 0) { goto end; }
	if ((r = load_object(ev)) < 0) { goto end; }
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}



/* resume handler */
inline int lua::coroutine::resume(event::proc &ev) {
	int r = unpack_response_to_stack(ev.m_object);
	return r < 0 ? constant::fiber::exec_error : resume(r);
}
inline int lua::coroutine::resume(event::emit &ev) {
	object &o = ev.m_object;
	int r, al = o.size();
	ASSERT(al > 0);
	for (int i = 1; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.elem(i))) < 0) { goto end; }
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(al - 1);
}
inline int lua::coroutine::resume(event::session &ev) {
	lua_pushboolean(m_exec, true);
	return resume(1);
}
inline int lua::coroutine::resume(event::timer &ev) {
	lua_pushboolean(m_exec, true);
	return resume(1);
}
inline int lua::coroutine::resume(event::signal &ev) {
	lua_pushboolean(m_exec, true);
	lua_pushinteger(m_exec, ev.m_signo);
	return resume(2);
}
inline int lua::coroutine::resume(event::listener &ev) {
	lua_pushlightuserdata(m_exec, ev.accepted_key());
	return resume(1);
}
inline int lua::coroutine::resume(event::fs &ev) {
	lua_pushstring(m_exec, ev.m_notify->path());
	return resume(1);
}
inline int lua::coroutine::resume(event::thread &ev) {
	lua_pushboolean(m_exec, true);
	return resume(1);
}
inline int lua::coroutine::resume(event::error &ev) {
	lua_pushboolean(m_exec, false);
	lua_newtable(m_exec);
	lua_pushinteger(m_exec, ev.m_errno);
	lua_pushinteger(m_exec, 1);
	lua_settable(m_exec, -3);
	if (ev.m_msg) {
		lua_pushstring(m_exec, ev.m_msg);
	}
	else {
		lua_pushvalue(m_exec, -2);
	}
	lua_pushinteger(m_exec, 2);
	lua_settable(m_exec, -3);
	return resume(2);
}

inline fiber *lua::coroutine::fb() {
	return reinterpret_cast<fiber *>(
		reinterpret_cast<U8 *>(this) - (sizeof(fiber) - sizeof(*this))
	);
}
}
}
}

