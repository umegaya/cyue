/****************************************************************
 * macro.h : storing error
 * 2008/07/14 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__MACRO_H__)
#define __MACRO_H__

#include <memory.h>
#include <arpa/inet.h>

#if defined(_DEBUG)
#include <assert.h>
#include <stdio.h>
#if !defined(ASSERT)
#define ASSERT	assert
#endif
#if !defined(VERIFY)
#define VERIFY	ASSERT
#endif
#if !defined(TRACE)
#if defined(__ANDROID_NDK__)
#include <android/log.h>
#define TRACE(...)	__android_log_print(ANDROID_LOG_INFO, "yue", __VA_ARGS__)
#else
#define TRACE(...)	fprintf(stderr, __VA_ARGS__)
#endif
#endif
#else	/* _DEBUG */
#if !defined(ASSERT)
#define ASSERT(...)
#endif
#if !defined(VERIFY)
#define VERIFY(f)	(f)
#endif
#if !defined(TRACE)
#define TRACE(...) //fprintf(stdout, __VA_ARGS__)
#endif
#endif	/* _DEBUG */

#if !defined(__FUNC__)
#if defined (__GNUC__)
#define	__FUNC__	__func__
#else
/* safety */
#define	__FUNC__	""
#endif
#endif

#define TO_UTIME(sec)	((UTIME)((sec) * 1000 * 1000))
#if !defined(STATIC__JOIN)
#define STATIC__JOIN(a, b)	a ## b
#endif
#if !defined(STATIC_ASSERT)
#define STATIC_ASSERT(f)	typedef char STATIC__JOIN(ZZZZZZZZZ_static___assert_, __LINE__)[(1 / ((f) ? 1 : 0))]
#endif

#define OR_DIE(exp) { 	\
	if (!(exp)) { TRACE(#exp" fail at %s(%u)\n", __FILE__, __LINE__); ASSERT(false); exit(-1); } \
}

/* test macro */
#define verify_true(expr) if (!(expr)) { 							\
		TRACE(#expr" fails: @%s(%u) is not true\n", __FILE__, __LINE__);	\
		return false; 										\
	}


#define verify_false(expr) if ((expr)) { 							\
		TRACE(#expr" fails: @%s(%u) is not false\n", __FILE__, __LINE__);	\
		return false; 										\
	}

#define verify_success(expr) if ((expr) < 0) { 							\
		TRACE(#expr" fails: @%s(%u) is not success\n", __FILE__, __LINE__);	\
		return false; 										\
	}

#define LOG(...) printf(__VA_ARGS__)
#define DIE(fmt, ...) do { 									\
	LOG("%s(%u):" fmt, __FILE__, __LINE__, __VA_ARGS__); 	\
	ASSERT(false); exit(-1); 								\
} while(0)

/* numerical conversion */
#define SAFETY_ATOI(p, r	)		\
{									\
	if (str::atoi(p, r, 256)) { return NBR_EFORMAT; }	\
}

/* configuration */
#define CONFIG_START()	if (false) {}
#define CONFIG_STR(value, k, v)	else if (util::str::cmp(#value, k)) {			\
				util::str::copy((value), sizeof((value)), v, sizeof((value))); 	\
				return NBR_OK;													\
			}
#define CONFIG_INT(value, k, v)	else if (util::str::cmp(#value, k)) {			\
				return util::str::atoi(v, (value));							\
			}
#define CONFIG_END() else {														\
				TRACE("invalid config: %s %s", k, v); ASSERT(false); 			\
				return NBR_ECONFIGURE;											\
			}

/* error in ctor */
#define CONSTRUCTOR_ERROR	throw std::bad_alloc()

#if !defined(FORBID_COPY)
#define FORBID_COPY(__classname)	\
	private:						\
		void operator = (const __classname &obj); \
		__classname(const __classname &obj);
#endif

/* function argument */
#define UNUSED(val)

/* array element count */
#define countof(a)	 (sizeof(a) / sizeof(a[0]))

/* support routine & macros */
#define __NBR_BIG_ENDIAN__ (1)
#define __NBR_LITTLE_ENDIAN__ (2)

/* android ARM build mode */
#define NDK_ARM_BUILD_thumb (1)
#define NDK_ARM_BUILD_arm (2)

#define NDK_CPU_ARCH_armeabi (1)
#define NDK_CPU_ARCH_armeabi_v7a (2)
#define NDK_CPU_ARCH_mips (3)
#define NDK_CPU_ARCH_x86 (4)

