/***************************************************************
 * util.h : utilities
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see lisence.txt for lisence detail
 ****************************************************************/
#include "util.h"
#include "osdep.h"
#include "syscall.h"
#define MEXP	19937	/* mersenne twister degree */
#include "exlib/sfmt/SFMT.c"
#include <ctype.h>
#if defined(_ENABLE_BACKTRACE)
#include "execinfo.h"
#endif
#if defined(_NO_STD_SWAP)
namespace std {
template <class T>
inline void swap(T &a, T &b) {
	T tmp = a; a = b; b = tmp;
}
}
#endif
#include "exlib/cityhash/city.cc"

#define STR_ERROUT OSDEP_ERROUT

namespace yue {
namespace util {
#include "exlib/b64/include/b64/cencode.h"
#include "exlib/b64/include/b64/cdecode.h"
#include "exlib/b64/src/cencode.c"
#include "exlib/b64/src/cdecode.c"
namespace base64 {
int encode(const char* plaintext_in, int length_in, char* code_out) {
	base64_encodestate state;
	base64_init_encodestate(&state);
	int len = base64_encode_block(plaintext_in, length_in, code_out, &state);
	len += base64_encode_blockend(code_out + len, &state);
	/* this encode routine append \n on last of result string. (why?) */
	code_out[len - 1] = '\0';
	return len;
}
int decode(const char* code_in, const int length_in, char* plaintext_out) {
	base64_decodestate state;
	base64_init_decodestate(&state);
	int len = base64_decode_block(code_in, length_in, plaintext_out, &state);
	plaintext_out[len] = '\0';
	return len;
}
}
namespace sha1 {
#define VOID void
#define SIZE_T size_t
#define UINT32 U32
#define UINT64 U64
#define UINT8 U8
#include "exlib/sha1/SHA1cc.h"
#define SHA1_CC_NO_HEADER_INCLUDE
#include "exlib/sha1/SHA1cc.c"
#undef SHA1_CC_NO_HEADER_INCLUDE
int encode(const char* data, int len, U8 result[20]) {
	SHA1_Context_t c;
	SHA1cc_Init(&c);
	SHA1cc_Update(&c, data, len);
	SHA1cc_Finalize(&c, result);
	return NBR_OK;
}
#undef VOID
#undef SIZE_T
#undef UINT32
#undef UINT64
#undef UINT8
}
/***************************************************************
 * util
 ****************************************************************/
int init() {
	int r;
	if ((r = math::rand::init()) < 0) { return r; }
	return NBR_OK;
}
void fin() {
	math::rand::fin();
}
int static_init() {
	int r;
	if ((r = time::init()) < 0) { return r; }
	return NBR_OK;
}
void static_fin() {
	time::fin();
}


/***************************************************************
 * util::time
 ****************************************************************/
namespace str {
int
_atobn(const char* str, S64 *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0LL;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		if ('0' <= *_s && *_s <= '9') {
			(*i) = (*i) * 10LL + (unsigned long long)((*_s) - '0');
		}
		else {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n",str);
			return LASTERR;
		}
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}
	if (minus) {
		(*i) = -1LL * (*i);
	}

	return NBR_OK;
}

int
_atoi(const char* str, int *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		if ('0' <= *_s && *_s <= '9') {
			(*i) = (*i) * 10 + (int)((*_s) - '0');
		}
		else {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			ASSERT(false);
			return LASTERR;
		}
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}

	return NBR_OK;
}

int
htoi(const char* str, int *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		int num = -1;
		if ('0' <= *_s && *_s <= '9') {
			num = (int)((*_s) - '0');
		}
		if ('a' <= *_s && *_s <= 'f') {
			num = (int)(((*_s) - 'a') + 10);
		}
		if ('A' <= *_s && *_s <= 'F') {
			num = (int)(((*_s) - 'A') + 10);
		}
		if (num < 0) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			return LASTERR;
		}
		(*i) = (*i) * 16 + num;
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}

	return NBR_OK;
}

int
htobn(const char* str, S64 *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		long long num = -1;
		if ('0' <= *_s && *_s <= '9') {
			num = (S64)((*_s) - '0');
		}
		if ('a' <= *_s && *_s <= 'f') {
			num = (S64)(((*_s) - 'a') + 10);
		}
		if ('A' <= *_s && *_s <= 'F') {
			num = (S64)(((*_s) - 'A') + 10);
		}
		if (num < 0) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			return LASTERR;
		}
		(*i) = (*i) * 16 + num;
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}
	return NBR_OK;
}

