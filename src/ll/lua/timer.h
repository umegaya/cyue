/******************************************************************************************/
/* timer */
struct timer {
	fabric *m_fbr;
	loop::timer_handle m_t;
	U32 m_flag;
	enum {
		TF_ATTEMPT_TO_STOP,
		TF_DELEGATED,
	};
	static inline int init(VM vm) {
		ASSERT(lua_isfunction(vm, -1) && lua_isnumber(vm, -2) && lua_isnumber(vm, -3));
		timer *t = reinterpret_cast<timer *>(lua_newuserdata(vm, sizeof(timer)));/*4*/
		lua_pushlightuserdata(vm, t);	/* t will be key also (5) */
		/* ref timer callback function */
		lua_insert(vm, -3);		/* insert key to the position of callback function */
		lua_insert(vm, -3);		/* insert key to the position of callback function */
		lua::dump_stack(vm);	/* now stack layout should be num,num,userdata,userdata,function */
		lua_settable(vm, LUA_REGISTRYINDEX);/* reg[userdata] = function. then will be num,num,userdata */

		yue::util::functional<int (loop::timer_handle)> h(*t);
		TRACE("setting: %lf, %lf\n", lua_tonumber(vm, -3), lua_tonumber(vm, -2));
		if (!(t->m_t = lua::module::served::set_timer(
			lua_tonumber(vm, -3), lua_tonumber(vm, -2), h))) {
			lua_pushfstring(vm, "create timer");
			lua_error(vm);
		}
		t->m_fbr = &(fabric::tlf());
		t->m_flag = 0;
		/* return t as timer object */
		return 1;
	}
	static inline int stop(VM vm, timer *t) {
		if (t->m_flag & TF_DELEGATED) {
			t->m_flag |= TF_ATTEMPT_TO_STOP;
			return 0;
		}
		/* unref timer callback function */
		lua_pushlightuserdata(vm, t);
		lua_pushnil(vm);
		lua_settable(vm, LUA_REGISTRYINDEX);
		/* stop timer (t is freed) */
		lua::module::served::stop_timer(t->m_t);
		t->m_t = NULL;
		t->m_fbr = NULL;
		t->m_flag = 0;
		return 0;
	}
	int operator () (fabric &f, void *) {
		if (m_flag & TF_ATTEMPT_TO_STOP) {
			lua *l = reinterpret_cast<lua *>(&(f.lang()));
			timer::stop(l->vm(), this);
			return fiber::exec_finish;
		}
		m_flag &= ~(TF_DELEGATED);
		if (operator () (m_t) < 0) {
			lua *l = reinterpret_cast<lua *>(&(f.lang()));
			timer::stop(l->vm(), this);
			return fiber::exec_error;
		}
		return fiber::exec_finish;
	}
	static int tick(loop::timer_handle t) {
		fabric *fbr = &(fabric::tlf());
		VM vm = fbr->lang().vm();
#if defined(_DEBUG)
		int top = lua_gettop(vm);
		ASSERT(top == 0);
#endif
		lua::module::registry(vm);
		lua_getfield(vm, -1, lua::tick_callback);
		if (!lua_isnil(vm, -1)) {
			if (lua_pcall(vm, 0, 0, 0) != 0) {
				TRACE("timer::tick error %s\n", lua_tostring(vm, -1));
			}
			lua_pop(vm, 1);
		}
		else {
			lua_pop(vm, 2);
		}
#if defined(_DEBUG)
		ASSERT(top == lua_gettop(vm));
#endif
		return NBR_OK;
	}
	inline fabric *attached() { return m_fbr; }
	inline int setup(VM vm, loop::timer_handle) {
		lua_pushlightuserdata(vm, this);
		lua_gettable(vm, LUA_REGISTRYINDEX);
		ASSERT(lua_isfunction(vm, -1));
		lua_pushlightuserdata(vm, this);
		return 1;
	}
	inline void delegate(fabric *fbr, loop::timer_handle) {
		fiber::phandler h(*this);
		m_flag |= TF_DELEGATED;
		m_fbr->delegate(h, NULL);
	}
	int operator () (loop::timer_handle t) {
		int r = callback_runner(*this, t);
		return (r >= 0 || r == NBR_EPENDING || r == NBR_ECANCEL) ? NBR_OK : r;
	}
};
