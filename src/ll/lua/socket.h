struct sock {
	typedef lua::session session;
	session *m_s;
	session::serial m_sn;
	static int init_metatable(VM vm) {
		lua_newtable(vm);
		lua_pushcfunction(vm, sock::read_cb);
		lua_setfield(vm, -2, "read_cb");
		lua_pushcfunction(vm, sock::try_connect);
		lua_setfield(vm, -2, "try_connect");
		lua_pushcfunction(vm, sock::close);
		lua_setfield(vm, -2, "close");
		lua_pushcfunction(vm, sock::fd);
		lua_setfield(vm, -2, "fd");
		lua_pushcfunction(vm, sock::close);
		lua_setfield(vm, -2, lua::gc_method);
		lua_pushvalue(vm, -1);
		lua_setfield(vm, -2, lua::index_method);
		return NBR_OK;
	}
	static int init(VM vm) {
		session *sk; int top;
		switch((top = lua_gettop(vm))) {
		case 1: {
			lua_error_check(vm, (lua_isstring(vm, -1)), "type error");
			sk = lua::module::served::spool().open(
				lua_tostring(vm, -1), NULL, true);
		} break;
		case 2: {
			const char *addr;
			lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
			if (lua_istable(vm, top)) {
				object obj;
				lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
						"obj from table");
				sk = lua::module::served::spool().open(addr, &obj, true);
			}
			else {
				sk = lua::module::served::spool().open(addr, NULL, true);
			}
		} break;
		}
		lua_error_check(vm, sk, "create session");
		sock *s = reinterpret_cast<sock *>(
			lua_newuserdata(vm, sizeof(sock))
		);
		s->m_s = sk;
		s->m_sn = sk->serial_id();
		lua_getglobal(vm, lua::sock_metatable);
		lua_setmetatable(vm, -2);
		return 1;
	}
protected:
	inline bool valid() const { return m_sn != 0 && m_s->serial_id() == m_sn; }
	static int read_cb(VM vm) {
#if 1
		lua_error_check(vm, lua_isuserdata(vm, 1), "invalid arg %d", lua_type(vm, 1));
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		coroutine *co = coroutine::to_co(vm); int r;
		lua_error_check(vm, co, "to_co");
		switch((r = lua_tointeger(vm, 2))) {
		case -1:
			TRACE("read: fails %d\n", syscall::error_no());
			if (syscall::error_again()) {
				co->set_flag(coroutine::FLAG_READ_RAW_SOCK, true);
				session::watcher sw(*co);
				sk->m_s->add_watcher(sw);
				return 1;	//return -1 to indicate LuaJIT to yield
			}
		case 0:	/* connection closed */
			TRACE("read: conn closes\n");
			lua_pop(vm, 1);
			lua_pushnil(vm);
			return 1;
		default:
			TRACE("read success: %d\n", r);
			return 1;
		}
#else
		lua_error_check(vm, lua_isuserdata(vm, 1), "invalid arg %d", lua_type(vm, 1));
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_error_check(vm, sk->valid(), "invalid socket");
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		return read_cb(sk->m_s, co, false);
#endif
	}
	static int try_connect(VM vm) {
		lua_error_check(vm, lua_isuserdata(vm, 1), "invalid arg %d", lua_type(vm, 1));
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_error_check(vm, sk->m_sn == 0 || sk->valid(), "invalid socket");
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		return try_connect(sk->m_s, co, false);
	}
	static int close(VM vm) {
		lua_error_check(vm, lua_isuserdata(vm, 1), "invalid arg %d", lua_type(vm, 1));
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		if (sk->valid()) {
			sk->m_s->close();
		}
		return 0;
	}
	static int fd(VM vm) {
		lua_error_check(vm, lua_isuserdata(vm, 1), "invalid arg %d", lua_type(vm, 1));
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_pushinteger(vm, sk->m_s->fd());
		return 1;
	}
public:
	static inline int read_cb(session *sk, coroutine *co, bool resume) {
		VM vm = co->vm(); int r;
		lua_pushvalue(vm, -1);	//copy function on the stack
		lua_pushlightuserdata(vm, sk);
		lua::dump_stack(vm);
		lua_error_check(vm, (r = lua_pcall(vm, 1, 1, 0)) == 0, "lua_pcall (%d)", r);
		switch((r = lua_tointeger(vm, -1))) {
		case -1:
			TRACE("read: fails %d %s\n", syscall::error_no(), resume ? "resume" : "normal");
			if (syscall::error_again()) {
				co->set_flag(coroutine::FLAG_READ_RAW_SOCK, true);
				session::watcher sw(*co);
				sk->add_watcher(sw);
				return resume ? co->resume(1) : 1;	//return -1 to indicate LuaJIT to yield
			}
		case 0:	/* connection closed */
			TRACE("read: conn closes\n");
			lua_pop(vm, 1);
			lua_pushnil(vm);
			return resume ? co->resume(1) : 1;
		default:
			TRACE("read success: %d\n", r);
			return resume ? co->resume(1) : 1;
		}
	}
	static inline int try_connect(session *sk, coroutine *co, bool resume) {
		VM vm = co->vm();
		if (sk->valid()) {
			sock *s = reinterpret_cast<sock *>(
				lua_touserdata(vm, 1)
			);
			s->m_sn = sk->serial_id();
			lua_pushlightuserdata(vm, sk);
 			return resume ? co->resume(1) : 1;
		}
		else if (!resume) {
			session::watcher sw(*co);
			co->set_flag(coroutine::FLAG_CONNECT_RAW_SOCK, true);
			lua_error_check(vm, (sk->reconnect(sw) >= 0), "reconnect");
			return co->yield();
		}
		else {
			lua_error_check(vm, false, "resume error");
			return 0;
		}
	}
};
