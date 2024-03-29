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

#define ERROR(fmt, ...)	{	\
	char __b[256]; snprintf(__b, sizeof(__b), fmt, __VA_ARGS__);	 \
	TRACE("buf = %s\n", __b);	\
	lua_pushfstring(m_exec, "Error@%s(%d):%s", __FILE__, __LINE__, __b); goto end;	\
}

/* emit handler (start) */
template <class PROC>
inline int lua::coroutine::load_proc(event::base &ev, PROC proc, bool ignore_protection) {
	lua_getfield(m_exec, LUA_REGISTRYINDEX, lua::namespaces);
	lua_pushlightuserdata(m_exec, ev.ns_key());
	lua_gettable(m_exec, -2);
	if (!lua_istable(m_exec, -1)) { ASSERT(false); return NBR_ENOTFOUND; }
	if (ignore_protection) {
		lua_getfield(m_exec, -1, lua::namespace_symbol);
	}
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
	if ((r = load_proc<const char *>(ev, symbol_socket[ev.m_state])) < 0) { 
		ERROR("fail to load proc for %s", symbol_socket[ev.m_state]);
	}
	if ((r = load_object(ev)) < 0) {
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::proc &ev) {
	int r, al;
	object &o = ev.m_object;
	al = o.alen();
	if (!fb()->endp().authorized()) {
		r = NBR_ERIGHT;
		ERROR("unauthorized endpoint %p", ev.ns_key()); 
	}
	if ((r = load_proc<const argument &>(ev, o.cmd(), false)) < 0) {
		ERROR("fail to load proc %u", o.msgid()); 
	}
	for (int i = 0; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.arg(i))) < 0) {
			ERROR("fail to unpack value %u", i); 
		}
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(al);
}
inline int lua::coroutine::start(event::emit &ev) {
	int r, al;
	object &o = ev.m_object;
	al = o.size();
	ASSERT(al > 0);
	char buffer[2 + 1 + o.elem(0).len()];
	util::str::printf(buffer, sizeof(buffer), "__%s", (const char *)o.elem(0));
	if ((r = load_proc<const char *>(ev, buffer)) < 0) {
		ERROR("fail to load proc %s", buffer);
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
	for (int i = 1; i < al; i++) {
		if ((r = unpack_stack(m_exec, o.elem(i))) < 0) { 
			ERROR("fail to unpack value %u", i); 
		}
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(al);
}
inline int lua::coroutine::start(event::timer &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_tick)) < 0) { 
		ERROR("fail to load proc %s", symbol_tick); 
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::signal &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_signal)) < 0) { 
		ERROR("fail to load proc %s", symbol_signal); 
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
end:
	return r < 0 ? constant::fiber::exec_error : resume(1);
}
inline int lua::coroutine::start(event::listener &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_accept)) < 0) { 
		ERROR("fail to load proc %s", symbol_accept); 
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
	lua_pushlightuserdata(m_exec, ev.accepted_key());
end:
	ASSERT(r >= 0);
	lua::dump_stack(m_exec);
	return r < 0 ? constant::fiber::exec_error : resume(2);
}
inline int lua::coroutine::start(event::fs &ev) {
	int r;
	if ((r = load_proc<const char *>(ev,
		handler::fs::symbol_from(ev.m_notify->flags()))) < 0) { 
		ERROR("fail to load proc %s", 
			handler::fs::symbol_from(ev.m_notify->flags())); 
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
	lua_pushstring(m_exec, ev.m_notify->path());
end:
	return r < 0 ? constant::fiber::exec_error : resume(2);
}
inline int lua::coroutine::start(event::thread &ev) {
	int r;
	if ((r = load_proc<const char *>(ev, symbol_join)) < 0) { 
		ERROR("fail to load proc %s", symbol_join); 
	}
	if ((r = load_object(ev)) < 0) { 
		ERROR("fail to load object for %p", ev.ns_key()); 
	}
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

	lua_pushinteger(m_exec, 1);
	lua_pushinteger(m_exec, ev.m_error->code);
	TRACE("ecode = %d\n", ev.m_error->code);
	lua_settable(m_exec, -3);

	lua_pushinteger(m_exec, 2);
	lua_pushstring(m_exec, ev.m_msg ? ev.m_msg : "");
	lua_settable(m_exec, -3);

	TRACE("resume error %p\n", this);
	lua::dump_stack(m_exec);
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

