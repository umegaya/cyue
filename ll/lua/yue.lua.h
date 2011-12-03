/***************************************************************
 * yue.lua.h : API header
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
#if !defined(__YUE_LUA_H__)
#define __YUE_LUA_H__
#if defined(__cplusplus)
extern "C" {
#endif
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#if 0 && defined(_DEBUG) /* lua 5.1.4 + coco */
extern lua_State *lua_newcthread(lua_State *L, int cstacksize);
extern lua_State *lua_newvm(void (*)(void*, void*, size_t, size_t),void *);
#else	/* luajit */
#define lua_newcthread(VM, SZ) lua_newthread((VM))
#define lua_newvm(ALLOCATOR,PARAM)	luaL_newstate()
extern int luaopen_ffi(lua_State *);
#endif
extern int luaopen_yue(lua_State *vm);
extern void yue_poll();
#if !defined(__USE_OLD_FIBER)
typedef void *yue_Fiber;
#else
typedef lua_State *yue_Fiber;
#endif
typedef int (*yue_FiberCallback)(yue_Fiber, bool);
extern yue_Fiber yue_newfiber(yue_FiberCallback cb);
extern void yue_deletefiber(yue_Fiber fb);
extern lua_State *yue_getstate(yue_Fiber fb);
extern int yue_run(yue_Fiber fb, int n_args);
/* for implementing user data pack/unpack */
typedef void *yue_Wbuf, *yue_Rbuf;
extern int yueb_write(yue_Wbuf yb, const void *p, int sz);
extern const void *yueb_read(yue_Rbuf yb, int *sz);
#if defined(__cplusplus)
}
#endif
#endif