#if defined(__NBR_IOS__)
#include "iosdefs.h"
#endif

#if __NBR_BYTE_ORDER__ == __NBR_BIG_ENDIAN__
static inline U64 ntohll(U64 n) { return n; }
static inline U64 htonll(U64 n) { return n; }
#else
static inline U64 ntohll(U64 n) { return (((U64)ntohl(n)) << 32) + ntohl(n >> 32); }
static inline U64 htonll(U64 n) { return (((U64)htonl(n)) << 32) + htonl(n >> 32); }
#endif

#if defined (_DEBUG)
#include	<stdio.h>
#define	__RETURN				fprintf(stderr, "PACKETMACRO ERROR : %s(%d)\n", __FILE__, __LINE__); return
#else	/* defined (_PACKETMACRO_DEBUG) */
#define	__RETURN				return
#endif	/* defined (_PACKETMACRO_DEBUG) */

/* binary access */
/* memory access support */
#if __CPU_ARCH__==NDK_CPU_ARCH_armeabi || __CPU_ARCH__==NDK_CPU_ARCH_armeabi_v7a
#define __STRICT_MEMALIGN__
#endif
#if defined(__STRICT_MEMALIGN__)
#define GET_8(ptr)              (*((U8 *)(ptr)))
inline U16 GET_16(const void *ptr)	{int __i = 0; U16 v; U8 *pv = (U8 *)&v; do {pv[__i] = ((U8 *)(ptr))[__i];} while(++__i < 2); return v;} 
inline U32 GET_32(const void *ptr)	{int __i = 0; U32 v; U8 *pv = (U8 *)&v; do {pv[__i] = ((U8 *)(ptr))[__i];} while(++__i < 4); return v;}
inline U64 GET_64(const void *ptr) 	{int __i = 0; U64 v; U8 *pv = (U8 *)&v; do {pv[__i] = ((U8 *)(ptr))[__i];} while(++__i < 8); return v;}

#define SET_8(ptr,v)    (*((U8 *)(ptr)) = v)
inline void SET_16(void *ptr,U16 v)   {int __i = 0; U8 *pv = (U8 *)&v; do {((U8 *)(ptr))[__i] = pv[__i];} while(++__i < 2);}
inline void SET_32(void *ptr,U32 v)   {int __i = 0; U8 *pv = (U8 *)&v; do {((U8 *)(ptr))[__i] = pv[__i];} while(++__i < 4);}
inline void SET_64(void *ptr,U64 v)   {int __i = 0; U8 *pv = (U8 *)&v; do {((U8 *)(ptr))[__i] = pv[__i];} while(++__i < 8);}
#else
#define GET_8(ptr)		(*((U8 *)(ptr)))
#define GET_16(ptr)		(*((U16 *)(ptr)))
#define GET_32(ptr)		(*((U32 *)(ptr)))
#define GET_64(ptr)		(*((U64 *)(ptr)))
#define SET_8(ptr,v)	(*((U8 *)(ptr)) = v)
#define SET_16(ptr,v)	(*((U16 *)(ptr)) = v)
#define SET_32(ptr,v)	(*((U32 *)(ptr)) = v)
#define SET_64(ptr,v)	(*((U64 *)(ptr)) = v)
#endif

