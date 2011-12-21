/******************************************************************************************/
/* future */
struct future {
	lua::coroutine *m_co;
	yue::yielded_context m_y;
	U8 m_state, padd[3];
	enum {
		INIT,
		TIMED_INIT,
		SET_CALLBACK,
		SET_TIMED_CALLBACK,
		RECV_RESPONSE,
		RECV_TIMED_RESPONSE,
		CALLBACKED,
		FINISH,
	};
public:	/* lua_Function */
	static inline int callback(VM vm) {
		/* first operator () called already? (that is, already remote peer respond?) */
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		int top = lua_gettop(ft->m_co->vm());
		switch(ft->m_state) {
		case INIT:
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_CALLBACK);
			lua_settop(ft->m_co->vm(), 0);	/* reset stack (because sent arg still on stack) */
			lua_xmove(vm, ft->m_co->vm(), 1);
			//lua::dump_stack(ft->m_co->vm());
			break;
		case TIMED_INIT:
			/* TODO: when rpc command not sent, should we put function value to upvalue or metatable? */
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_TIMED_CALLBACK);
			lua_settop(ft->m_co->vm(), 0);	/* reset stack (because sent arg still on stack) */
			lua_xmove(vm, ft->m_co->vm(), 1);
			break;
		case RECV_RESPONSE:
			lua_xmove(vm, ft->m_co->vm(), 1);
			/* if top > 0, it means RPC call (and thus already sent args are rewinded)
			 * already_respond, current stack order is,
			 * [respond arg1],[respond arg2],...,[respond argN],[callback func]
			 * so, we need to put [callback func] on the top of stack
			 * [callback func],[respond arg1],...,[respond argN].
			 * fortunately, we have lua_insert API which exactly do as above. */
			lua_insert(ft->m_co->vm(), 1);
			/* NOTE: in that case, callback called before invoking future.will finished. */
			int r;
			if ((r = lua_resume(ft->m_co->vm(), top)) == LUA_YIELD) {
				ft->set_status(CALLBACKED);
			}
			else if (r != 0) {	/* finished but error */
				lua_pushstring(vm, lua_tostring(ft->m_co->vm(), -1));
				ft->fin(); lua_error(vm);
			}
			else {	/* successfully finished */
				ft->fin();
			}
			break;
		case RECV_TIMED_RESPONSE:
			for (; top > 1; top--) {
				if (fiber::exec_error == ft->pop_obj_and_run_fiber(vm, -1)) {
					lua_error(vm);
				}
				ASSERT(lua_isfunction(vm, 1));
			}
			lua_settop(ft->m_co->vm(), 0);
			lua_xmove(vm, ft->m_co->vm(), 1);
			break;
		default:
			lua_pushfstring(vm, "invalid state: %u", ft->m_state);
			ft->fin(); lua_error(vm);
			break;
		}
		return 0;
	}
	static future *init(VM vm, lua &ll, bool timed) {
		int top = lua_gettop(vm);
		future *ft = reinterpret_cast<future *>(
			lua_newuserdata(vm, sizeof(future))
		);
		lua_getglobal(vm, lua::future_metatable);
		lua_setmetatable(vm, -2);

		/* create coroutine to execute callback */
		fiber::handler h(*ft);
		ft->m_y.set(h);
		if (timed) {
			ft->m_state = TIMED_INIT;
			/* block removed from yield list */
			ft->m_y.set_removable(false);
		}
		else {
			ft->m_state = INIT;
		}
		lua_error_check(vm, (ft->m_co = ll.create(&(ft->m_y))), "create coroutine");

		/* prevent top object (future *ft) from GC-ing */
		ASSERT(lua_isuserdata(vm, -1) && ft == lua_touserdata(vm, -1));
		lua::refer(vm, ft);
		//lua::refer(ft->m_co->vm(), ft) is not work because m_co->vm() has not resumed yet.
		/* future shift to bottom of stack
			(others copy into coroutine which created by future as above) */
		lua_insert(vm, 1);

		/* copy rpc args to ft->m_co (and it removes after
		 * 1. future.callback is called
		 * 2. rpc response received) */
		lua_xmove(vm, ft->m_co->vm(), top);

		//TRACE("=========== future ptr = %p\n", ft);
		return ft;
	}
	static int gc(VM vm) {
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		TRACE("lua future::gc:%p\n", ft);
		return 0;
	}
