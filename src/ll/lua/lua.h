/***************************************************************
 * lua.h : lua VM wrapper
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of pfm framework.
 * pfm framework is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * pfm framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__LUAVM_H__)
#define __LUAVM_H__

#include "common.h"
#include "map.h"
#include "serializer.h"
#include "functional.h"
#include "loop.h"
#include "constant.h"
#include "session.h"
#include "runner.h"
#include <memory.h>

#include "yue.lua.h"

namespace yue {
class fabric;
class fiber;
class server;
struct yielded_context;
struct fiber_context;
typedef lua_State *VM;
namespace module {
namespace ll {
class lua {
public:	/* type & constant */
	typedef yue::serializer serializer;
	typedef yue::object object;
	typedef yue::handler::session session;
	static const char kernel_table[];
	static const char index_method[];
	static const char newindex_method[];
	static const char call_method[];
	static const char gc_method[];
	static const char len_method[];
	static const char pack_method[];
	static const char unpack_method[];
	static const char actor_metatable[];
	static const char rmnode_metatable[];
	static const char rmnode_sync_metatable[];
	static const char thread_metatable[];
	static const char future_metatable[];
	static const char error_metatable[];
	static const char sock_metatable[];
	static const char module_name[];
	static const char ldname[];
	static const char watcher[];
	static const char tick_callback[];
	enum {	/* packed object type (sync with yue.lua) */
		YUE_OBJECT_METHOD = 1,
		YUE_OBJECT_ACTOR = 2,
	};
public:
	enum {
		RPC_MODE_NORMAL,
		RPC_MODE_SYNC,
		RPC_MODE_PROTECTED,
	};
	static U32 ms_mode;
	static inline bool normal_mode() { return ms_mode == RPC_MODE_NORMAL; }
	static inline bool sync_mode() { return ms_mode == RPC_MODE_SYNC; }
	static inline bool protect_mode() { return ms_mode == RPC_MODE_PROTECTED; }