static inline char *
get_oneword_utf8(char *ibuf, int *ilen, char *obuf, int *olen)
{
	/* ported from UnicodeConvert.cpp */
	if (ibuf == NULL || obuf == NULL || *ilen <= 0 || *olen <= 0) {
		return 0;
	}

	U8	ch = (U8)ibuf[0];
	if (ch <= 0x7F) {
		*obuf++ = *ibuf++;
		*olen = 1;
		*ilen -= 1;
		return ibuf;
	}
	else if ((ch & 0xE0) == 0xE0) {
		// �ｽ�ｽR�ｽ�ｽo�ｽ�ｽC�ｽ�ｽgUTF-8�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�鯉ｼ托ｿｽ�ｽo�ｽ�ｽC�ｽ�ｽg�ｽ�ｽ�ｽ�ｽ
		if (*ilen >= 3 && *olen >= 3) {
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*olen = 3;
			*ilen -= 3;
			return ibuf;
		}
	}
	else if ((ch & 0xC0) == 0xC0) {
		// �ｽ�ｽQ�ｽ�ｽo�ｽ�ｽC�ｽ�ｽgUTF-8�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�ｽ�鯉ｼ托ｿｽ�ｽo�ｽ�ｽC�ｽ�ｽg�ｽ�ｽ�ｽ�ｽ
		if (*ilen >= 2 && *olen >= 2) {
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*olen = 2;
			*ilen -= 2;
			return ibuf;
		}
	}
	else {
		ASSERT(false);
	}

	*olen = 0;
	return NULL;
}

size_t
utf8_copy(char *dst, int dlen, const char *src, int smax, int len)
{
	char *s_now = (char *)src, *d_now = dst;
	int dwork, swork;
	while(*s_now) {
		dwork = dlen, swork = smax;
		s_now = get_oneword_utf8(s_now, &swork, d_now, &dwork);
		d_now += dwork;
		dlen -= dwork;
		smax = swork;
		len--;
		if (len <= 0 || dlen <= 1 || smax <= 0) {
			break;
		}
	}
	*d_now = '\0';
	return (src - s_now);
}

const char *
divide_tag_and_val(char sep, const char *line, char *tag, int taglen)
{
	const char *w = line;
	while(*w) {
		if (*w == sep) {
			*tag = '\0';
			return w + 1;
		}
		*tag++ = *w++;
		if ((int)(w - line) > taglen) {
			ASSERT(false);
			return NULL;
		}
	}

	*tag = '\0';
	ASSERT(false);
	return NULL;
}

const char *
divide(const char *sep, const char *line, char *tag, int *tlen)
{
	const char *w = line, *tmp = tag;
	const char *s;
	int maxlen = *tlen;
	while(*w) {
		s = sep;
		while (*s) {
			if (*w == *s) {
				*tag = '\0';
				return w + 1;
			}
			s++;
		}
		*tag++ = *w++;
		if ((int)(w - line) > maxlen) {
			ASSERT(false);
			return NULL;
		}
	}

	*tag = '\0';
	*tlen = (w - line);
	return tmp;
}


int
cmp_nocase(const char *a, const char *b, U32 len)
{
	const char *wa = a, *wb = b;
	while(1) {
		if (*wa == 0) {
			if (*wb == 0) {
				return 0;
			}
			else {
				return -1;
			}
		}
		else {
			if (tolower(*wa) != tolower(*wb)) {
				if (tolower(*wa) > tolower(*wb)) {
					return 1;
				}
				else {
					return -1;
				}
			}
		}
		wa++; wb++;
		if (wa - a > len) {
			return 0;
		}
	}
	ASSERT(false);
	return 0;
}

int
cmp_tail(const char *a, const char *b, int len, int max)
{
	const char *wa = a, *wb = b;
	while (*wa) {
		wa++;
		if ((wa - a) > max) {
			return 1;
		}
	}
	while (*wb) {
		wb++;
		if ((wb - b) > max) {
			return -1;
		}
	}
	if ((wb - b) < len || (wa - a) < len) {
		return util::str::_cmp(a, (wa - a), b, (wb - b));
	}
	wb -= len;
	wa -= len;
	int cnt = 0;
	while (cnt < len) {
		if (wb[cnt] != wa[cnt]) {
			if (wb[cnt] > wa[cnt]) {
				return -1;
			}
			else {
				return 1;
			}
		}
		cnt++;
	}
	return 0;
}

