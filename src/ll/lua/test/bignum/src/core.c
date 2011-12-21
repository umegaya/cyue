/*
 * Lua BIGNUM
 * Copyright (C) 2007  Rodrigo Cacilhas
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/*
 * Para compilar:
 * gcc -c -fPIC -O3 -Wall *.c
 * ld -shared -lm -lssl *.o -o core.so
 * rm -f *.o
 */


#include <openssl/bn.h>
#include <openssl/dh.h>
#include <memory.h>
#include <arpa/inet.h>
#include "../../../yue.lua.h"
#include "bn_mime.h"


static void set_info(lua_State *);


/*
 * Série BN_new()
 */


static int luaBN_new(lua_State *);
static int luaBN_free(lua_State *);
static int luaBN_init(lua_State *);
static int luaBN_clear(lua_State *);
static int luaBN_clear_free(lua_State *);


/*
 * Série BN_copy()
 */


static int luaBN_copy(lua_State *);
static int luaBN_dup(lua_State *);
static int luaBN_swap(lua_State *);


/*
 * Série BN_num_bytes()
 */


static int luaBN_num_bytes(lua_State *);
static int luaBN_num_bits(lua_State *);


/*
 * Série BN_set_negative()
 */


static int luaBN_set_negative(lua_State *);
static int luaBN_is_negative(lua_State *);


/*
 * Série BN_add()
 */


static int luaBN_add(lua_State *);
static int luaBN_sub(lua_State *);
static int luaBN_mul(lua_State *);
static int luaBN_sqr(lua_State *);
static int luaBN_div(lua_State *);
static int luaBN_mod(lua_State *);
static int luaBN_nnmod(lua_State *);
static int luaBN_mod_add(lua_State *);
static int luaBN_mod_sub(lua_State *);
static int luaBN_mod_mul(lua_State *);
static int luaBN_mod_sqr(lua_State *);
static int luaBN_exp(lua_State *);
static int luaBN_mod_exp(lua_State *);
static int luaBN_gcd(lua_State *);


/*
 * Série BN_cmp()
 */


static int luaBN_cmp(lua_State *);
static int luaBN_ucmp(lua_State *);
static int luaBN_is_zero(lua_State *);
static int luaBN_is_one(lua_State *);
static int luaBN_is_odd(lua_State *);


/*
 * Série BN_zero()
 */


static int luaBN_zero(lua_State *);
static int luaBN_one(lua_State *);
static int luaBN_value_one(lua_State *);


/*
 * Série BN_rand()
 */


static int luaBN_rand(lua_State *);
static int luaBN_pseudo_rand(lua_State *);
static int luaBN_rand_range(lua_State *);
static int luaBN_pseudo_rand_range(lua_State *);


/*
 * Série BN_generate_prime()
 */


static int luaBN_generate_prime(lua_State *);
static int luaBN_is_prime(lua_State *);


/*
 * Série BN_set_bit()
 */


static int luaBN_set_bit(lua_State *);
static int luaBN_clear_bit(lua_State *);
static int luaBN_is_bit_set(lua_State *);
static int luaBN_mask_bits(lua_State *);
static int luaBN_lshift(lua_State *);
static int luaBN_lshift1(lua_State *);
static int luaBN_rshift(lua_State *);
static int luaBN_rshift1(lua_State *);


/*
 * Série BN_bn2bin()
 */


static int luaBN_bn2hex(lua_State *);
static int luaBN_bn2dec(lua_State *);
static int luaBN_hex2bn(lua_State *);
static int luaBN_dec2bn(lua_State *);


/*
 * Série BN_mod_inverse()
 */


static int luaBN_mod_inverse(lua_State *);


/*
 * Complemento
 */


static int lua_increment(lua_State *);
static int lua_decrement(lua_State *);


/*
 * Diffie-Hellman
 */


static int luaDH_generate_parameters(lua_State *);
static int luamime_b64btwoc(lua_State *);
static int luamime_unb64btwoc(lua_State *);


