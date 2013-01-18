/***************************************************************
 * yue.lua.h : API header
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for detail
 ****************************************************************/
#if !defined(__YUE_LUA_H__)
#define __YUE_LUA_H__
#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__ANDROID_NDK__)
#define __USE_BUILTIN_LUA_HEADER__
#endif

/* lua jit definition */
#if defined(__USE_BUILTIN_LUA_HEADER__)
#include "exlib/luajit/src/lua.h"
#include "exlib/luajit/src/lauxlib.h"
#include "exlib/luajit/src/lualib.h"
#else
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#endif
#define lua_newcthread(VM, SZ) lua_newthread((VM))
#define lua_newvm(ALLOCATOR,PARAM)	luaL_newstate()
extern int luaopen_ffi(lua_State *);

/* yue.so entry point */
extern int luaopen_libyue(lua_State *vm);

/* yue fiber APIs */
extern lua_State *yue_state();
extern void yue_poll();
typedef void *yue_Fiber;
typedef int (*yue_FiberCallback)(yue_Fiber, int);
extern yue_Fiber yue_newfiber(yue_FiberCallback cb);
extern void yue_deletefiber(yue_Fiber fb);
extern lua_State *yue_getstate(yue_Fiber fb);
extern int yue_run(yue_Fiber fb, int n_args);

/* for implementing user data pack/unpack */
typedef void *yue_Wbuf, *yue_Rbuf;
extern int yueb_write(yue_Wbuf yb, const void *p, int sz);
extern const void *yueb_read(yue_Rbuf yb, int *sz);

/* ffi APIs */
typedef void *emitter_t;
typedef struct lua_State *vm_t;
typedef struct {
	int wblen, rblen;
	int timeout;
} option_t;
extern emitter_t yue_emitter_new();
extern emitter_t yue_emitter_bind(vm_t vm, emitter_t p);
extern emitter_t yue_listener_new(const char *addr, option_t *opt);

#if defined(__cplusplus)
}
#endif
#endif