int
parse_url(const char *in, int max, char *host, U16 *port, char *url)
{
	const char *w = in;
	int i = 0;
	char tok[65536];
	while(*w) {
		switch(i) {
			case 0:	/* host */
				{
					char *t = host;
					*port = 80;
					while(*w) {
						*t++ = *w++;
						if (*w == ':') { i = 1; *t = '\0'; w++; break; }
						if (*w == '/') { i = 2; *t = '\0'; break; }
					}
					/* reach to null terminator */
					if (*w == '\0') { return NBR_OK; }
				}
				break;
			case 1:	/* port */
				{
					char *t = tok;
					while(*w != '/') {
						*t++ = *w++;
						/* reach to null terminator */
						if (*w == '\0') { break; }
					}
					*t = '\0';
					int _port;
					if (util::str::_atoi(tok, &_port, 256) < 0) {
						STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", in );
						return LASTERR;
					}
					*port = (U16)_port;
					i++;
					if (*w == '\0') { return NBR_OK; }
				}
				break;
			case 2:	/* URL */
				{
					char *t = url;
					while(*w) { *t++ = *w++; }
					*t = '\0';
					if (*(t - 1) == '\n') { *(t - 1) = '\0'; }
				}
				break;
		}
		if ((w - in) > max) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", in );
			return LASTERR;
		}
	}
	return i > 0 ? NBR_OK : NBR_EFORMAT;
}

char *
chop(char *buffer)
{
	char *w = buffer;
	while(*w) {
		w++;
		if ((w - buffer) > (int)(0x7FFFFFFF)) {
			return NULL;
		}
	}
	if ((w - buffer) > 0 && *(w - 1) == '\n') {
		*(w - 1) = '\0';
		return buffer;
	}
	else if ((w - buffer) > 1 && *(w - 1) == '\n' && *(w - 2) == '\r') {
		*(w - 1) = *(w - 2) = '\0';
		return buffer;
	}
	return buffer;
}

const char*
rchr(const char *in, char sep, int max)
{
	const char *w = in, *p = NULL;
	while(*w) {
		if (*w == sep) {
			p = w;
		}
		if ((w - in) > max) {
			return NULL;
		}
		w++;
	}
	return p;
}

static const char SEP_EQUAL = '=';
static const char SEP_AMP = '&';

int
parse_http_req_str(const char *req, const char *tag, char *buf, int buflen)
{
	const char *w = req, *prev = w, *mark = NULL;
	while (*w) {
		if (*w == SEP_EQUAL && mark == NULL) {
			mark = w;
		}
		if (*w == SEP_AMP) {
			if (mark == NULL) {
				continue;
			}
			if (util::mem::cmp(prev, tag, (mark - prev)) == 0) {
				if (1 < (w - mark) && (w - mark) <= buflen) {
					util::mem::copy(buf, mark + 1, (w - mark - 1));
					buf[(w - mark - 1)] = '\0';
					return (w - mark - 1);
				}
				else {
					return NBR_ENOTFOUND;
				}
			}
			prev = (w + 1);
			mark = NULL;
		}
		w++;
		if ((w - req) > 65536) {
			return NBR_ENOTFOUND;
		}
	}
	if (mark != NULL && mark != req) {	/* <tag=value> pattern */
		if (util::mem::cmp(prev, tag, (mark - prev)) == 0) {
			if (1 < (w - mark) && (w - mark) <= buflen) {
				util::mem::copy(buf, mark + 1, (w - mark - 1));
				buf[(w - mark - 1)] = '\0';
				return (w - mark - 1);
			}
			else {
				return NBR_ENOTFOUND;
			}
		}
	}
	return NBR_ENOTFOUND;
}

int
parse_http_req_int(const char *req, const char *tag, int *buf)
{
	int r;
	char buffer[65536];
	if ((r = parse_http_req_str(req, tag, buffer, sizeof(buffer))) < 0) {
		return r;
	}
	if ((r = util::str::_atoi(buffer, buf, sizeof(buffer))) < 0) {
		return r;
	}
	return NBR_OK;
}

int
parse_http_req_bigint(const char *req, const char *tag, long long *buf)
{
	int r;
	char buffer[65536];
	if ((r = parse_http_req_str(req, tag, buffer, sizeof(buffer))) < 0) {
		return r;
	}
	if ((r = util::str::_atobn(buffer, buf, sizeof(buffer))) < 0) {
		return r;
	}
	return NBR_OK;
}
}