/*
 * yue support
 */
static int yue_BN_pack(lua_State *);
static int yue_BN_unpack(lua_State *);


/*
 * Principal
 */


int luaopen_bignum_core(lua_State *L) {
	static const luaL_reg Funcs[] = {
		// Série BN_new()
		{ "new", luaBN_new },
		{ "free", luaBN_free },
		{ "init", luaBN_init },
		{ "clear", luaBN_clear },
		{ "clear_free", luaBN_clear_free },
		
		// Série BN_copy()
		{ "copy", luaBN_copy },
		{ "dup", luaBN_dup },
		{ "swap", luaBN_swap },
		
		// Série BN_num_bytes()
		{ "num_bytes", luaBN_num_bytes },
		{ "num_bits", luaBN_num_bits },
		
		// Série BN_set_negative()
		{ "set_negative", luaBN_set_negative },
		{ "is_negative", luaBN_is_negative },
		
		// Série BN_add()
		{ "add", luaBN_add },
		{ "sub", luaBN_sub },
		{ "mul", luaBN_mul },
		{ "sqr", luaBN_sqr },
		{ "div", luaBN_div },
		{ "mod", luaBN_mod },
		{ "nnmod", luaBN_nnmod },
		{ "mod_add", luaBN_mod_add },
		{ "mod_sub", luaBN_mod_sub },
		{ "mod_mul", luaBN_mod_mul },
		{ "mod_sqr", luaBN_mod_sqr },
		{ "exp", luaBN_exp },
		{ "mod_exp", luaBN_mod_exp },
		{ "gcd", luaBN_gcd },
		
		// Série BN_cmp()
		{ "cmp", luaBN_cmp },
		{ "ucmp", luaBN_ucmp },
		{ "is_zero", luaBN_is_zero },
		{ "is_one", luaBN_is_one },
		{ "is_odd", luaBN_is_odd },
		
		// Série BN_zero()
		{ "zero", luaBN_zero },
		{ "one", luaBN_one },
		{ "value_one", luaBN_value_one },
		
		// Série BN_rand()
		{ "rand", luaBN_rand },
		{ "pseudo_rand", luaBN_pseudo_rand },
		{ "rand_range", luaBN_rand_range },
		{ "pseudo_rand_range", luaBN_pseudo_rand_range },
		
		// Série BN_generate_prime()
		{ "generate_prime", luaBN_generate_prime },
		{ "is_prime", luaBN_is_prime },
		
		// Série BN_set_bit()
		{ "set_bit", luaBN_set_bit },
		{ "clear_bit", luaBN_clear_bit },
		{ "is_bit_set", luaBN_is_bit_set },
		{ "mask_bits", luaBN_mask_bits },
		{ "lshift", luaBN_lshift },
		{ "lshift1", luaBN_lshift1 },
		{ "rshift", luaBN_rshift },
		{ "rshift1", luaBN_rshift1 },
		
		// Série BN_bn2bin()
		{ "bn2hex", luaBN_bn2hex },
		{ "bn2dec", luaBN_bn2dec },
		{ "hex2bn", luaBN_hex2bn },
		{ "dec2bn", luaBN_dec2bn },
		
		// Série BN_mod_inverse()
		{ "mod_inverse", luaBN_mod_inverse },
		
		// Complemento
		{ "increment", lua_increment },
		{ "decrement", lua_decrement },
		
		// Diffie-Hellman
		{ "generate_DH", luaDH_generate_parameters },
		{"b64btwoc", luamime_b64btwoc},
		{"unb64btwoc", luamime_unb64btwoc},
		
		//yue rpc support
		{"unpack", yue_BN_unpack},
		{"pack", yue_BN_pack},

		// Fim
		{ NULL, NULL } 
	};
	
	luaL_register(L, "bignum.core", Funcs);
	set_info(L);
	return 1;
}


