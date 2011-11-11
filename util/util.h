/***************************************************************
 * util.h : utilities
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
#if !defined(__UTIL_H__)
#define __UTIL_H__

#include "common.h"
#include <time.h>
#include <stdarg.h>
#include <memory.h>

namespace yue {
/* nil class */
namespace type {
struct nil {};
}
inline type::nil nc_nil() { return type::nil(); }
inline const type::nil c_nil() { return type::nil(); }

namespace util {
/* timeval */
namespace time {
static inline time_t unix_time() {
	return ::time(NULL);
}
static inline time_t max_unix_time() {
	return ((time_t)0xFFFFFFFF);
}
static inline UTIME clock() {
	return nbr_clock();
}
extern "C" void nbr_clock_poll();
static inline void update_clock() {
	nbr_clock_poll();
}
static inline UTIME now() {
	return nbr_time();
}
typedef U64 NTIME;	/* nanosec */
static inline int sleep(NTIME ns) {
	return nbr_osdep_sleep(ns);
}
}
/* string */
namespace str {
static const U32 MAX_LENGTH = 256;
inline int cmp(const char *a, const char *b, U32 sz = MAX_LENGTH) {
	return nbr_str_cmp(a, sz, b, sz);
}
inline int cmp_nocase(const char *a, const char *b, U32 sz = MAX_LENGTH) {
	return nbr_str_cmp_nocase(a, b, sz);
}
inline size_t length(const char *a, U32 sz = MAX_LENGTH) {
	return nbr_str_length(a, sz);
}
inline int copy(char *a, const char *b, U32 sz = MAX_LENGTH) {
	return nbr_str_copy(a, sz, b, sz);
}
template <class T> int atoi(const char *a, T &t, int max = MAX_LENGTH) {
	int r; ;
	if (sizeof(T) > sizeof(U32)) {
		S64 tmp;
		if ((r = nbr_str_atobn(a, &tmp, max)) < 0) { return r; }
		t = static_cast<T>(tmp);
	}
	else {
		S32 tmp;
		if ((r = nbr_str_atoi(a, &tmp, max)) < 0) { return r; }
		t = static_cast<T>(tmp);
	}
	return r;
}
inline char *random(char *p, int l) {
	const char texts[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	for (int i = 0; i < l; i++) {
		/* rand character except last \0 */
		p[i] = texts[nbr_rand32() % (sizeof(texts) - 1)];
	}
	return p;
}
inline size_t printf(char *p, size_t l, const char *fmt, ...) {
	va_list v;
	size_t sz;
	va_start(v, fmt);
	sz = vsnprintf(p, l, fmt, v);
	va_end(v);
	return sz;
}
inline void dumpbin(const char *buf, size_t sz) {
	TRACE("%p:%llu[%02x", buf, (long long unsigned int)sz, (U8)*buf);
	for (size_t s = 1; s < sz; s++) {
		TRACE(":%02x", ((const U8 *)buf)[s]);
	}
	TRACE("]\n");
}
inline char *dup(const char *src, size_t sz = MAX_LENGTH) {
	return strndup(src, sz);
}
inline const char *divide(char sep, const char *src, char *tag, int tlen) {
	return nbr_str_divide_tag_and_val(sep, src, tag, tlen);
}
}
/* memory */
namespace mem {
inline void *alloc(size_t sz) {
	return nbr_malloc(sz);
}
inline void *realloc(void *p, size_t sz) {
	return nbr_realloc(p, sz);
}
inline void free(void *p) {
	return nbr_free(p);
}
inline int copy(void *p, const void *q, size_t sz) {
	nbr_mem_copy(p, q, sz);
	return sz;
}
inline int cmp(const void *p, const void *q, size_t sz) {
	return nbr_mem_cmp(p, q, sz);
}
inline void *move(void *p, const void *q, size_t sz) {
	return ::memmove(p, q, sz);
}
inline int fill(void *p, U8 value, size_t sz) {
	::memset(p, value, sz);
	return sz;
}
inline int bzero(void *p, size_t sz) {
	fill(p, 0, sz);
	return sz;
}
}
/* meta programming */
namespace mpg {
template <class X>
struct ref_traits {
	typedef X &type;
};
template <class X>
struct ref_traits<X &> {
	typedef X &type;
};
template <class X>
struct ref_traits<X *> {
	typedef X *type;
};
}
}
}

#endif