/***************************************************************
 * util::time
 ****************************************************************/
namespace time {
/* clock related */
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
typedef struct timeval ostime_t;
#elif defined(__NBR_WINDOWS__)
typedef struct ostime {
	int		tm_round;	/* how many times GetTickCount() returns 0? */
	unsigned int	tm_tick;	/* retval of GetTickCount() */
} ostime_t;
#endif
static ostime_t	g_start; /* initial time */
static UTIME	g_clock; /* passed time */

/*
static UTIME clock_from_ostime(ostime_t *t)
{
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	return (((UTIME)t->tv_sec) * 1000000 + (UTIME)t->tv_usec);
#elif defined(__NBR_WINDOWS__)
	return (UTIME)((t->tm_round << 32 + t->tm_tick) * 1000);
#endif
}
*/

static void clock_get_ostime(ostime_t *t)
{
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	gettimeofday(t, NULL);
#elif defined(__NBR_WINDOWS__)
	t->tm_round = (int)(g_clock >> 32 / 1000);
	t->tm_tick = GetTickCount();
#endif
}

static UTIME clock_get_time_diff(ostime_t *t)
{
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	ASSERT(t->tv_sec > g_start.tv_sec || t->tv_usec >= g_start.tv_usec);
	return (UTIME)(((UTIME)(t->tv_sec - g_start.tv_sec)) * 1000000 +
			(int)(t->tv_usec - g_start.tv_usec));
#elif defined(__NBR_WINDOWS__)
	return (UTIME)((t->tm_tick - g_start.tm_tick) +
		((t->tm_round - g_start.tm_round) << 32));
#else
	ASSERT(false);
	return 0;
#endif
}

int init()
{
	clock_get_ostime(&g_start);
	return NBR_OK;
}

void fin() {}

void update_clock()
{
	ostime_t	ost;
	clock_get_ostime(&ost);
	g_clock = clock_get_time_diff(&ost);
	ASSERT((g_clock & 0xFFFFFFFF00000000LL) != 0xFFFFFFFF00000000LL);
}

UTIME clock()
{
	return g_clock;
}

UTIME now()
{
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	ostime_t ost;
	gettimeofday(&ost, NULL);
#if defined(_DEBUG)
	{
		UTIME ut = (((UTIME)ost.tv_sec) * 1000 * 1000 + ((UTIME)ost.tv_usec));
		ASSERT(ut >= 0x00000000FFFFFFFFLL);
		return ut;
	}
#else
	return (((UTIME)ost.tv_sec) * 1000 * 1000 + ((UTIME)ost.tv_usec));
#endif
#else
	return 0LL;
#endif
}

int sleep(NTIME nanosec) {
	int r; struct timespec ts, rs, *pts = &ts, *prs = &rs, *tmp;
	ts.tv_sec = nanosec / (1000 * 1000 * 1000);
	ts.tv_nsec = nanosec % (1000 * 1000 * 1000);
resleep:
	//TRACE("start:%p %u(s) + %u(ns)\n", pts, pts->tv_sec, pts->tv_nsec);
	if (0 == (r = nanosleep(pts, prs))) {
		return NBR_OK;
	}
	//TRACE("left:%p %u(s) + %u(ns)\n", prs, prs->tv_sec, prs->tv_nsec);
	/* signal interrupt. keep on sleeping */
	if (r == -1 && errno == EINTR) {
		tmp = pts; pts = prs; prs = tmp;
		goto resleep;
	}
	return NBR_ESYSCALL;
}

msgid_generator<U32> logical_clock::m_gen;

}

/***************************************************************
 * util::math
 ****************************************************************/
