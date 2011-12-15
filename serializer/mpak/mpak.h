/***************************************************************
 * mpak.h : binary serializer (compatible to msgpack)
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
#if !defined(__MPAK_H__)
#define __MPAK_H__

#include "msgpack/object.h"
typedef enum {
        MSGPACK_UNPACK_SUCCESS                          =  2,
        MSGPACK_UNPACK_EXTRA_BYTES                      =  1,
        MSGPACK_UNPACK_CONTINUE                         =  0,
        MSGPACK_UNPACK_PARSE_ERROR                      = -1,
} msgpack_unpack_return;
#include "sbuf.h"
#include "types.h"

namespace yue {
using namespace util;
namespace module {
namespace serializer {

#define MPAK_TRACE(...)
class mpak {
public:
	static const U8 P_FIXNUM_START= 0x00;
	static const U8 P_FIXNUM_END 	= 0x7f;
	static const U8 N_FIXNUM_START= 0xe0;
	static const U8 N_FIXNUM_END 	= 0xff;
	static const U8 VARIABLE_START= 0x80;
	static const U8 NIL_VALUE 	= 0xc0;
	static const U8 STR 		= 0xc1;
	static const U8 BOOLEAN_FALSE = 0xc2;
	static const U8 BOOLEAN_TRUE 	= 0xc3;
	static const U8 RESERVED1 	= 0xc4;
	static const U8 RESERVED2 	= 0xc5;
	static const U8 RESERVED3 	= 0xc6;
	static const U8 RESERVED4 	= 0xc7;
	static const U8 RESERVED5 	= 0xc8;
	static const U8 RESERVED6 	= 0xc9;
	static const U8 TABLE_START  = 0xc4;
	static const U8 FLOAT 	= 0xca;
	static const U8 DOUBLE_TYPE 	= 0xcb;
	static const U8 UINT_8BIT 	= 0xcc;
	static const U8 UINT_16BIT 	= 0xcd;
	static const U8 UINT_32BIT 	= 0xce;
	static const U8 UINT_64BIT 	= 0xcf;
	static const U8 SINT_8BIT 	= 0xd0;
	static const U8 SINT_16BIT 	= 0xd1;
	static const U8 SINT_32BIT 	= 0xd2;
	static const U8 SINT_64BIT 	= 0xd3;
	static const U8 BIG_FLOAT_16 	= 0xd6;
	static const U8 BIG_FLOAT_32 	= 0xd7;
	static const U8 BIGINT_16 	= 0xd8;
	static const U8 BIGINT_32 	= 0xd9;
	static const U8 RAW16 	= 0xda;
	static const U8 RAW32 	= 0xdb;
	static const U8 ARRAY16 	= 0xdc;
	static const U8 ARRAY32 	= 0xdd;
	static const U8 MAP16 	= 0xde;
	static const U8 MAP32 	= 0xdf;
	static const U8 VARIABLE_END 	= 0xdf;
	static const U8 TABLE_END = 0xdf;
	static const U8 FIXRAW_MASK	= 0xa0;
	static const U8 FIXARRAY_MASK = 0x90;
	static const U8 FIXMAP_MASK	= 0x80;
public:
	enum {
		NIL = MSGPACK_OBJECT_NIL,
		BOOLEAN = MSGPACK_OBJECT_BOOLEAN,
		INTEGER = MSGPACK_OBJECT_NEGATIVE_INTEGER,
		DOUBLE = MSGPACK_OBJECT_DOUBLE,
		BLOB = MSGPACK_OBJECT_RAW,
		ARRAY = MSGPACK_OBJECT_ARRAY,
		MAP = MSGPACK_OBJECT_MAP,
		STRING = MSGPACK_OBJECT_RAW,
	};
	enum {
		UNPACK_SUCCESS		=  MSGPACK_UNPACK_SUCCESS,
		UNPACK_EXTRA_BYTES	=  MSGPACK_UNPACK_EXTRA_BYTES,
		UNPACK_CONTINUE		=  MSGPACK_UNPACK_CONTINUE,
		UNPACK_PARSE_ERROR	=  MSGPACK_UNPACK_PARSE_ERROR,
	};
protected:
#if defined(__USE_OLD_BUFFER)
	char *m_p;
	size_t m_s, m_c;
#else
	pbuf m_pbuf;
	size_t m_c;
#endif
public:
#if defined(__USE_OLD_BUFFER)
	mpak() : m_p(NULL), m_s(0), m_c(0) { init_context(m_ctx); m_ctx.stack[0].d = &(m_ctx.at_work); }
#else
	mpak() : m_pbuf(), m_c(0) { init_context(m_ctx); m_ctx.stack[0].d = &(m_ctx.at_work); }
#endif
	~mpak() {}
/* external interface required from eio */
public:	/* 1. type 'object' and 'data' */
#define CHECK(require)	(((int)msgpack_object::type) == ((int)require))
#define CHECK_INTEGER()	(CHECK(MSGPACK_OBJECT_POSITIVE_INTEGER)||CHECK(MSGPACK_OBJECT_NEGATIVE_INTEGER))
	typedef msgpack_object object_struct;
	class data : public object_struct {
		friend class mpak;
	public:
		inline data &elem(int n) 				{ ASSERT(CHECK(ARRAY)); return (data &)via.array.ptr[n]; }
		inline const data &elem(int n) const 	{ ASSERT(CHECK(ARRAY)); return (const data &)via.array.ptr[n]; }
		inline data &key(int n) 				{ ASSERT(CHECK(MAP)); return (data &)via.map.ptr[n].key; }
		inline const data &key(int n) const 	{ ASSERT(CHECK(MAP)); return (const data &)via.map.ptr[n].key; }
		inline data &val(int n) 				{ ASSERT(CHECK(MAP)); return (data &)via.map.ptr[n].val; }
		inline const data &val(int n) const 	{ ASSERT(CHECK(MAP)); return (const data &)via.map.ptr[n].val; }
		/* FIXME: now via.array == via.map (same structure) so its ok. */
		inline U32 size() const 				{ ASSERT(CHECK(MAP)||CHECK(ARRAY)); return via.array.size; }
		inline U32 len() const 				{ ASSERT(CHECK(BLOB)); return via.raw.size; }
		inline int kind() const {
			switch(msgpack_object::type) {
			case MSGPACK_OBJECT_NIL:
				return NIL;
			case MSGPACK_OBJECT_BOOLEAN:
				return BOOLEAN;
			case MSGPACK_OBJECT_POSITIVE_INTEGER:
			case MSGPACK_OBJECT_NEGATIVE_INTEGER:
				return INTEGER;
			case MSGPACK_OBJECT_DOUBLE:
				return DOUBLE;
			case MSGPACK_OBJECT_RAW:
				return BLOB;
			case MSGPACK_OBJECT_ARRAY:
				return ARRAY;
			case MSGPACK_OBJECT_MAP:
				return MAP;
			default:
				ASSERT(false);
				return NIL;
			}
		}
	public:
		inline operator bool () const 				{ ASSERT(CHECK(BOOLEAN)); return via.boolean; }
		inline bool operator ! () const			{ return !this->operator bool (); }
		inline operator int () const 				{ ASSERT(CHECK_INTEGER()); return (int)via.i64; }
		inline operator unsigned int () const 		{ ASSERT(CHECK_INTEGER()); return (unsigned int)via.u64; }
		inline operator long long () const 		{ ASSERT(CHECK_INTEGER()); return via.i64; }
		inline operator unsigned long long () const{ ASSERT(CHECK_INTEGER()); return via.u64; }
		inline operator double() const 			{ CHECK(DOUBLE); return via.dec; }
		inline operator float() const				{ CHECK(DOUBLE); return (float)via.dec; }/* BLACKMAGIC: C style cast!! */
		inline operator const char *() const 		{ CHECK(BLOB); return via.raw.ptr; }
		inline operator const void *() const 		{ CHECK(BLOB); return via.raw.ptr; }
		inline operator void *() 					{ CHECK(BLOB); return (void *)via.raw.ptr; }
		inline operator char *()					{ CHECK(BLOB); return (char *)via.raw.ptr; }
		/* it implement here because it depends on type system. */
	public:
		/* FIXME : black magic...(for security, swap packet content) */
		inline void set_ptr(const void *p) 	{ ASSERT(CHECK(BLOB)); via.raw.ptr = (const char *)p; }
	private:
		class data &operator = (const data &d);
		inline void set_type(int t) {
			/* BLACKMAGIC: C style cast!! */
			msgpack_object::type = (msgpack_object_type)t;
		}
		inline void set_nil() { set_type(NIL); }
		inline void set(S64 i) { set_type(INTEGER); via.i64 = i; }
		inline void set(bool b) { set_type(BOOLEAN); via.boolean = b; }
		inline void set(double d) { set_type(DOUBLE); via.dec = d; }
		inline void set(float d) { set_type(DOUBLE); via.dec = (double)d; }
		inline void set(char *p, size_t sz) {
			set_type(BLOB);
			via.raw.ptr = p;
			via.raw.size = sz;
		}
		int init_array(sbuf &sbf, size_t sz) {
			set_type(ARRAY);
			via.array.size = sz;
			if (sz == 0) {
				via.array.ptr = NULL;
				return NBR_OK;
			}
			return (via.array.ptr = reinterpret_cast<msgpack_object *>(
					sbf.malloc(sz * sizeof(msgpack_object))
				))? NBR_OK : NBR_EMALLOC;
		}
		int init_map(sbuf &sbf, size_t sz) {
			set_type(MAP);
			via.map.size = sz;
			if (sz == 0) {
				via.map.ptr = NULL;
				return NBR_OK;
			}
			return (via.map.ptr = reinterpret_cast<msgpack_object_kv *>(
					sbf.malloc(sz * sizeof(msgpack_object_kv))
				))? NBR_OK : NBR_EMALLOC;
		}
		inline data *elem_new(size_t idx) {
			ASSERT(via.array.size > idx && CHECK(ARRAY));
			return reinterpret_cast<data *>(&(via.array.ptr[idx]));
		}
		inline data *key_new(size_t idx) {
			ASSERT(via.map.size > idx && CHECK(MAP));
			return reinterpret_cast<data *>(&(via.map.ptr[idx].key));
		}
		inline data *val_new(size_t idx) {
			ASSERT(via.map.size > idx && CHECK(MAP));
			return reinterpret_cast<data *>(&(via.map.ptr[idx].val));
		}
	};