static void set_info(lua_State *L) {
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2007  Rodrigo Cacilhas");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "Big number core support");
	lua_settable(L, -3);
	lua_pushliteral(L, "_NAME");
	lua_pushliteral(L, "bignum.core");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "1.0");
	lua_settable(L, -3);
}


/***********************************************************************
 * Descendo na hierarquia top-down
 */

/*
 * Série BN_new()
 */


static int luaBN_new(lua_State *L) {
	BIGNUM *a = BN_new();
	
	lua_pushlightuserdata(L, a);
	return 1;
}

static int luaBN_free(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	
	BN_free(a);
	
	return 0;
}

static int luaBN_init(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	
	BN_init(a);
	
	return 0;
}

static int luaBN_clear(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	
	BN_clear(a);
	
	return 0;
}

static int luaBN_clear_free(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	
	BN_clear_free(a);
	
	return 0;
}


/*
 * Série BN_copy()
 */


static int luaBN_copy(lua_State *L) {
	BIGNUM *to = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *from = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *resp;
	
	resp = BN_copy(to, from);
	
	if (resp != NULL)
		lua_pushlightuserdata(L, resp);
	else
		lua_pushnil(L);
	return 1;
}

static int luaBN_dup(lua_State *L) {
	BIGNUM *from = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *resp;
	
	resp = BN_dup(from);
	
	if (resp != NULL)
		lua_pushlightuserdata(L, resp);
	else
		lua_pushnil(L);
	return 1;
}

static int luaBN_swap(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 2);
	
	BN_swap(a, b);
	
	return 0;
}


/*
 * Série BN_num_bytes()
 */


static int luaBN_num_bytes(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_num_bytes(a);
	
	lua_pushnumber(L, resp);
	return 1;
}

static int luaBN_num_bits(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_num_bits(a);
	
	lua_pushnumber(L, resp);
	return 1;
}


/*
 * Série BN_set_negative()
 */


static int luaBN_set_negative(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int n = (int) lua_tonumber(L, 2);
	
	BN_set_negative(a, n);
	
	return 0;
}