namespace math {
namespace rand {
int init()
{
	//return NBR_OK;
	//maybe now this routine is linux specific.
	union {
		struct {
			U8	hwaddr[6];
			U16	pid;
			U32	time;
		}	src;
		U32	data[3];
	}	seed;
	int r;

	if ((r = util::syscall::get_macaddr(DEFAULT_IF, seed.src.hwaddr)) != NBR_OK) {
		TRACE("get_macaddr: %d\n", r);
		return NBR_EINTERNAL; //(ERROR,INTERNAL,"get_macaddr: %d\n", r);
		//if no eth0, but continue
	}
	seed.src.pid = getpid();
	seed.src.time = time::unix_time();
	init_by_array(seed.data, 3);

	return NBR_OK;
}

void fin()
{
	cleanup_rand();
}
}

int prime(int given)
{
	int i, j;
	unsigned char *p;

	if (given <= 3) {
		return given;
	}

	/* this may cause problem if given is too huge */
	p = (unsigned char *)util::mem::alloc(given);
	if (!p) {
		TRACE( "_prime:cannot alloc work memory:size=%d", given );
		return -1;
	}
	util::mem::bzero(p, given);/* p[N] correspond to N + 1 is prime or not */
	for (i = 2; i <= given; i++) {
		if (i > (int)(given/i)) {
//			TRACE("_prime:break at %u,(%u)", i, (int)(given/i));
			break;
		}
		if (p[i - 1]) {
			continue;
		}
		else {
			for(j = (i * 2); j <= given; j += i) {
				p[j - 1] = 1;
			}
		}
	}

	for (i = (given - 1); i > 0; i--) {
		if (p[i] == 0) {
			util::mem::free(p);
			//TRACE("_prime:is %u\n", i + 1);
			return i + 1;
		}
	}

	// no prime number!? you're kidding!!
	ASSERT(false);
	util::mem::free(p);
	return -1;
}

U32 rand32()
{
	return gen_rand32();
}

U64 rand64()
{
	return ((U64)gen_rand32() << 32) | gen_rand32();
}
}
#if defined(_ENABLE_BACKTRACE)
namespace debug {
/*
             > deeper
	stack = [0|1|2|3|....|start|...|start + num|...|bottom]
	if start > bottom or start < 0, show error message (because start == 0
	if start + num > bottom, show from start to bottom.
*/
template <class PRINTER>
void _bt(int start, int num, PRINTER &p) {
	if (start < 0) { printf("start depth is too small to show something\n"); return; }
	start++;

	const int MAX_TRACES = 1024; // 隴ｬ�ｼ驍城亂笘�ｹｧ荵昴○郢ｧ�ｿ郢晢ｿｽ縺醍ｹ晁ｼ釆樒ｹ晢ｽｼ郢晢ｿｽ�ｽ隴幢ｿｽ�､�ｧ陋溷玄辟�
	void* traceBuffers[MAX_TRACES]; // 郢ｧ�ｹ郢ｧ�ｿ郢晢ｿｽ縺醍ｹ晁ｼ釆樒ｹ晢ｽｼ郢晢ｿｽ竏育ｸｺ�ｮ郢ｧ�｢郢晏ｳｨﾎ樒ｹｧ�ｹ郢ｧ蜻茨ｿｽ驍擾ｿｽ
	int bottom = (backtrace(traceBuffers, MAX_TRACES) - 1);

	char** traceStrings = backtrace_symbols(traceBuffers, N);
	if (!traceStrings) { printf("Error get trace strings\n"); return; }

	if (bottom < start) { 
		printf("start depth is too deep to show something bottom: %s\n", traceStrings[bottom]); 
		util::mem::free(traceStrings);
		return; 
	}

	int end = (start + num);
	if (end > bottom) { end = bottom; }

	for (int i = start; i < end; ++i) {
		if (p(i, traceStrings[i]) < 0) {
			break;
		}
	}
	util::mem::free(traceStrings);  // backtrace_symbols邵ｺ�ｮ隰鯉ｽｻ郢ｧ髮�ｿｽ邵ｺ�ｯ陷ｻ�ｼ邵ｺ�ｳ陷�ｽｺ邵ｺ諤懶ｿｽ邵ｺ�ｧ髫暦ｽ｣隰ｾ�ｾ邵ｺ蜷ｶ�狗ｸｺ阮吮�
}

namespace printer {
struct console {
	int operator () (int stack_index, const char *trace_string) {
		printf("%d:%s\n", stack_index, trace_string);
		return NBR_OK;
	}
};
struct string {
	int m_size, m_curr;
	char *m_buff;
	int operator () (int stack_index, const char *trace_string) {
		if (m_size <= m_curr) {
			return NBR_ESHORT;
		}
		m_curr += snprintf(m_buff + m_curr, m_size - m_curr, "%d:%s\n", stack_index, trace_string);
		return NBR_OK;
	}
};
} //end of namespace printer

void bt(int start, int num) {
	printer::console p;
	_bt(start, num, p);
}
void btstr(char *buff, int size, int start, int num) {
	printer::string p = { size, 0, buff };
	_bt(start, num, p);
}
} //end of namespace debug
#endif
} //end of namespace util

} //end of namespace yue