#undef CHECK
#undef CHECK_INTEGER
	class object : public data {
		friend class mp;
		sbuf *m_sbuf;
	public:
		void set_sbuf(sbuf *sbf) { m_sbuf = sbf; }
		void *malloc(size_t s) { return m_sbuf->malloc(s); }
		void fin() { if (m_sbuf) { delete m_sbuf; m_sbuf = NULL; } }
		int pack(mpak &mpk) const {
			return mpk << *this;
		}
	public:
		class object &operator = (object &d) {
			msgpack_object::type = d.type;
			msgpack_object::via = d.via;
			m_sbuf = d.m_sbuf; d.m_sbuf = NULL;
			return *this;
		}
	};
public:	/* 2. seek functions */
#if defined(__USE_OLD_BUFFER)
	inline int curpos() const { return m_c; }
	inline char *curr_p() { return m_p + m_c; }
	inline int len() const { return curpos(); }
	inline void set_curpos(U32 pos) { m_c = pos; }
	inline int rewind(U32 sz) {
		if (m_c < sz) { return m_c; }
		m_c -= sz;
		return m_c;
	}
	inline int skip(U32 sz);
	void start_pack(char *p, size_t s) { m_p = p; m_s = s; m_c = 0; }
	int remain() const { ASSERT(m_s >= m_c); return (m_s - m_c); }
	inline void reset_limit(size_t s) { m_s = s; }
