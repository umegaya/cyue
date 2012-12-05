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
#include "msgid.h"
#include "exlib/cityhash/city.h"

namespace yue {
/* nil class */
namespace type {
struct nil {};
}
inline type::nil nc_nil() { return type::nil(); }
inline const type::nil c_nil() { return type::nil(); }

namespace util {
extern int static_init();
extern int init();
extern void static_fin();
extern void fin();
/*-------------------------------------------------------------------*/
/* math 															 */
/*-------------------------------------------------------------------*/
namespace math {
/* calc PJW hush */
static inline unsigned int
pjw_hush(int M, const unsigned char *t)
{
	unsigned int h = 0, g;
	for(;*t;++t) {
		h = (h << 4) + *t;
		if ((g = h&0xf0000000) != 0) {
			h ^= g >> 24;
			h ^= g;
		}
	}
	return h % M;
}

/* calc Murmur hush */
#include "exlib/murmur/MurmurHash2.cpp"

/* calc city hash */
inline U64 cityhash64(const char *b, size_t len) {
	return CityHash64(b, len);
}
inline U32 cityhash32(const char *b, size_t len) {
	return CityHash32(b, len);
}

/* find max prime number that less than given integer */
/* extremely slow cause it is simple impl of sieve of Eratosthenes */
extern int prime(int given);
namespace rand {
extern int init();
extern void fin();
}
extern U32 rand32();
extern U64 rand64();
}
/*-------------------------------------------------------------------*/
/* timeval 															 */
/*-------------------------------------------------------------------*/
namespace time {
extern int init();
extern void fin();
static inline time_t unix_time() {
	return ::time(NULL);
}
static inline time_t max_unix_time() {
	return ((time_t)0xFFFFFFFF);
}
extern UTIME clock();
extern void update_clock();
extern UTIME now();
extern int sleep(NTIME ns);
struct logical_clock {
	time_t wallclock;
	U32 logical_timestamp;
	static util::msgid_generator<U32> m_gen;
public:
	logical_clock() : wallclock(unix_time()), logical_timestamp(m_gen.new_id()) {}
	inline bool operator == (const logical_clock &lc) const {
		return lc.wallclock == wallclock && lc.logical_timestamp == logical_timestamp;
	}
	inline void invalidate() {
		wallclock = 0; logical_timestamp = 0;
	}
};
}
/*-------------------------------------------------------------------*/
/* memory 															 */
/*-------------------------------------------------------------------*/
namespace mem {
inline void *alloc(size_t sz) {
	return ::malloc(sz);
}
inline void *realloc(void *p, size_t sz) {
	return ::realloc(p, sz);
}
inline void *calloc(size_t nmemb, size_t sz) {
	return ::calloc(nmemb, sz);
}
inline void free(void *p) {
	::free(p);
}
inline int copy(void *p, const void *q, size_t sz) {
	::memcpy(p, q, sz);
	return sz;
}
inline int cmp(const void *p, const void *q, size_t sz) {
	return ::memcmp(p, q, sz);
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
/*-------------------------------------------------------------------*/
/* base64  															 */
/*-------------------------------------------------------------------*/
namespace base64 {
extern int encode(const char* plaintext_in, int length_in, char* code_out);
extern int decode(const char* code_in, const int length_in, char* plaintext_out);
inline size_t buffsize(size_t in_size) {
	return (( 3 + /* padding to size to multiple of 4byte */
		((((2 + in_size) * 4) / 3) + 1 + 1)) /*
because current base64 routine append \n on last of encoded string for unknown reason.
	*/ >> 2) << 2;	/* align to 4byte */
}
}
/*-------------------------------------------------------------------*/
/* sha1  															 */
/*-------------------------------------------------------------------*/
static const size_t SHA1_160BIT_RESULT_SIZE = 20;
namespace sha1 {
extern int encode(const char* data, int len, U8 result[20]);
}
/*-------------------------------------------------------------------*/
/* string 															 */
/*-------------------------------------------------------------------*/
namespace str {
static const U32 MAX_LENGTH = 1024;
inline int _cmp(const char *a, size_t al, const char *b, size_t bl) {
	const char *wa = a, *wb = b;
	while (*wa && *wb) {
		if (*wa > *wb) {
			return 1;
		}
		else if (*wa < *wb) {
			return -1;
		}
		wa++; wb++;
		if ((size_t)(wa - a) > al) {
			if ((size_t)(wb - b) > bl) {
				return 0;
			}
			return -1;
		}
		else {
			if ((size_t)(wb - b) > bl) {
				return 1;
			}
		}
	}
	if (*wa == *wb) {
		return 0;
	}
	else if (!(*wb)) {
		return 1;
	}
	else if (!(*wa)) {
		return -1;
	}
	return 0;
}
inline int cmp(const char *a, const char *b, U32 sz = MAX_LENGTH) {
	return _cmp(a, sz, b, sz);
}
extern int cmp_nocase(const char *a, const char *b, U32 sz = MAX_LENGTH);
inline int _length(const char *str, size_t max)
{
	const char *w = str;
	while(*w) {
		w++;
		if ((size_t)(w - str) > max) {
			ASSERT(0);
			return max;
		}
	}
	return (w - str);
}
inline size_t length(const char *a, U32 sz = MAX_LENGTH) {
	return _length(a, sz);
}
inline int _copy(char *a, size_t al, const char *b, size_t bl)
{
	char *wa = a;
	const char *wb = b;
	while (*wb) {
		*wa++ = *wb++;
		if ((size_t)(wa - a) >= al) {
			a[al - 1] = '\0';
			return al;
		}
		if ((size_t)(wb - b) >= bl) {
			*wa = '\0';
			return (wa - a);
		}
	}
	*wa = '\0';
	return (wa - a);
}
inline int copy(char *a, const char *b, U32 sz = MAX_LENGTH) {
	return _copy(a, sz, b, sz);
}
extern int _atobn(const char* str, S64 *i, int max);
extern int _atoi(const char* str, int *i, int max);
template <class T> int atoi(const char *a, T &t, int max = MAX_LENGTH) {
	int r; ;
	if (sizeof(T) > sizeof(U32)) {
		S64 tmp;
		if ((r = _atobn(a, &tmp, max)) < 0) { return r; }
		t = static_cast<T>(tmp);
	}
	else {
		S32 tmp;
		if ((r = _atoi(a, &tmp, max)) < 0) { return r; }
		t = static_cast<T>(tmp);
	}
	return r;
}
inline char *random(char *p, int l) {
	const char texts[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	for (int i = 0; i < l; i++) {
		/* rand character except last \0 */
		p[i] = texts[math::rand32() % (sizeof(texts) - 1)];
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
#if POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700
	return strndup(src, sz);
#else
	size_t l = length(src, sz);
	char *p = reinterpret_cast<char *>(util::mem::alloc(l + 1));
	util::mem::copy(p, src, l);
	p[l] = '\0';
	return p;
#endif
}
inline int split(char *src, const char *delim, char **buff, int bufsize) {
	char **org = buff;
	for (*buff++ = strtok(src, delim);
		*(buff - 1) && (buff - org) <= bufsize;
		*buff++ = strtok(NULL, delim));
	return (buff - org - 1);
}
extern int htoi(const char* str, int *i, int max);
extern int htobn(const char* str, S64 *i, int max);
extern size_t utf8_copy(char *dst, int dlen, const char *src, int smax, int len);
extern const char *divide_tag_and_val(char sep, const char *line, char *tag, int taglen);
extern const char *divide(const char *sep, const char *line, char *tag, int *tlen);
extern int cmp_tail(const char *a, const char *b, int len, int max);
extern int parse_url(const char *in, int max, char *host, U16 *port, char *url);
extern char *chop(char *buffer);
extern const char *rchr(const char *in, char sep, int max);
extern int parse_http_req_str(const char *req, const char *tag, char *buf, int buflen);
extern int parse_http_req_int(const char *req, const char *tag, int *buf);
extern int parse_http_req_bigint(const char *req, const char *tag, long long *buf);
}
/*-------------------------------------------------------------------*/
/* meta programming													 */
/*-------------------------------------------------------------------*/
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
template <class T>
struct reference_holder {
	T &m_ref;
	inline reference_holder(T &t) : m_ref(t) {}
	inline reference_holder(const reference_holder &h) : m_ref(h.m_ref) {}
	inline operator T &() { return m_ref; }
};
template <class T>
struct const_reference_holder {
	const T &m_ref;
	inline const_reference_holder(const T &t) : m_ref(t) {}
	inline const_reference_holder(const const_reference_holder &h) : m_ref(h.m_ref) {}
	inline operator const T &() { return m_ref; }
};
template <class X> reference_holder<X> inline ref(X &x) {
	return reference_holder<X>(x);
}
template <class X> const_reference_holder<X> inline cref(const X &x) {
	return const_reference_holder<X>(x);
}
}
namespace debug {
#if defined(_DEBUG)
extern void bt(int start = 1, int num = 64);
extern void btstr(char *buff, int size, int start = 1, int num = 64);
#else
static inline void bt(int = 1, int num = 64) {}
static inline void btstr(char *buff, int size, int start = 1, int num = 64) {}
#endif
}
}
}

#endif