public:	/* userdatas */
	typedef yue::util::functional<int (char *, int, bool)> userdata;
	struct module {
		//static class server *m_server;
		//static class server *served() { return m_server; }
		typedef class server served;
		static void init(VM vm, class server *srv);
		static int index(VM vm);
		static int connect(VM vm);
		static int poll(VM vm);
		static int stop(VM vm);
		static int newthread(VM vm);
		static int resume(VM vm);
		static int exit(VM vm);
		static int yield(VM vm);
		static int peer(VM vm);
		static int mode(VM vm);
		static int timer(VM vm);
		static int stop_timer(VM vm);
		static int accepted(VM vm);
		static int listeners(VM vm);
		static int sleep(VM vm);
		static int listen(VM vm);
		static int configure(VM vm);
		static int registry(VM co, VM vm) {
			lua_pushlightuserdata(co, vm);
			lua_gettable(co, LUA_REGISTRYINDEX);
			return 1;
		}
		static int error(VM vm) {
			lua_getmetatable(vm, -1);
			lua_getglobal(vm, lua::error_metatable);
			bool r = (lua_topointer(vm, -1) == lua_topointer(vm, -2));
			lua_pop(vm, 2);
			lua_pushboolean(vm, r);
			return 1;
		}
		static int read(VM vm) {
			yue_Rbuf rb = lua_touserdata(vm, 1);
			int size; const void *p = yueb_read(rb, &size);
			lua_pushlightuserdata(vm, const_cast<void *>(p));
			lua_pushnumber(vm, size);
			return 2;
		}
		static int write(VM vm) {
			yue_Wbuf wb = lua_touserdata(vm, 1);
			void *p = lua_touserdata(vm, 2);
			int size = lua_tointeger(vm, 3), r;
			if ((r = yueb_write(wb, p, size)) < 0) {
				lua_pushfstring(vm, "yueb_write error : %d, %d", r, size);
				lua_error(vm);
			}
			return 0;
		}
		static int command_line(VM vm);
		static int nop(VM) {return 0;}
	};
	struct accept_watcher : public session_delegator::impl<accept_watcher> {
		class fabric *attached();
		void set_watcher(session::watcher *) {}
		int setup(VM vm, session_delegator::args &a);
		bool operator () (session *s, int st);
	};
	struct actor : public session_delegator::impl<actor> {
		enum {
			RMNODE,
			THREAD,
		};
		union {
			server *m_la;
			struct {
				session *m_s;
				session::handler_serial m_sn;
				inline bool valid() const {
					return (!m_s->finalized()) && m_s->serial() == m_sn;
				}
			} m_rm;
		};
		U8 m_kind, padd[3];
		class lua *m_ll;
		session::watcher *m_w;
		int kind() const { return m_kind; }
		void set_watcher(session::watcher *w) { m_w = w; }
		class lua &ll() { return *m_ll; }
		class fabric *attached();
		int setup(VM vm, session_delegator::args &a);
		template <class CHANNEL> static inline int init(VM vm, CHANNEL c) {
			actor *a = reinterpret_cast<actor *>(
				lua_newuserdata(vm, sizeof(actor))
			);
			/* make this userdata searchable from pointer value */
			lua::refer(vm, a);
#if defined(_DEBUG)
			lua_pushlightuserdata(vm, a);
			lua_gettable(vm, LUA_REGISTRYINDEX);
			actor *aa = (actor *)lua_touserdata(vm, -1);
			ASSERT(aa == a);
			lua_pop(vm, 1);	//remove aa (it is for test
#endif
			/* meta table */
			//lua_getglobal(vm, lua::actor_metatable);
			init_metatable(vm);
			lua_setmetatable(vm, -2);
			if (!a->set(c)) {
				/* cannot create actor. return null instead. */
				lua_pushnil(vm);
			}
			addr(vm);	//store address to table
			lua_setfield(vm, -2, "__addr");
			return 1;
		}
		static int init_metatable(VM vm) {
			lua_newtable(vm);
			lua_pushcfunction(vm, index);
			lua_setfield(vm, -2, index_method);
			lua_pushvalue(vm, -1);	/* use metatable itself as newindex */
			lua_setfield(vm, -2, newindex_method);
			lua_pushcfunction(vm, close);
			lua_setfield(vm, -2, "__close");
			lua_pushcfunction(vm, permit_access);
			lua_setfield(vm, -2, "__permit_access");
			lua_pushcfunction(vm, fd);
			lua_setfield(vm, -2, "__fd");
			lua_pushcfunction(vm, fin);
			lua_setfield(vm, -2, "__fin");
			lua_pushcfunction(vm, gc);
			lua_setfield(vm, -2, gc_method);
			lua_pushcfunction(vm, actor::pack);
			lua_setfield(vm, -2, lua::pack_method);
			return 1;
		}
		static int index(VM vm);
		static int gc(VM vm) {
			TRACE("GC:%p\n", lua_touserdata(vm, -1));
			return 0;
		}
		static int fin(VM vm);
		static int fd(VM vm);
		static int addr(VM vm);
		static int close(VM vm);
		static int permit_access(VM vm);
		static int pack(VM vm) {
			actor *a = reinterpret_cast<actor *>(
				lua_touserdata(vm, 1)
			);
			if (a->m_kind != RMNODE) {
				lua_pushfstring(vm, 
					"actor type %d cannot be rpc param", a->m_kind);
				lua_error(vm);
			}
			yue_Wbuf wb = lua_touserdata(vm, 2);
			U8 b = YUE_OBJECT_ACTOR;
			yueb_write(wb, &b, sizeof(b));
			char buff[1024]; 
			session *s = a->get_session();
			const char *p = s ? s->addr(buff, sizeof(buff)) : NULL;
			if (!p) {
				lua_pushstring(vm, "invalid address");
				lua_error(vm);
			}
			yueb_write(wb, p, util::str::length(p));
			lua_pushstring(vm, "yue");
			return 1;
		}
		static int push_address_from(VM vm, yielded_context *y);
	public:
		bool set(server *la);
		bool set(session *s);
		session *get_session() {
			return (m_kind == RMNODE && m_rm.valid()) ? m_rm.m_s : NULL;
		}
	public:
		bool operator () (session *s, int state);
		int operator () (fabric &fbr, void *p);
	private:
		actor(const actor &a);
	};
	struct method {
		enum {
			NOTIFICATION 	= 0x00000001,
			CLIENT_CALL 	= 0x00000002,
			TRANSACTIONAL 	= 0x00000004,
			QUORUM 			= 0x00000008,
			TIMED 			= 0x00000010,

			ALLOCED			= 0x10000000,
		};
		static char prefix_NOTIFICATION[];
		static char prefix_CLIENT_CALL[];
		static char prefix_TRANSACTIONAL[];
		static char prefix_QUORUM[];
		static char prefix_TIMED[];
	public:
		actor *m_a;
		const char *m_name;
		U32 m_attr;
		static int init(VM vm, actor *a, const char *name, method *parent);
		static int init_metatable(VM vm, int (*call_fn)(VM)) {
			lua_newtable(vm);
			lua_pushcfunction(vm, call_fn);
			lua_setfield(vm, -2, lua::call_method);
			lua_pushcfunction(vm, method::gc);
			lua_setfield(vm, -2, lua::gc_method);
			lua_pushcfunction(vm, method::index);
			lua_setfield(vm, -2, lua::index_method);
			lua_pushcfunction(vm, method::pack);
			lua_setfield(vm, -2, lua::pack_method);
			return NBR_OK;
		}
		template <class CH> static int call(VM vm);
		static int sync_call(VM vm);
		static int pack(VM vm) {
			method *m = reinterpret_cast<method *>(
				lua_touserdata(vm, 1)
			);
			yue_Wbuf wb = lua_touserdata(vm, 2);
			U8 b = YUE_OBJECT_METHOD;
			yueb_write(wb, &b, sizeof(b));
			yueb_write(wb, m->m_name, util::str::length(m->m_name));
			lua_pushstring(vm, "yue");
			return 1;
		}
		static int index(VM vm);
		static int gc(VM vm);
	public:
		static const char *parse(const char *name, U32 &attr);
		inline bool notification() const { return m_attr & NOTIFICATION; }
		inline bool client_call() const { return m_attr & CLIENT_CALL; }
		inline bool transactional() const { return m_attr & TRANSACTIONAL; }
		inline bool quorum() const { return m_attr & QUORUM; }
		inline bool timed() const { return m_attr & TIMED; }
		inline bool has_future() const { return (m_attr & NOTIFICATION); }
	};
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
public:
	class coroutine {
	friend class lua;
	friend struct sock;
	friend struct future;
	protected:
		VM m_exec;
		yielded_context *m_y;
		class lua *m_ll;
		U32 m_flag;
		session::watcher *m_w;
	public:
		coroutine() : m_y(NULL), m_ll(NULL), m_flag(0), m_w(NULL) {}
		coroutine(VM exec) : m_exec(exec), m_y(NULL), m_ll(NULL), m_w(NULL) {}
		~coroutine() {}
		int init(yielded_context *y, lua *scr);
		void fin();
		void fin_with_context(int result);
		void free();
		void set_watcher(session::watcher *w) { m_w = w; }
		static inline coroutine *to_co(VM vm);
		inline int resume(const object &o, const fiber_context &c) {
			int r = unpack_request_to_stack(o, c);
			return r < 0 ? constant::fiber::exec_error : resume(r);
		}
		inline int resume(const object &o) {
			int r = unpack_response_to_stack(o);
			return r < 0 ? constant::fiber::exec_error : resume(r);
		}
		inline int resume(VM main_co, const object &o) {
			int r = unpack_stack_with_vm(main_co, o);
			return r < 0 ? constant::fiber::exec_error : resume(r);
		}
		inline int resume(VM main_co) {
			int r = unpack_stack_with_vm(main_co);
			return r < 0 ? constant::fiber::exec_error : resume(r);
		}
		int resume(int r);
		/* lua_gettop means all value on m_exec keeps after exit lua_resume()  */
		inline int yield() { return lua_yield(m_exec, lua_gettop(m_exec)); }
	public:
		int pack_stack_as_rpc_args(serializer &sr);
		inline int pack_error(serializer &sr) { return pack_stack(m_exec, sr, lua_gettop(m_exec)); }
		inline int pack_response(serializer &sr) { return pack_stack_as_response(m_exec, 1, sr); }
		int unpack_request_to_stack(const object &o, const fiber_context &c);
		int unpack_response_to_stack(const object &o);
		int unpack_stack_with_vm(VM main_co, const data &o);
		int unpack_stack_with_vm(VM main_co);
	public:
		enum {
			FLAG_EXIT = 0x1,
			FLAG_CONNECT_RAW_SOCK = 0x2,
			FLAG_READ_RAW_SOCK = 0x4,
		};
		inline void set_flag(U32 flag, bool on) {
			if (on) { m_flag |= flag; } else { m_flag &= (~(flag)); }
		}
		inline bool has_flag(U32 flag) { return (m_flag & flag); }
	public:
		inline class lua &ll() { return *m_ll; }
		inline yielded_context *yldc(){ return m_y; }
		inline VM vm() { return m_exec; }
	public:
		bool operator () (session *s, int state);
		int operator () (fabric &fbr, void *p);
		inline int operator () (loop::timer_handle t) {
			int r = resume(0);
			if (r == constant::fiber::exec_error ||
				r == constant::fiber::exec_finish) {
				fin_with_context(r);
			}
			return NBR_ETIMEOUT;
		}
		inline void refer();
		inline void unref();
	protected:
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
	inline coroutine *create(yielded_context *y) {
		coroutine *co = m_pool.alloc();
		if (!co) { return NULL; }
		if (co->init(y, this) < 0) {
			destroy(co); return NULL;
		}
		return co;
	}
	inline void destroy(coroutine *co) {
		if (lua_status(co->vm()) != 0) {
			TRACE("co_destroy: abnormal status %d\n", lua_status(co->vm()));
			co->fin();//thread yields or end with error
		}
		m_pool.free(co);
	}
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
protected:
	VM m_vm;
	class fabric *m_attached;
	array<coroutine> m_pool;
	array<userdata> m_alloc;
	static const size_t smblock_size = 64;
	array<char[smblock_size]> m_smp;
	static accept_watcher m_w;
public:
	lua() : m_vm(NULL), m_attached(NULL), m_pool(), m_alloc() {}
	~lua() { fin(); }
	static int static_init();
	static void static_fin();
	int init(const char *bootstrap, int max_rpc_ongoing);
	void fin();
	void tick(loop::timer_handle t);
	array<char[smblock_size]> &smpool() { return m_smp; }
	inline VM vm() { return m_vm; }
	inline class fabric *attached() const { return m_attached; }
	int load_module(const char *srcfile);
	static const char *bootstrap_source() { return NULL; }
protected:
	/* lua hook */
	static int panic(VM vm);
	static void *allocator(void *ud, void *ptr, size_t os, size_t ns);
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