#else
#if defined(__USE_OLD_BUFFER)
	inline char *start_p() { return m_p; }
	inline size_t buffsize() const { return m_s; }
	void start_pack(char *p, size_t s) { m_p = p; m_s = s; m_c = 0; }
#else
	inline char *start_p() { return m_pbuf.last_p(); }
	inline size_t buffsize() const { return m_pbuf.available(); }
	inline void start_pack(pbuf &pbf) { m_pbuf.copy(pbf); m_c = 0; }
	inline pbuf &pack_buffer() { return m_pbuf; }
#endif
	inline int curpos() const { return m_c; }
	inline char *curr_p() { return start_p() + m_c; }
	inline int len() const { return curpos(); }
	inline void set_curpos(U32 pos) { m_c = pos; }
	inline int rewind(U32 sz) {
		if (m_c < sz) { return m_c; }
		m_c -= sz;
		return m_c;
	}
	inline int skip(U32 sz);
	inline int remain() const { return buffsize() - m_c; }
#endif
protected:
	inline void push(U8 u) { start_p()[m_c++] = (char)u; }
	inline void push(S8 s) { start_p()[m_c++] = (char)s; }
	inline void commit(size_t s) { m_c += s; }
	inline void push(U16 u) { SET_16(curr_p(), htons(u)); commit(sizeof(u)); }
	inline void push(S16 s) { SET_16(curr_p(), htons(s)); commit(sizeof(s)); }
	inline void push(U32 u) { SET_32(curr_p(), htonl(u)); commit(sizeof(u)); }
	inline void push(S32 s) { SET_32(curr_p(), htonl(s)); commit(sizeof(s)); }
	inline void push(U64 u) { SET_64(curr_p(), htonll(u)); commit(sizeof(u)); }
	inline void push(S64 s) { SET_64(curr_p(), htonll(s)); commit(sizeof(s)); }