/* unpack support */
#define	POP_START(data, len)	const char *_data=(data); int _len=(len); int _ofs=0;
#define POP_LEN()				_ofs
#define	POP_BUF()				((char *)_data)
#define POP_REMAIN()			(_len - _ofs)
#define POP_SKIP(n)				(_ofs += n, _data += n)
#define POP_HAS_REMAIN()		(_len > _ofs)
#define	POP_LENGTHCHECK(x)		if ((_ofs + (int)sizeof(x)) > _len) {__RETURN -1;}
#define	POP_8(i)				{POP_LENGTHCHECK(i); i = *_data;_data+=sizeof(i); _ofs+=sizeof(i);}
#define	POP_16(i)				{U16 __ts;  POP_LENGTHCHECK(__ts); U32 _i = 0; do{((char *)(&__ts))[_i] = _data[_i];}while(++_i < 2); i = ntohs(__ts); _data+=sizeof(__ts); _ofs+=sizeof(__ts);}
#define	POP_32(i)				{U32 __tl;  POP_LENGTHCHECK(__tl); U32 _i = 0; do{((char *)(&__tl))[_i] = _data[_i];}while(++_i < 4); i = ntohl(__tl); _data+=sizeof(__tl); _ofs+=sizeof(__tl);}
#define	POP_64(i)				{U64 __tll; POP_LENGTHCHECK(__tll);U32 _i = 0; do{((char *)(&__tll))[_i] = _data[_i];}while(++_i < 8); i = ntohll(__tll); _data+=sizeof(__tll); _ofs+=sizeof(__tll);}
#define POP_8A(ia, ialen)		{U16 __alen; POP_16(__alen); if (__alen > 0) { if ((S32)__alen>(S32)(ialen)) {__RETURN -3;} if ((_ofs + __alen) > _len) {__RETURN -1;} memcpy((ia),_data,__alen); _data+=__alen; _ofs+=__alen; ialen=__alen;}}
#define	POP_16A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U16(ia[__ii]); }}
#define	POP_32A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U32(ia[__ii]); }}
#define	POP_64A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U64(ia[__ii]); }}
#define	POP_STR(buf, buf_len)	{char *__strt=(char *)buf; size_t __i=0; while (*_data!='\0') {*__strt=*_data; _data++; __strt++; _ofs++; __i++; POP_LENGTHCHECK(char); if (buf_len<=__i+1) {break;}} *__strt='\0';_ofs++;_data++;}
#define	POP_STR2(buf, buf_len,__i)	{char *__strt=(char *)buf; __i=0; while (*_data!='\0') {*__strt=*_data; _data++; __strt++; _ofs++; __i++; POP_LENGTHCHECK(char); if (buf_len<=__i+1) {break;}} *__strt='\0';_ofs++;_data++;}
#define POP_MEM(buf, buf_len)   {if ((_ofs + (int)buf_len) > _len) { __RETURN -1; } memcpy(buf, _data, buf_len); _data += buf_len; _ofs += buf_len; }

/* pack support */
#define	PUSH_START(work, max)	char *_work=(work); int _outmax=(max); int _ofs=0;
#define	PUSH_LEN()				_ofs
#define	PUSH_BUF()				_work
#define PUSH_REMAIN()			(_outmax - _ofs)
#define PUSH_SKIP(n)			(_ofs += n, _work += n)
#define	PUSH_LENGTHCHECK(x)		if ((_ofs + (int)sizeof(x)) > _outmax) {__RETURN -1;}
#define PUSH_8(i)				{PUSH_LENGTHCHECK(U8); *_work = (U8)i; _work+=sizeof(U8); _ofs+=sizeof(U8);}
#define	PUSH_16(i)				{U16 __ts;	PUSH_LENGTHCHECK(__ts); __ts = htons((U16)(i));   U32 _i = 0; do{_work[_i] = ((char *)(&__ts))[_i];}while(++_i < 2); _work+=sizeof(__ts); _ofs+=sizeof(__ts);}
#define PUSH_32(i)				{U32 __tl;  PUSH_LENGTHCHECK(__tl); __tl = htonl((U32)(i)); U32 _i = 0; do{_work[_i] = ((char *)(&__tl))[_i];}while(++_i < 4); _work+=sizeof(__tl); _ofs+=sizeof(__tl);}
#define	PUSH_64(i)				{U64 __tll; PUSH_LENGTHCHECK(__tll); __tll = htonll((U64)(i));U32 _i = 0; do{_work[_i] = ((char *)(&__tll))[_i];}while(++_i < 8); _work+=sizeof(__tll); _ofs+=sizeof(__tll);}
#define	PUSH_8A(a, alen)		{PUSH_16((U16)alen); if ((((size_t)_ofs) + (alen)) > ((size_t)_outmax)) {__RETURN -1;} memcpy(_work, (a), (alen)); _work+=(long)(alen); _ofs+=(long)(alen); }
#define	PUSH_16A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_16(a[__ii]); }}
#define	PUSH_32A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_32(a[__ii]); }}
#define	PUSH_64A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_16(a[__ii]); }}
#define	PUSH_STR(str)			{S32 __n=(S32)strlen(str); if ((_ofs+__n+1)>_outmax) {__RETURN -1;} memcpy(_work, str, __n); _work+=__n; *_work='\0'; _work++; _ofs+=(__n+1);}
#define PUSH_MEM(buf, buf_len)  {if ((_ofs + (int)buf_len) > _outmax) { __RETURN -1; } memcpy(_work, buf, buf_len); _work += buf_len; _ofs += buf_len; }

#endif	/* __MACRO_H__ */
