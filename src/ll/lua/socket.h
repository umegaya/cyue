struct sock {
	session *m_s;
	session::serial m_sn;
	struct sender {
		VM m_vm;
		inline sender(VM vm) : m_vm(vm) {}
		inline int pack(yue::serializer &sr) const {
			/* array len also packed in following method
			 * (eg. lua can return multiple value) */
			verify_success(coroutine::pack_stack(m_vm, 1, sr));
			return sr.len();
		}
	};
	static int init_metatable(VM vm) {
		lua_newtable(vm);
		lua_pushcfunction(vm, sock::read);
		lua_setfield(vm, -2, "read");
		lua_pushcfunction(vm, sock::write);
		lua_setfield(vm, -2, "write");
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
			sk = lua::module::served()->spool().open(
				lua_tostring(vm, -1), NULL, true);
		} break;
		case 2: {
			const char *addr;
			lua_error_check(vm, (addr = lua_tostring(vm, -2)), "type error");
			if (lua_istable(vm, top)) {
				object obj;
				lua_error_check(vm, (lua::coroutine::get_object_from_table(vm, top, obj) >= 0),
						"obj from table");
				sk = lua::module::served()->spool().open(addr, &obj, true);
			}
			else {
				sk = lua::module::served()->spool().open(addr, NULL, true);
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
	bool valid() const { return m_s->serial_id() == m_sn; }
	static int read(VM vm) {
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_error_check(vm, sk->valid(), "invalid socket");
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		return read(sk->m_s, co, false);
	}
	static int write(VM vm) {
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_error_check(vm, sk->valid(), "invalid socket");
		coroutine *co = coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		return write(sk->m_s, co, false);
	}
	static int close(VM vm) {
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		if (sk->valid()) {
			sk->m_s->close();
		}
		return 0;
	}
	static int fd(VM vm) {
		sock *sk = reinterpret_cast<sock *>(
			lua_touserdata(vm, 1)
		);
		lua_pushinteger(vm, sk->m_s->fd());
		return 1;
	}
public:
	static inline int read(session *sk, coroutine *co, bool resume) {
		VM vm = co->vm(); int r;
		if (!sk->valid()) {
			session::watcher sw(*co);
			co->set_flag(coroutine::FLAG_READ_RAW_SOCK, true);
			lua_error_check(vm, (sk->reconnect(sw) >= 0), "reconnect");
			return co->yield();
		}
		switch((r = sk->read(reinterpret_cast<char *>(lua_touserdata(vm, 2)), 
			lua_tointeger(vm, 3)))) {
		case -1:
			TRACE("read: fails %d\n", syscall::error_no());
			if (syscall::error_again()) {
				co->set_flag(coroutine::FLAG_READ_RAW_SOCK, true);
				session::watcher sw(*co);
				sk->add_watcher(sw);
				return co->yield();
			}
		case 0:	/* connection closed */
			TRACE("read: conn closes\n");
			lua_pushnil(vm);
			return resume ? co->resume(1) : 1;
		default:
			TRACE("read success: %d\n", r);
			lua_pushinteger(vm, r);
			return resume ? co->resume(1) : 1;
		}
	}
	static inline int write(session *sk, coroutine *co, bool resume) {
		VM vm = co->vm();
		if (sk->valid()) {
			sender s(co->vm()); yue::serializer sr;
			lua_pushinteger(vm, sk->writeo(sr, s));
			return resume ? co->resume(1) : 1;
		}
		else if (!resume) {
			session::watcher sw(*co);
			co->set_flag(coroutine::FLAG_WRITE_RAW_SOCK, true);
			lua_error_check(vm, (sk->reconnect(sw) >= 0), "reconnect");
			return co->yield();
		}
		else {
			lua_error_check(vm, false, "resume error");
			return 0;
		}
	}
};