public:	/* 3. 'operator << (TYPE)'*/
	inline int pushnil();
	inline int operator << (bool f);
	inline int operator << (U8 u);
	inline int operator << (S8 s);
	inline int operator << (U16 u);
	inline int operator << (S16 s);
	inline int operator << (U32 u);
	inline int operator << (S32 s);
	inline int operator << (U64 u);
	inline int operator << (S64 s);
	inline int operator << (float f);
	inline int operator << (double f);
	inline int operator << (const data &d);
	inline int operator << (const char *s);
	inline int push_raw_len(size_t l);
	inline int push_raw_onlydata(const char *p, size_t l);
	inline int push_raw(const char *p, size_t l);
	inline int push_string(const char *str, int l = -1);
	inline int push_array_len(size_t l);
	inline int push_array(const data *a, size_t l);
	inline int push_map_len(size_t l);
	/* size of a is l * 2 (key + value) */
	inline int push_map(const data *a, size_t l);
protected:
	static const U32 UNPACK_STACK_DEPTH = 256;
	struct context {
		enum parse_state {
			INITIAL,		/* need to detect next type. it will be
								=> 	INITIAL,
									PROCESS_LENGTH,
									PROCESS_ARRAY,
									PROCESS_MAP,
						 	 */
			PROCESS_LENGTH,/* need to parse length
								=>  INITIAL,
									PROCESS_DATA,
									PROCESS_ARRAY,
									PROCESS_MAP,
			 	 	 	 	 */
			PROCESS_DATA,	/* need to parse data
								=> 	INITIAL
			 	 	 	 	 */
			PROCESS_ARRAY,	/* need to parse array structure. will be
			 	 	 	 	 	=> PROCESS_ARRAY,
			 	 	 	 	 		INITIAL, */
			PROCESS_MAP,	/* need to parse map structure (key). will be
			 	 	 	 	 	 => PROCESS_MAP_VAL */
			PROCESS_MAP_VAL,/* need to parse map structure (val). will be
			 	 	 	 	 	 => PROCESS_MAP,
			 	 	 	 	 	 	 INITIAL. */
		};
		size_t stack_height;
		object at_work;
		sbuf *sbf;
		struct stackmem {
			parse_state status;
			data *d;
			size_t require_len, n_pop_obj;
			U8 require_type, padd[3];
		} stack[UNPACK_STACK_DEPTH];
	} m_ctx;
	template <typename T> static inline T cast_to(pbuf &pbf) {
		return *(reinterpret_cast<T *>(pbf.cur_p()));
	}
	static void init_context(context &ctx) {
		ctx.stack_height = 0;
		ctx.sbf = NULL;
		ctx.stack[0].status = context::INITIAL;
	}
public:	/* 4. function 'unpack' */
	inline int unpack(pbuf &pbf) {
		if (!m_ctx.sbf && !(m_ctx.sbf = new sbuf)) {
			return NBR_EMALLOC;
		}
		return unpack(m_ctx, pbf, *m_ctx.sbf);
	}
	inline object &result() { return m_ctx.at_work; }
protected:
	static inline int unpack(context &ctx, pbuf &pbf, sbuf &sbf);
};

/* template specialization */
template <> inline U8 mpak::cast_to<U8>(pbuf &pbf) {
	return GET_8(pbf.cur_p());
}
template <> inline U16 mpak::cast_to<U16>(pbuf &pbf) {
	return ntohs(GET_16(pbf.cur_p()));
}
template <> inline U32 mpak::cast_to<U32>(pbuf &pbf) {
	return ntohl(GET_32(pbf.cur_p()));
}
template <> inline U64 mpak::cast_to<U64>(pbuf &pbf) {
	return ntohll(GET_64(pbf.cur_p()));
}
template <> inline float mpak::cast_to<float>(pbuf &pbf) {
	if (sizeof(float) == sizeof(U64)) {
		U64 tmp = ntohll(GET_64(pbf.cur_p()));
		return *(reinterpret_cast<float *>(&tmp));
	}
	else if (sizeof(float) == sizeof(U32)) {
		U32 tmp = ntohl(GET_32(pbf.cur_p()));
		return *(reinterpret_cast<float *>(&tmp));
	}
	ASSERT(false);
	return 0.0f;
}
template <> inline double mpak::cast_to<double>(pbuf &pbf) {
	if (sizeof(double) == sizeof(U64)) {
		U64 tmp = ntohll(GET_64(pbf.cur_p()));
		return *(reinterpret_cast<double *>(&tmp));
	}
	else if (sizeof(double) == sizeof(U32)) {
		U32 tmp = ntohl(GET_32(pbf.cur_p()));
		return *(reinterpret_cast<double *>(&tmp));
	}
	ASSERT(false);
	return 0.0f;
}