public:	/* internal methods */
	inline void set_status(int newstate) {
		if (m_state != FINISH) {
			m_state = newstate;
		}
	}
	inline int run_fiber(VM vm, object &o) {
		PROCEDURE(callproc) *p = new (o) PROCEDURE(callproc)(c_nil(), o);
		if (!p) { ASSERT(false); return fiber::exec_error; }
		int r = p->rval().init(*(m_co->ll().attached()), p);
		if (r < 0) { ASSERT(false); return fiber::exec_error; }
		switch((r = p->rval().co()->resume(vm, o))) {
		case fiber::exec_error:	/* unrecoverable error happen */
			lua_xmove(vm, p->rval().co()->vm(), 1);	/* copy error into caller VM */
			p->fin(true); break;
		case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
			p->fin(false); break;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			break;
		default:
			ASSERT(false);
			return fiber::exec_error;
		}
		return r;
	}
	inline int pop_obj_and_run_fiber(VM cb_vm, int cb_index) {
		VM vm = m_co->vm();
		object *pargs = reinterpret_cast<object *>(
			lua_touserdata(vm, -1)
		);
		if (!pargs) { ASSERT(false); return fiber::exec_error; }
		/* after below copy, pargs is ok to free its memory
		 * because sbuf which belongs pargs move into args. */
		object args = *pargs;
		/* so it can be popped from stack. */
		lua_pop(vm, 1);
		lua_pushvalue(cb_vm, cb_index);	/* copy callback function to top */
		int r = run_fiber(cb_vm, args);
		/* then args is no more necessary, so free sbuf inside of it */
		args.fin();
		return r;
	}
	inline int store_response_on_stack(object &response) {
		VM vm = m_co->vm();
		int r, top = lua_gettop(vm);
		object *packed = reinterpret_cast<object *>(
			lua_newuserdata(vm, sizeof(object))
		);
		lua_pushboolean(vm, !response.is_error());
		if ((r = m_co->unpack_response_to_stack(response)) < 0) {
			/* callback with error */
			lua_pop(vm, 1);
			lua_pushboolean(vm, false);
			lua_pushstring(vm, "response unpack error");
		}
		/* pack stack except object * on stack bottom */
		if ((r = lua::coroutine::get_object_from_stack(vm, top + 2, *packed)) < 0) {
			ASSERT(false);
			return r;
		}
		/* shrink object */
		lua_settop(vm, top + 1);
		return NBR_OK;
	}
	inline int resume(fabric &f, object &o) {
		int r;
		ASSERT(o.is_response());
		if (m_co->ll().attached() == &f) {
			/* 1) future::callback() not called yet.
				if so, after will called, immediately start coroutine.
			 * 			(see implementation of future::will())
			 * 2) already future::will() called 
				and lua function (or cfunction) put on top of stack.*/
			switch(m_state) {
			case INIT:
				TRACE("resume: reset stack\n");
				lua_settop(m_co->vm(), 0);
				lua_pushboolean(m_co->vm(), !o.is_error());
				if (m_co->unpack_response_to_stack(o) < 0) { /* case 1) */
					/* callback with error */
					lua_pop(m_co->vm(), 1);
					lua_pushboolean(m_co->vm(), false);
					lua_pushstring(m_co->vm(), "response unpack error");
				}
				set_status(RECV_RESPONSE);
				return fiber::exec_yield;
			case TIMED_INIT: {
				TRACE("resume: reset stack\n");
				lua_settop(m_co->vm(), 0);
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				set_status(RECV_TIMED_RESPONSE);
				return fiber::exec_yield;
			}
			case SET_CALLBACK: {
				//lua::dump_stack(m_co->vm());
				ASSERT(lua_isfunction(m_co->vm(), 1));
				lua_pushboolean(m_co->vm(), !o.is_error());
				if ((r = m_co->unpack_response_to_stack(o)) < 0) {
					/* callback with error */
					lua_pop(m_co->vm(), 1);
					lua_pushboolean(m_co->vm(), false);
					lua_pushstring(m_co->vm(), "response unpack error");
				}
				set_status(CALLBACKED);
				return m_co->resume(r + 1);/* case 2) */
			}
			case SET_TIMED_CALLBACK: {
				ASSERT(lua_gettop(m_co->vm()) == 1);
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				/* create new fiber and run */
				r = pop_obj_and_run_fiber(m_co->vm(), 1);
				ASSERT( (
							r == fiber::exec_finish &&
							lua_gettop(m_co->vm()) == 1 &&
							lua_isfunction(m_co->vm(), 1)
						)
						|| (r != fiber::exec_finish));
				return r;
			}
			case RECV_TIMED_RESPONSE: {
				if ((r = store_response_on_stack(o)) < 0) {
					return fiber::exec_error;
				}
				break;
			}
			case CALLBACKED: {
				return m_co->resume(o);
			}
			default:
				ASSERT(false);
				break;
			}
		}
		else {
			/* we can assume future object exists until processed by delegated thread.
			 * because in this timing, this object removed from fabric::yielded_fibers,
			 * so never checked timeout and also no way to remove future object by user. */
			fiber_handler fh(*this);
			return m_co->ll().attached()->delegate(fh, o);
		}
		return fiber::exec_error;
	}
	int operator () (fabric &f, object &o) {
		switch(resume(f, o)) {
		case fiber::exec_error:	/* unrecoverable error happen */
		case fiber::exec_finish: 	/* procedure finish (it should reply to caller actor) */
			fin();
			return NBR_OK;
		case fiber::exec_yield: 	/* procedure yields. (will invoke again) */
		case fiber::exec_delegate:	/* fiber send to another native thread. */
			return NBR_OK;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	}
	void fin() {
		//TRACE("===========================future %p called fin\n", this);
		if (!m_y.removable()) {
			fabric::yielded_fibers().erase(m_y.msgid());
		}
		m_state = FINISH;
		lua::unref(m_co->vm(), this);
		if (m_co) {
			m_co->ll().destroy(m_co);
		}
	}
};
