/***************************************************************
 * lua.h : lua VM wrapper
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license detail
 ****************************************************************/
#if !defined(__LUAVM_H__)
#define __LUAVM_H__

#include "common.h"
#include "serializer.h"
#include "loop.h"
#include "event.h"
#include "error.h"
#include "constant.h"
#include "socket.h"

#include "yue.lua.h"

#define lua_error_check(vm, cond, ...)	if (!(cond)) {				\
	char __b[256]; snprintf(__b, sizeof(__b), __VA_ARGS__);			\
	lua_pushfstring(vm, "error %s(%d):%s", __FILE__, __LINE__, __b);\
	lua_error(vm);													\
}

namespace yue {
class fiber;
class fabric;
class server;
namespace module {
namespace ll {
typedef lua_State *VM;
class lua {
public:	/* type & constant */
	typedef yue::serializer serializer;
	typedef yue::fiber fiber;
	typedef yue::fabric fabric;
	typedef yue::util::pbuf pbuf;
	static const char namespaces[];
	static const char objects[];
	static const char index_method[];
	static const char newindex_method[];
	static const char call_method[];
	static const char gc_method[];
	static const char pack_method[];
	static const char unpack_method[];
	static const char bootstrap_source[];
	static const char finalizer[];
public:
	class coroutine {
	friend class lua;
	protected:
		VM m_exec;
		U32 m_flag;
		int init(lua *scr);
		void fin();
		void fin_with_context(int result);
		void free(); //in case no error, not destroy m_exec to reuse.
	public:
		static const char symbol_tick[];
		static const char symbol_signal[];
		static const char symbol_accept[];
		static const char symbol_join[];
		static const char *symbol_socket[];
	public:
		coroutine() : m_flag(0) {} //dont initialize m_exec
		coroutine(VM exec) : m_exec(exec), m_flag(0) {}
		~coroutine() {}
		static inline coroutine *to_co(VM vm);
		inline int operator () (loop::timer_handle t);
	public:	/* event handlers (emit) */
		inline int start(event::session &ev);
		inline int start(event::proc &ev);
		inline int start(event::emit &ev);
		inline int start(event::timer &ev);
		inline int start(event::signal &ev);
		inline int start(event::listener &ev);
		inline int start(event::fs &ev);
		inline int start(event::thread &ev);
	public:	/* event handlers (resume) */
		inline int resume(event::session &ev);
		inline int resume(event::proc &ev);
		inline int resume(event::emit &ev);
		inline int resume(event::timer &ev);
		inline int resume(event::signal &ev);
		inline int resume(event::listener &ev);
		inline int resume(event::fs &ev);
		inline int resume(event::thread &ev);
		inline int resume(event::error &ev);
	protected:
		template <class PROC>
		inline int load_proc(event::base &ev, PROC proc);
		inline int push_procname(VM vm, const char *proc) {
			lua_pushstring(vm, proc); 
			return NBR_OK;
		}
		inline int push_procname(VM vm, const argument &obj) {
			return unpack_stack(vm, obj); 
		}
		inline int load_object(event::base &ev);
	public: /* basic resume & yield */
		int resume(int r);
		/* lua_gettop means all value on m_exec keeps after exit lua_resume()  */
		inline int yield() { return lua_yield(m_exec, lua_gettop(m_exec)); }
	public:
		int pack_stack_as_rpc_args(serializer &sr, int start_index);
		inline int pack_error(serializer &sr) const { return pack_stack(m_exec, sr, lua_gettop(m_exec)); }
		/* coroutine::start(event::proc &) push 2 variable on stack, so packing response starts from 3. */
		inline int pack_response(serializer &sr) const { return pack_stack_as_response(m_exec, 3, sr); }
		int unpack_response_to_stack(const object &o);
	public:
		enum {
			FLAG_EXIT = 0x1,
			FLAG_HANDLER = 0x2,
		};
		inline void set_flag(U32 flag, bool on) {
			if (on) { m_flag |= flag; } else { m_flag &= (~(flag)); }
		}
		inline bool has_flag(U32 flag) { return (m_flag & flag); }
	public:
		inline fiber *fb();
		inline VM vm() { return m_exec; }
	public:
		inline void refer();
		inline void unref();
	public:
		struct args {
			coroutine *m_co;
			MSGID m_msgid;
			U32 m_start, m_timeout;
			inline args(coroutine *co, U32 start, U32 t_o = 0) : m_co(co), m_start(start), m_timeout(t_o) {}
			/* serialize */
			inline int operator () (serializer &sr, MSGID) const {
				/* array len also packed in following method
			 	 * (eg. lua can return multiple value) */
				verify_success(m_co->pack_stack_as_rpc_args(sr, m_start));
				return sr.len();
			}
			inline int pack(serializer &sr) const;
		};
	public:
		static int unpack_stack(VM vm, const argument &o);
		static int unpack_function(VM vm, const argument &o);
		static int unpack_userdata(VM vm, const argument &o);
		static int unpack_table(VM vm, const argument &o);
		static int call_custom_unpack(VM vm, const argument &o);
		static int pack_stack_as_response(VM vm, int start_id, serializer &sr);
		static int pack_stack(VM vm, serializer &sr, int stkid);
		static int pack_function(VM vm, serializer &sr, int stkid);
		static int pack_table(VM vm, serializer &sr, int stkid);
		static int call_custom_pack(VM vm, serializer &sr, int stkid);
		static int get_object_from_table(VM vm, int stkid, object &o);
		static int get_object_from_stack(VM vm, int start_id, object &o);
	private:
		coroutine(const coroutine &m);
	};
	inline coroutine *create(fiber *fb);
	inline void destroy(coroutine *co) {
		if (lua_status(co->vm()) != 0) {
			TRACE("co_destroy: abnormal status %d\n", lua_status(co->vm()));
			co->fin();//destroy lua_State (it is re-usable when coroutine finished normally)
		}
	}
public: /* refer/unref/fetch specified lua object by using C-pointer key */
	static inline void refer(VM vm, void *p) {
		lua_pushlightuserdata(vm, p); /* this is key for referred object (to be unique) */
		lua_pushvalue(vm, -2);
		lua_settable(vm, LUA_REGISTRYINDEX);
	}
	static inline void unref(VM vm, void *p) {
		/* remove refer of object from registry (so it will gced) */
		lua_pushlightuserdata(vm, p);
		lua_pushnil(vm);
		lua_settable(vm, LUA_REGISTRYINDEX);
	}
	static inline void fetch_ref(VM vm, void *p) {
		lua_pushlightuserdata(vm, p); /* this is key for referred object (to be unique) */
		lua_gettable(vm, LUA_REGISTRYINDEX);
	}
protected:	/* reader/writer */
	struct writer {
		static int callback(lua_State *, const void* p, size_t sz, void* ud) {
			TRACE("writer cb: %p %u\n", p, (U32)sz);
			pbuf *pbf = reinterpret_cast<pbuf *>(ud);
			if (pbf->reserve(sz) < 0) { return NBR_ESHORT; }
			util::mem::copy(pbf->last_p(), p, sz);
			pbf->commit(sz);
			return 0;
		}
	};
	struct reader {
		static const char *callback(lua_State *L, void *data, size_t *size) {
			argument *a = reinterpret_cast<argument *>(data);
			*size = a->len();
			return a->operator const char *();
		}
	};
protected:
	VM m_vm;
public:
	lua() : m_vm(NULL) {}
	~lua() { fin(); }
	VM vm() { return m_vm; }
	static const char *bootstrap() { return bootstrap_source; }
	static int static_init();
	static void static_fin();
	int init(const class util::app &a, server *sv);
	int init_objects_map(VM vm);
	int init_emittable_objects(VM vm, server *sv);
	int init_constants(VM vm);
	int eval(const char *code_or_file, coroutine *store_result = NULL);
	void fin();
public:
	static int peer(VM vm);
	static int poll(VM vm);
	static int alive(VM vm);
	static int finalize(VM vm);
protected:
	/* lua hook */
	static int panic(VM vm);
public:	/* debug */
	static void dump_stack(VM vm);
	static int copy_table(VM vm, int from, int to, int type);
	static void dump_table(VM vm, int index);
};

lua::coroutine *
lua::coroutine::to_co(VM vm)
{
	lua_pushthread(vm);
	lua_gettable(vm, LUA_REGISTRYINDEX);
	coroutine *co = (coroutine *)lua_touserdata(vm, -1);
	lua_pop(vm, 1);
	return co;
}
inline void
lua::coroutine::refer() {
	lua::refer(m_exec, this);
}
inline void
lua::coroutine::unref() {
	lua::unref(m_exec, this);
}

}
}
}

#endif