/* inline implementation */
#if defined(CHECK_LENGTH)
#undef CHECK_LENGTH
#endif
#define CHECK_LENGTH(len)	if ((m_c + len) >= buffsize()) { 	\
	TRACE("length error %s(%u)\n", __FILE__, __LINE__);	\
	ASSERT(false); return NBR_ESHORT; }
inline int mpak::skip(U32 sz) {
	CHECK_LENGTH(sz);
	commit(sz);
	return curpos();
}
inline int mpak::pushnil() {
	CHECK_LENGTH(1); push(NIL_VALUE); return curpos();
}
inline int mpak::operator << (bool f) {
	CHECK_LENGTH(1);
	if (f) { push(BOOLEAN_TRUE); }
	else { push(BOOLEAN_FALSE); }
	return curpos();
}
inline int mpak::operator << (U8 u) {
	if (u <= P_FIXNUM_END) {
		CHECK_LENGTH(1);
		push(u);
	}
	else {
		CHECK_LENGTH(2);
		push(UINT_8BIT);
		push(u);
	}
	return curpos();
}
inline int mpak::operator << (S8 s) {
	if (((U8)s) >= N_FIXNUM_START) {
		CHECK_LENGTH(1);
		push(s);
	}
	else {
		CHECK_LENGTH(2);
		push(SINT_8BIT);
		push(s);
	}
	return curpos();
}
inline int mpak::operator << (U16 u) {
	CHECK_LENGTH(3); push(UINT_16BIT); push(u); return curpos();
}
inline int mpak::operator << (S16 s) {
	CHECK_LENGTH(3); push(SINT_16BIT); push(s); return curpos();
}
inline int mpak::operator << (U32 u) {
	CHECK_LENGTH(5); push(UINT_32BIT); push(u); return curpos();
}
inline int mpak::operator << (S32 s) {
	CHECK_LENGTH(5); push(SINT_32BIT); push(s); return curpos();
}
inline int mpak::operator << (U64 u) {
	CHECK_LENGTH(9); push(UINT_64BIT); push(u); return curpos();
}
inline int mpak::operator << (S64 s) {
	CHECK_LENGTH(9); push(SINT_64BIT); push(s); return curpos();
}
inline int mpak::operator << (float f) {
	CHECK_LENGTH(sizeof(float) + 1);
	push(FLOAT);
	char *p_f = (char *)(&f);
	if (sizeof(float) == sizeof(U64)) {
		push(GET_64(p_f));
	}
	else if (sizeof(float) == sizeof(U32)) {
		push(GET_32(p_f));
	}
	else {
		ASSERT(false);
		return NBR_ENOTSUPPORT;
	}
	return curpos();
}
inline int mpak::operator << (double f) {
	CHECK_LENGTH(sizeof(double) + 1);
	push(DOUBLE_TYPE);
	char *p_f = (char *)(&f);
	if (sizeof(double) == sizeof(U64)) {
		push(GET_64(p_f));
	}
	else if (sizeof(double) == sizeof(U32)) {
		push(GET_32(p_f));
	}
	else {
		ASSERT(false);
		return NBR_ENOTSUPPORT;
	}
	return curpos();
}
inline int mpak::operator << (const data &o) {
#if defined(verify)
#undef verify
#endif
#define verify(expr) if ((expr) < 0) { 							\
		TRACE(#expr" fails: @%s(%u)\n", __FILE__, __LINE__);	\
		return NBR_ESHORT; 										\
	}
	switch(o.kind()) {
	case NIL:
		verify(pushnil());
		return curpos();
	case BOOLEAN:
		verify(*this << o.operator bool());
		return curpos();
	case INTEGER:
		verify(*this << o.operator int());
		return curpos();
	case DOUBLE:
		verify(*this << o.operator double());
		return curpos();
	case BLOB: /*case STRING: */
		verify(mpak::push_raw(o.operator const char *(), o.len()));
		return curpos();
	case ARRAY: {
		for (size_t i = 0; i < o.size(); i++) {
			verify(*this << o.elem(i));
		}
	} 	return curpos();
	case MAP: {
		for (size_t i = 0; i < o.size(); i++) {
			verify(*this << o.key(i));
			verify(*this << o.val(i));
		}
	}	return curpos();
	default:
		ASSERT(false);
		return NBR_EINVAL;
	}
#undef verify
	return curpos();
}