static int luaBN_is_negative(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_is_negative(a);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Série BN_add()
 */


static int luaBN_add(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	int resp;
	
	resp = BN_add(r, a, b);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_sub(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	int resp;
	
	resp = BN_sub(r, a, b);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mul(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mul(r, a, b, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_sqr(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_sqr(r, a, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_div(lua_State *L) {
	BIGNUM *dv;
	if (lua_type(L, 1) == LUA_TNIL)
		dv = NULL;
	else
		dv = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *rem = NULL;
	if (lua_type(L, 2) != LUA_TNIL)
		rem = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 3);
	BIGNUM *d = (BIGNUM *) lua_touserdata(L, 4);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_div(dv, rem, a, d, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod(lua_State *L) {
	BIGNUM *rem = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod(rem, a, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_nnmod(lua_State *L) {
	BIGNUM *rem = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_nnmod(rem, a, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod_add(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 4);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod_add(r, a, b, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod_sub(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 4);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod_sub(r, a, b, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod_mul(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 4);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod_mul(r, a, b, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod_sqr(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod_sqr(r, a, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_exp(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *p = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_exp(r, a, p, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mod_exp(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *p = (BIGNUM *) lua_touserdata(L, 3);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 4);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_mod_exp(r, a, p, m, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_gcd(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_gcd(r, a, b, ctx);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Série BN_cmp()
 */


static int luaBN_cmp(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_cmp(a, b);
	
	lua_pushnumber(L, resp);
	return 1;
}

static int luaBN_ucmp(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *b = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_ucmp(a, b);
	
	lua_pushnumber(L, resp);
	return 1;
}

static int luaBN_is_zero(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_is_zero(a);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_is_one(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_is_one(a);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_is_odd(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_is_odd(a);
	
	lua_pushboolean(L, resp);
	return 1;
}



/*
 * Série BN_zero()
 */


static int luaBN_zero(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_zero(a);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_one(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int resp;
	
	resp = BN_one(a);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_value_one(lua_State *L) {
	BIGNUM *a = (BIGNUM *) BN_value_one();
	
	lua_pushlightuserdata(L, a);
	return 1;
}


/*
 * Série BN_rand()
 */


static int luaBN_rand(lua_State *L) {
	BIGNUM *rnd = (BIGNUM *) lua_touserdata(L, 1);
	int bits = (int) lua_tonumber(L, 2);
	int top = (int) lua_toboolean(L, 3);
	int bottom = (int) lua_toboolean(L, 4);
	int resp;
	
	resp = BN_rand(rnd, bits, top, bottom);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_pseudo_rand(lua_State *L) {
	BIGNUM *rnd = (BIGNUM *) lua_touserdata(L, 1);
	int bits = (int) lua_tonumber(L, 2);
	int top = (int) lua_toboolean(L, 3);
	int bottom = (int) lua_toboolean(L, 4);
	int resp;
	
	resp = BN_pseudo_rand(rnd, bits, top, bottom);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_rand_range(lua_State *L) {
	BIGNUM *rnd = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *range = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_rand_range(rnd, range);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_pseudo_rand_range(lua_State *L) {
	BIGNUM *rnd = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *range = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_pseudo_rand_range(rnd, range);
	
	lua_pushboolean(L, resp);
	return 1;
}



/*
 * Série BN_generate_prime()
 */


static int luaBN_generate_prime(lua_State *L) {
	BIGNUM *ret = (BIGNUM *) lua_touserdata(L, 1);
	int num = (int) lua_tonumber(L, 2);
	int safe = (int) lua_toboolean(L, 3);
	BIGNUM *add = NULL;
	if (lua_type(L, 4) != LUA_TNIL)
		add = (BIGNUM *) lua_touserdata(L, 4);
	BIGNUM *rem = NULL;
	if (lua_type(L, 5) != LUA_TNIL)
		rem = (BIGNUM *) lua_touserdata(L, 5);
	BIGNUM *a;
	
	a = BN_generate_prime(ret, num, safe, add, rem, NULL, NULL);
	
	lua_pushlightuserdata(L, a);
	return 1;
}

static int luaBN_is_prime(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int checks = (int) lua_tonumber(L, 2);
	BN_CTX *ctx = BN_CTX_new();
	int resp;
	
	resp = BN_is_prime(a, checks, NULL, ctx, NULL);
	BN_CTX_free(ctx);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Série BN_set_bit()
 */


static int luaBN_set_bit(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int n = (int) lua_tonumber(L, 2);
	int resp;
	
	resp = BN_set_bit(a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_clear_bit(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int n = (int) lua_tonumber(L, 2);
	int resp;
	
	resp = BN_clear_bit(a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_is_bit_set(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int n = (int) lua_tonumber(L, 2);
	int resp;
	
	resp = BN_is_bit_set(a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_mask_bits(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	int n = (int) lua_tonumber(L, 2);
	int resp;
	
	resp = BN_mask_bits(a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_lshift(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	int n = (int) lua_tonumber(L, 3);
	int resp;
	
	resp = BN_lshift(r, a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_rshift(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	int n = (int) lua_tonumber(L, 3);
	int resp;
	
	resp = BN_rshift(r, a, n);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_lshift1(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_lshift1(r, a);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_rshift1(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	int resp;
	
	resp = BN_rshift1(r, a);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Série BN_bn2bin()
 */


static int luaBN_bn2hex(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	char *resp;
	
	resp = BN_bn2hex(a);
	
	lua_pushstring(L, resp);
	return 1;
}

static int luaBN_bn2dec(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	char *resp;
	
	resp = BN_bn2dec(a);
	
	lua_pushstring(L, resp);
	return 1;
}

static int luaBN_hex2bn(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	char *str = (char *) lua_tostring(L, 2);
	int resp;
	
	resp = BN_hex2bn(&a, str);
	
	lua_pushboolean(L, resp);
	return 1;
}

static int luaBN_dec2bn(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	char *str = (char *) lua_tostring(L, 2);
	int resp;
	
	resp = BN_dec2bn(&a, str);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Série BN_mod_inverse()
 */


static int luaBN_mod_inverse(lua_State *L) {
	BIGNUM *r = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 2);
	BIGNUM *m = (BIGNUM *) lua_touserdata(L, 3);
	BN_CTX *ctx = BN_CTX_new();
	BIGNUM *resp;
	
	resp = BN_mod_inverse(r, a, m, ctx);
	BN_CTX_free(ctx);
	
	if (resp != NULL)
		lua_pushlightuserdata(L, resp);
	else
		lua_pushnil(L);
	return 1;
}


/*
 * Complemento
 */


static int lua_increment(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *m = NULL;
	BN_CTX *ctx = NULL;
	if (lua_type(L, 2) != LUA_TNIL) {
		m = (BIGNUM *) lua_touserdata(L, 2);
		ctx = BN_CTX_new();
	}
	BIGNUM *one = (BIGNUM *) BN_value_one();
	int resp;
	
	if (m != NULL) {
		resp = BN_mod_add(a, a, one, m, ctx);
		BN_CTX_free(ctx);
	} else
		resp = BN_add(a, a, one);
	
	lua_pushboolean(L, resp);
	return 1;
}


static int lua_decrement(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	BIGNUM *m = NULL;
	BN_CTX *ctx = NULL;
	if (lua_type(L, 2) != LUA_TNIL) {
		m = (BIGNUM *) lua_touserdata(L, 2);
		ctx = BN_CTX_new();
	}
	BIGNUM *one = (BIGNUM *) BN_value_one();
	int resp;
	
	if (m != NULL) {
		resp = BN_mod_sub(a, a, one, m, ctx);
		BN_CTX_free(ctx);
	} else
		resp = BN_sub(a, a, one);
	
	lua_pushboolean(L, resp);
	return 1;
}


/*
 * Diffie-Hellman
 */


static int luaDH_generate_parameters(lua_State *L) {
	int bits = (int) lua_tonumber(L, 1);
	int generator = (int) lua_tonumber(L, 2);
	DH *dh;
	
	dh = DH_generate_parameters(bits, generator, NULL, NULL);
	
	if (dh != NULL) {
		lua_pushlightuserdata(L, dh->p);
		lua_pushlightuserdata(L, dh->g);
		return 2;
	} else {
		lua_pushnil(L);
		return 1;
	}
}


static int luamime_b64btwoc(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	unsigned int len = BN_num_bytes(a) + 512; // 512B = folga
	char *in = (char *) malloc(sizeof(char) * len);
	char *out = (char *) malloc(sizeof(char) * len);
	
	len = btwoc(in, a);
	base64(out, in, len);

	lua_pushstring(L, out);
	return 1;
}


static int luamime_unb64btwoc(lua_State *L) {
	BIGNUM *a = (BIGNUM *) lua_touserdata(L, 1);
	char *b = (char *) lua_tostring(L, 2);
	char *aux = (char *) malloc(sizeof(char) * 1024);
	unsigned int len;
	
	len = unbase64(aux, b);
	unbtwoc(a, aux, len);

	return 0;
}

/*
 * yue support
 */
static int yue_BN_pack(lua_State *L) {
	BIGNUM *bn = (BIGNUM *)lua_touserdata(L, 1);
	yue_Wbuf *wb = lua_touserdata(L, 2);
	unsigned char buffer[BN_num_bytes(bn)];
	int len = BN_bn2bin(bn, buffer);
	yueb_write(wb, buffer, len);
	return 0;
}

static int yue_BN_unpack(lua_State *L) {
	BIGNUM *bn;
	yue_Rbuf *rb = lua_touserdata(L, 1);
	int size; const void *p = yueb_read(rb, &size);
	if (!(bn = BN_bin2bn(p, size, NULL))) {
		lua_pushfstring(L, "cannot convert buffer %d\n", size);
		lua_error(L);
	}
	lua_pushlightuserdata(L, bn);
	return 1;
}
