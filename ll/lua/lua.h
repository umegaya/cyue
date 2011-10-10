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
#include "net.h"
#include "constant.h"
#include <memory.h>

#include "yue.lua.h"

namespace yue {
class fabric;
class fiber;
class server;
struct yielded_context;
namespace module {
namespace ll {
class lua {
public:	/* type & constant */
	typedef lua_State *VM;
	typedef yue::serializer serializer;
	typedef yue::object object;
	static const char kernel_table[];
	static const char index_method[];
	static const char newindex_method[];
	static const char call_method[];
	static const char gc_method[];
	static const char pack_method[];
	static const char unpack_method[];
	static const char unpack_module_name[];
	static const char rmnode_metatable[];
	static const char rmnode_sync_metatable[];
	static const char thread_metatable[];
	static const char future_metatable[];
	static const char module_name[];
	static bool ms_sync_mode;
public:	/* userdatas */
	typedef yue::util::functional<int (char *, int, bool)> userdata;
	struct module {
		static class server *m_server;
		static class server *served() { return m_server; }
		static void init(VM vm, class server *srv);
		static int index(VM vm);
		static int connect(VM vm);
		static int poll(VM vm);
		static int stop(VM vm);
		static int newthread(VM vm);
		static int resume(VM vm);
		static int yield(VM vm);
		static int sync_mode(VM vm);
		static int timer(VM vm) {return 0; }
		static int listen(VM vm);
	};
	struct actor {
		enum {
			RMNODE,
			THREAD,
		};
		union {
			local_actor *m_la;
			session *m_s;
		};
		U8 m_kind, padd[3];
		int kind() const { return m_kind; }
		template <class CHANNEL> static inline int init(VM vm, CHANNEL c) {
			actor *a = reinterpret_cast<actor *>(
				lua_newuserdata(vm, sizeof(actor))
			);
			a->set(c);
			/* meta table */
			lua_newtable(vm);
			lua_pushcfunction(vm, index);
			lua_setfield(vm, -2, index_method);
			lua_pushvalue(vm, -1);	/* use metatable itself as newindex */
			lua_setfield(vm, -2, newindex_method);
			lua_pushcfunction(vm, gc);
			lua_setfield(vm, -2, gc_method);
			lua_setmetatable(vm, -2);
			return 1;
		}
		static int index(VM vm);
		static int gc(VM vm);
	public:
		void set(local_actor *la) { m_la = la; m_kind = THREAD; }
		void set(session *s) { m_s = s; m_kind = RMNODE; }
	};
	struct method {
		enum {
			NOTIFICATION = 0x00000001,
			CLIENT_CALL = 0x00000002,
			TRANSACTIONAL = 0x00000004,
			QUORUM = 0x00000008,
		};
		static char prefix_NOTIFICATION[];
		static char prefix_CLIENT_CALL[];
		static char prefix_TRANSACTIONAL[];
		static char prefix_QUORUM[];
	public:
		actor *m_a;
		const char *m_name;
		U32 m_attr;
		static int init(VM vm, actor *a, const char *name);
		template <class CH> static int call(VM vm);
		static int sync_call(VM vm);
	public:
		static const char *parse(const char *name, U32 &attr);
		inline bool notification() const { return m_attr & NOTIFICATION; }
		inline bool client_call() const { return m_attr & CLIENT_CALL; }
		inline bool transactional() const { return m_attr & TRANSACTIONAL; }
		inline bool quorum() const { return m_attr & QUORUM; }
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
	protected:
		VM m_exec;
		yielded_context *m_y;
		class lua *m_ll;
	public:
		coroutine() : m_y(NULL), m_ll(NULL) {}
		coroutine(VM exec) : m_exec(exec), m_y(NULL), m_ll(NULL) {}
		~coroutine() {}
		int init(yielded_context *y, lua *scr);
		void fin();
		void free();
		static inline coroutine *to_co(VM vm);
		inline int resume(const object &o) {
			int r;
			if ((r = unpack_stack(o)) < 0) { return constant::fiber::exec_error; }
			return resume(r);
		}
		int resume(int r);
		/* lua_gettop means all value on m_exec keeps after exit lua_resume()  */
		int yield() { return lua_yield(m_exec, lua_gettop(m_exec)); }
	public:
		int pack_stack(serializer &sr);
		int pack_error(serializer &sr) { return pack_stack(m_exec, sr, lua_gettop(m_exec)); }
		int pack_response(serializer &sr) { return pack_stack(m_exec, 1, sr); }
		int unpack_stack(const object &o);
	public:
		class lua &ll() { return *m_ll; }
		yielded_context *yldc(){ return m_y; }
		VM vm() { return m_exec; }
		bool operator () (session *s, int state);
		inline void refer() {
			lua_pushlightuserdata(m_exec, this); /* this is key for referred object (to be unique) */
			lua_pushvalue(m_exec, -2);
			lua_settable(m_exec, LUA_REGISTRYINDEX);
		}
		inline void unref() {
			/* remove refer of object from registry (so it will gced) */
			lua_pushlightuserdata(m_exec, this);
			lua_pushnil(m_exec);
			lua_settable(m_exec, LUA_REGISTRYINDEX);
		}
	protected:
		static int unpack_stack(VM vm, const argument &o);
		static int unpack_function(VM vm, const argument &o);
		static int unpack_userdata(VM vm, const argument &o);
		static int pack_stack(VM vm, int start_id, serializer &sr);
		static int pack_stack(VM vm, serializer &sr, int stkid);
		static int pack_function(VM vm, serializer &sr, int stkid);
		static int pack_userdata(VM vm, serializer &sr, int stkid);
	};
	inline coroutine *create(yielded_context *y) {
		coroutine *co = m_pool.alloc();
		if (!co) { return NULL; }
		if (co->init(y, this) < 0) {
			destroy(co); return NULL;
		}
		return co;
	}
	inline void destroy(coroutine *co) { m_pool.free(co); }
protected:
	VM m_vm;
	class fabric *m_attached;
	array<coroutine> m_pool;
	array<userdata> m_alloc;
	static const size_t smblock_size = 64;
	array<char[smblock_size]> m_smp;
public:
	lua() : m_vm(NULL), m_attached(NULL), m_pool(), m_alloc() {}
	~lua() { fin(); }
	int init(const char *bootstrap, int max_rpc_ongoing);
	void fin();
	array<char[smblock_size]> &smpool() { return m_smp; }
	inline VM vm() { return m_vm; }
	inline class fabric *attached() const { return m_attached; }
	int load_module(const char *srcfile);
	static const char *bootstrap_source() { return "./ll/lua/bootstrap.lua"; }
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
}
}
}

#endif