inline int mpak::operator << (const char *s) {
	return mpak::push_string(s);
}

inline int mpak::push_raw_len(size_t l) {
	if (l <= 0x1F) {
		CHECK_LENGTH(1);
		push(((U8)(FIXRAW_MASK | (U8)l)));
	}
	else if (l <= 0xFFFF) {
		CHECK_LENGTH(3);
		push(RAW16);
		push(((U16)l));
	}
	else {
		CHECK_LENGTH(5);
		push(RAW32);
		push(((U32)l));
	}
	return curpos();
}
inline int mpak::push_raw_onlydata(const char *p, size_t l) {
	CHECK_LENGTH(l);
	util::mem::copy(curr_p(), p, l);
	commit(l);
	return curpos();
}
inline int mpak::push_raw(const char *p, size_t l) {
	int r = mpak::push_raw_len(l);
	if (r < 0) { return r; }
	return mpak::push_raw_onlydata(p, l);
}
inline int mpak::push_string(const char *str, int l) {
	if (l < 0) { l = util::str::length(str, 64 * 1024); }
	return mpak::push_raw(str, l + 1);	/* last '\0' also */
}
inline int mpak::push_array_len(size_t l) {
	if (l <= 0xF) {
		CHECK_LENGTH(1);
		push(((U8)(FIXARRAY_MASK | (U8)l)));
	}
	else if (l <= 0xFFFF) {
		CHECK_LENGTH(3);
		push(ARRAY16);
		push(((U16)l));
	}
	else {
		CHECK_LENGTH(5);
		push(ARRAY32);
		push(((U32)l));
	}
	return curpos();
}
inline int mpak::push_array(const data *a, size_t l) {
	int r = mpak::push_array_len(l);
	if (r < 0) { return r; }
	for (size_t s = 0; s < l; s++) {
		if ((*this << a[s]) < 0) { return NBR_ESHORT; }
	}
	return curpos();
}
inline int mpak::push_map_len(size_t l) {
	if (l <= 0xF) {
		CHECK_LENGTH(1);
		push(((U8)(FIXMAP_MASK | (U8)l)));
	}
	else if (l <= 0xFFFF) {
		CHECK_LENGTH(3);
		push(MAP16);
		push(((U16)l));
	}
	else {
		CHECK_LENGTH(5);
		push(MAP32);
		push(((U32)l));
	}
	return curpos();
}
/* size of a is l * 2 (key + value) */
inline int mpak::push_map(const data *a, size_t l) {
	int r = mpak::push_map_len(l);
	if (r < 0) { return r; }
	for (size_t s = 0; s < l; s++) {
		if ((*this << a[2 * s    ])) { return NBR_ESHORT; }
		if ((*this << a[2 * s + 1]) < 0) { return NBR_ESHORT; }
	}
	return curpos();
}

#undef CHECK_LENGTH

