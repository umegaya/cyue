/******************************************************************************************/
/* future */
struct future {
	yue::fiber *m_f;
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
	inline lua::coroutine *co() { return m_f->co(); }
public:	/* lua_Function */
	static inline int callback(VM vm) {
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		int top = lua_gettop(ft->co()->vm());
		switch(ft->m_state) {
		case INIT:
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_CALLBACK);
			ASSERT(lua_gettop(ft->co()->vm()) == 1);
			lua_xmove(vm, ft->co()->vm(), 1);
			//lua::dump_stack(ft->co()->vm());
			break;
		case TIMED_INIT:
			/* TODO: when rpc command not sent, should we put function value to upvalue or metatable? */
			TRACE("setcallback: reset stack\n");
			ft->set_status(SET_TIMED_CALLBACK);
			ASSERT(lua_gettop(ft->co()->vm()) == 1);
			lua_xmove(vm, ft->co()->vm(), 1);
			break;
		case RECV_RESPONSE:
			lua_xmove(vm, ft->co()->vm(), 1);
			/* now RPC call already dispatched (and thus already sent args are rewinded),
			 * so current stack order is,
			 * [future::resume] [respond arg1],[respond arg2],...,[respond argN],[callback func]
			 * so, we need to put [callback func] on the top of stack
			 * [future::resume] [callback func],[respond arg1],...,[respond argN].
			 * fortunately, we have lua_insert API which exactly do as above. */
			lua_insert(ft->co()->vm(), 2);
			/* NOTE: in that case, callback called before invoking future.will finished. */
			int r;
			if ((r = lua_resume(ft->co()->vm(), top)) == LUA_YIELD) {
				ft->set_status(CALLBACKED);
			}
			else if (r != 0) {	/* finished but error */
				lua_pushstring(vm, lua_tostring(ft->co()->vm(), -1));
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
			lua_xmove(vm, ft->co()->vm(), 1);
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
		/* create future object and set metatable */
		future *ft = reinterpret_cast<future *>(
			lua_newuserdata(vm, sizeof(future))
		);
		lua_getglobal(vm, lua::future_metatable);
		lua_setmetatable(vm, -2);

		/* create coroutine to execute callback */
		lua_error_check(vm, (ft->m_f = ll.attached()->create()), "create fiber");
		ft->co()->set_flag(lua::coroutine::FLAG_HANDLER, true);
		if (timed) {
			ft->m_state = TIMED_INIT;
			/* block removed from yield list */
			ft->m_f->set_removable(false);
		}
		else {
			ft->m_state = INIT;
		}

		/* prevent top object (future *ft) from GC-ing by referring like reg[ft->m_f] = ft*/
		/* lua::refer(ft->co()->vm(), ft) is not work because m_co->vm() has not resumed yet.*/
		ASSERT(lua_isuserdata(vm, -1) && ft == lua_touserdata(vm, -1));
		lua::refer(vm, ft->m_f);

		/* future shift to bottom of stack
			(others copy into coroutine which created by future as below) */
		lua_insert(vm, 1);

		/* set future handler for main coroutine (thus, when resume, future::resume caled) */
		lua_pushcfunction(ft->co()->vm(), future::resume);

		/* copy rpc args to ft->co() (and it removes after
		 * 1. future.callback is called => now return future always after rpc command is sent to server.
		 * 2. rpc response received) */
		lua_xmove(vm, ft->co()->vm(), top);

		/* now stack args are [future::resume] [rpc arg1], ..., [rpc argN] */
		ASSERT(lua_gettop(ft->co()->vm()) == (top + 1));

		return ft;
	}
	static int gc(VM vm) {
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, 1)
		);
		TRACE("lua future::gc:%p\n", ft);
		ft->fin();
		return 0;
	}
public:	/* internal methods */
	inline void set_status(int newstate) {
		if (m_state != FINISH) {
			m_state = newstate;
		}
	}
	static inline int run_fiber(VM vm, object &o) {
		fiber *fb = fabric::tlf().create();
		if (!fb) { ASSERT(false); return fiber::exec_error; }
		return fb->respond(fb->co()->resume(vm, o));
	}
	static inline int run_fiber(VM vm) {
		fiber *fb = fabric::tlf().create();
		if (!fb) { ASSERT(false); return fiber::exec_error; }
		return fb->respond(fb->co()->resume(vm));
	}
	inline int pop_obj_and_run_fiber(VM cb_vm, int cb_index) {
		VM vm = co()->vm();
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
	inline int store_response_on_stack() {
		VM vm = co()->vm();
		int r, top = lua_gettop(vm);
		object *packed = reinterpret_cast<object *>(
			lua_newuserdata(vm, sizeof(object))
		);
		/* pack stack except object * on stack bottom */
		if ((r = lua::coroutine::get_object_from_stack(vm, top + 2, *packed)) < 0) {
			ASSERT(false);
			return r;
		}
		/* shrink stack */
		lua_settop(vm, top + 1);
		return NBR_OK;
	}
	static int resume(VM vm) {
		int r;
		lua::coroutine *co = lua::coroutine::to_co(vm);
		lua_error_check(vm, co, "to_co");
		lua::fetch_ref(vm, co->fb());	/* fetch referred future object */
		future *ft = reinterpret_cast<future *>(
			lua_touserdata(vm, -1)
		);
		/* 1) future::callback() not called yet.
			if so, after it has called, immediately start coroutine.
		 * 			(see implementation of future::callback())
		 * 2) already future::will() called 
			and lua function (or cfunction) put on top of stack.*/
		switch(ft->m_state) {
		case INIT:
			/* when reach here, vm already contains rpc response on the stack. */
			ft->set_status(RECV_RESPONSE);
			return co->yield();	/* wait for set callback */
		case TIMED_INIT: {
			/* vm stack layout is same as INIT, so just pack it as object. */
			if ((r = ft->store_response_on_stack()) < 0) {
				return fiber::exec_error;
			}
			ft->set_status(RECV_TIMED_RESPONSE);
			return co->yield();
		}
		case SET_CALLBACK:
			//lua::dump_stack(co->vm());
			ft->set_status(CALLBACKED);
			/* fall through */
		case CALLBACKED: {
			/* -1 means callee function on stack top */
			/* this coroutine calls callback function by itself. so we provide co->vm() to lua_call */
			lua_call(co->vm(), lua_gettop(co->vm()) - 1, LUA_MULTRET);/* case 2) */
			return (lua_gettop(co->vm()) - 1);	/* if not resume or error happen, return rval of callee function */
		}
		case SET_TIMED_CALLBACK: {
			ASSERT(lua_gettop(co->vm()) > 1);
			/* create new coroutine and copy args to it then run new fiber */
			r = run_fiber(co->vm());
			if (r == fiber::exec_error) {
				lua_error(co->vm());
			}
			return co->yield();	//wait next response
		}
		case RECV_TIMED_RESPONSE: {
			if ((r = ft->store_response_on_stack()) < 0) {
				return fiber::exec_error;
			}
			return co->yield();	//wait next response
		}
		default:
			ASSERT(false);
			break;
		}
		return fiber::exec_error;
	}
	void fin() {
		fabric::yielded_fibers().erase(m_f->msgid());
		m_state = FINISH;
		lua::unref(m_f->co()->vm(), m_f);
		m_f->fin();
	}
};