inline int mpak::unpack(context &ctx, pbuf &pbf, sbuf &sbf) {
	U8 b; context::stackmem *sp = &(ctx.stack[ctx.stack_height]);
	data *obj; size_t tmp;
	while (true) {
		switch(sp->status) {
		case context::INITIAL: {
			if (!pbf.readable()) { goto l_wait_next_buffer; }
			b = (U8)pbf.pop();
			if (b <= P_FIXNUM_END) {
				sp->d->set((S64)b);
				goto l_popstack;
			}
			else if ((b & ~0x1f) == FIXRAW_MASK) {
				sp->require_len = b & 0x1f;
				sp->status = context::PROCESS_DATA;
				sp->require_type = BLOB;
				goto l_keepstack;
			}
			else if ((b & ~0x0f) == FIXARRAY_MASK) {
				sp->require_len = b & 0x0f;
				sp->require_type = ARRAY;
				goto l_length_check;
			}
			else if ((b & ~0x0f) == FIXMAP_MASK) {
				sp->require_len = b & 0x0f;
				sp->require_type = MAP;
				goto l_length_check;
			}
			else if (b == NIL_VALUE) {
				sp->d->set_nil();
				goto l_popstack;
			}
			else if (b == BOOLEAN_FALSE) {
				sp->d->set(false);
				goto l_popstack;
			}
			else if (b == BOOLEAN_TRUE) {
				sp->d->set(true);
				goto l_popstack;
			}
			else if (b >= TABLE_START && b <= TABLE_END) {
				static struct dest {
					context::parse_state status_to;
					size_t rlen;
					U32 rtype;
				} table[] = {
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xc0)
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xc1)
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xc2)
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xc3)

					{ context::PROCESS_DATA, 0, NIL },	//reserved1(0xc4)
					{ context::PROCESS_DATA, 0, NIL },	//reserved2(0xc5)
					{ context::PROCESS_DATA, 0, NIL },	//reserved3(0xc6)
					{ context::PROCESS_DATA, 0, NIL },	//reserved4(0xc7)
					{ context::PROCESS_DATA, 0, NIL },	//reserved5(0xc8)
					{ context::PROCESS_DATA, 0, NIL },	//reserved6(0xc9)
					{ context::PROCESS_DATA, sizeof(float), DOUBLE },	//FLOAT
					{ context::PROCESS_DATA, sizeof(double), DOUBLE },	//DOUBLE_TYPE
					{ context::PROCESS_DATA, 1, INTEGER },	//UINT_8BIT
					{ context::PROCESS_DATA, 2, INTEGER },	//UINT_16BIT
					{ context::PROCESS_DATA, 4, INTEGER },	//UINT_32BIT
					{ context::PROCESS_DATA, 8, INTEGER },	//UINT_64BIT
					{ context::PROCESS_DATA, 1, INTEGER },	//SINT_8BIT
					{ context::PROCESS_DATA, 2, INTEGER },	//SINT_16BIT
					{ context::PROCESS_DATA, 4, INTEGER },	//SINT_32BIT
					{ context::PROCESS_DATA, 8, INTEGER },	//SINT_64BIT
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xd4)
					{ context::PROCESS_DATA, 0, NIL },	//dummy(0xd5)
					{ context::PROCESS_LENGTH, 2, DOUBLE },	//BIG_FLOAT_16
					{ context::PROCESS_LENGTH, 4, DOUBLE },	//BIG_FLOAT_32
					{ context::PROCESS_LENGTH, 2, INTEGER },//BIGINT_16
					{ context::PROCESS_LENGTH, 4, INTEGER },//BIGINT_32
					{ context::PROCESS_LENGTH, 2, BLOB },	//RAW16
					{ context::PROCESS_LENGTH, 4, BLOB },	//RAW32
					{ context::PROCESS_LENGTH, 2, ARRAY },	//ARRAY16
					{ context::PROCESS_LENGTH, 4, ARRAY },	//ARRAY32
					{ context::PROCESS_LENGTH, 2, MAP },	//MAP16
					{ context::PROCESS_LENGTH, 4, MAP },	//MAP32
				};
				dest *dst = table + (b & 0x1f);
				sp->status = dst->status_to;
				sp->require_len = dst->rlen;
				sp->require_type = dst->rtype;
				goto l_keepstack;
			}
			else if (N_FIXNUM_START <= b) {
				sp->d->set((S64)(S8)b);
				goto l_popstack;
			}
			else {
				ASSERT(false);
				goto l_error;
			}
		} break;
		case context::PROCESS_LENGTH: {
			if ((sp->require_len + pbf.ofs()) > pbf.last()) {
				/* more buffer need to received */
				goto l_wait_next_buffer;
			}
			else {
				tmp = sp->require_len;
				switch(tmp) {
				case 1: sp->require_len = cast_to<U8>(pbf); break;
				case 2: sp->require_len = cast_to<U16>(pbf); break;
				case 4: sp->require_len = cast_to<U32>(pbf); break;
				case 8: sp->require_len = cast_to<U64>(pbf); break;
				}
				pbf.add_parsed_ofs(tmp);
l_length_check:
				switch(sp->require_type) {
				case ARRAY: {
					if (sp->require_len == 0) {
						sp->d->init_array(sbf, 0);
						goto l_popstack;
					}
					if (sp->d->init_array(sbf, sp->require_len) < 0) {
						ASSERT(false);
						goto l_error;
					}
					sp->status = context::PROCESS_ARRAY;
					sp->n_pop_obj = 0;
					obj = sp->d->elem_new(0);
					MPAK_TRACE("ary: obj: depth:%u, n_pop:%u, d:%p, obj:%p\n",
						ctx.stack_height, sp->n_pop_obj, sp->d, obj);
					goto l_pushstack;
				} break;
				case MAP: {
					if (sp->require_len == 0) {
						sp->d->init_map(sbf, 0);
						goto l_popstack;
					}
					if (sp->d->init_map(sbf, sp->require_len) < 0) {
						ASSERT(false);
						goto l_error;
					}
					sp->status = context::PROCESS_MAP;
					sp->n_pop_obj = 0;
					obj = sp->d->key_new(0);
					MPAK_TRACE("map: obj: depth:%u, n_pop:%u, d:%p, obj:%p(%p)\n",
						ctx.stack_height, sp->n_pop_obj, sp->d, obj, sp->d->val_new(0));

					goto l_pushstack;
				} break;
				default:
					sp->status = context::PROCESS_DATA;
					goto l_keepstack;
				}
			}
		} break;
		case context::PROCESS_DATA: {
			if ((sp->require_len + pbf.ofs()) > pbf.last()) {
				/* more buffer need to received */
				goto l_wait_next_buffer;
			}
			else {
				switch(sp->require_type) {
				case INTEGER: {
					switch(sp->require_len) {
					case 1: sp->d->set(static_cast<S64>(cast_to<U8>(pbf))); break;
					case 2: sp->d->set(static_cast<S64>(cast_to<U16>(pbf))); break;
					case 4: sp->d->set(static_cast<S64>(cast_to<U32>(pbf))); break;
					case 8: sp->d->set(static_cast<S64>(cast_to<U64>(pbf))); break;
					default: ASSERT(false); goto l_error; /* from BIGINT_16/32. now not support. */
					}
					pbf.add_parsed_ofs(sp->require_len);
					goto l_popstack;
				};
				case DOUBLE: {
					if (sp->require_len == sizeof(double)) {
						sp->d->set(cast_to<double>(pbf));
					}
					else {
						ASSERT(sp->require_len == sizeof(float));
						/* BLACKMAGIC: c-style cast! */
						sp->d->set((double)(cast_to<float>(pbf)));
					}
					pbf.add_parsed_ofs(sp->require_len);
					goto l_popstack;
				} break;
				case BLOB:	/* zero-copy */ {
					sp->d->set(pbf.cur_p(), sp->require_len);
					pbf.add_parsed_ofs(sp->require_len);
					goto l_popstack;
				} break;
				default:
					ASSERT(false);
					goto l_error;
				}
			}
		} break;
		case context::PROCESS_ARRAY: {
			if (++sp->n_pop_obj < sp->require_len) {
				/* more buffer need to received */
				obj = sp->d->elem_new(sp->n_pop_obj);
				MPAK_TRACE("elm: obj: depth:%u, n_pop:%u, d:%p, obj:%p\n",
										ctx.stack_height, sp->n_pop_obj, sp->d, obj);
				goto l_pushstack;
			}
			else {
				goto l_popstack;
			}
		} break;
		case context::PROCESS_MAP: {
			sp->status = context::PROCESS_MAP_VAL;
			obj = sp->d->val_new(sp->n_pop_obj);
			MPAK_TRACE("val: obj: depth:%u, n_pop:%u, d:%p, obj:%p(%p)\n",
				ctx.stack_height, sp->n_pop_obj, sp->d, obj, sp->d->key_new(sp->n_pop_obj));
			goto l_pushstack;
		} break;
		case context::PROCESS_MAP_VAL: {
			if (++sp->n_pop_obj < sp->require_len) {
				/* more buffer need to received */
				sp->status = context::PROCESS_MAP;
				obj = sp->d->key_new(sp->n_pop_obj);
				MPAK_TRACE("key: obj: depth:%u, n_pop:%u, d:%p, obj:%p(%p)\n",
					ctx.stack_height, sp->n_pop_obj, sp->d, obj, sp->d->val_new(sp->n_pop_obj));
				goto l_pushstack;
			}
			else {
				goto l_popstack;
			}
		} break;
		}
l_wait_next_buffer:
		return UNPACK_CONTINUE;
l_pushstack:
		ctx.stack_height++;
		if (ctx.stack_height >= UNPACK_STACK_DEPTH) {
			ASSERT(false);	/* too much nested */
			return UNPACK_PARSE_ERROR;
		}
		sp = &(ctx.stack[ctx.stack_height]);
		sp->d = obj;
		sp->status = context::INITIAL;
		continue;
l_popstack:
		if (ctx.stack_height == 0) {
			ctx.at_work.set_sbuf(sbf.refer(pbf));
			init_context(ctx);
			return pbf.last() == pbf.ofs() ? UNPACK_SUCCESS : UNPACK_EXTRA_BYTES;
		}
		ctx.stack_height--;
		sp = &(ctx.stack[ctx.stack_height]);
		MPAK_TRACE("nxt: depth:%u, sp->d:%p\n", ctx.stack_height, sp->d);
		ASSERT(sp->status != context::INITIAL);
		continue;	/* at least one more loop execution for checking end of array/map
		 	 	 	 	(if double pop stack, come here again) */
l_keepstack:
		continue;
l_error:	/* TODO : should rollback? => I think discard every thing is better. */
		return UNPACK_PARSE_ERROR;
	}
	return UNPACK_CONTINUE;
}
}
}
}

#endif
