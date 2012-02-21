/***************************************************************
 * http.h : http receiver fsm and sender implementation
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__HTTP_H__)
#define __HTTP_H__

#include "timerfd.h"
#include <ctype.h>

namespace yue {
namespace net {
namespace http {
#include "httprc.h"
/*
 * sender
 */
struct sender {
	static const int CHUNK_SIZE = 1024;
	struct sendable {
		static const int NO_LIMIT = -1;
		virtual ~sendable() {}
		virtual int will_send(int) { return 0; }
		/* >= 0 bytes to send, < 0 error */
		virtual int send(DSCRPTR, int) { return 0; };
		/* if no more buffer remains to send, return true otherwise false */
		virtual bool finished() { return true; };
	};
	struct buffer_sendable : public sendable {
		const char *m_buff; U32 m_max, m_curr;
		~buffer_sendable() {}
		inline buffer_sendable(const char *p, U32 l) : 
			m_buff(p), m_max(l), m_curr(0) {}
		virtual int will_send(int limit) { 
			return limit < 0 ? (m_max - m_curr) : limit; 
		}
		virtual int send(DSCRPTR fd, int willsend) {
			int n_write = (m_max - m_curr);
			if (n_write > willsend) { n_write = willsend; }
			if ((n_write = sender::send(fd, m_buff + m_curr, n_write)) < 0) { 
				return n_write; 
			}
			m_curr += n_write;
			return n_write;
		};
		virtual bool finished() { return (m_max <= m_curr); };
	};
public:
	static array<sender> ms_sbufs;
	DSCRPTR m_fd;
	sendable *m_sendbuf;
	sender(DSCRPTR fd, sendable *s) : m_fd(fd), m_sendbuf(s) {}
	~sender() { if (m_sendbuf) { delete m_sendbuf; } }
public:
#define log(LVL, FMT, ...) TRACE(FMT, __VA_ARGS__)
	static int init(int maxfd) {
		if (ms_sbufs.initialized()) { return NBR_OK; }
		return ms_sbufs.init(maxfd, -1, util::opt_threadsafe | util::opt_expandable) ?
				NBR_OK : NBR_EMALLOC;
	}
	static void fin() { ms_sbufs.fin(); }
	static inline int send(DSCRPTR fd, const char *p, size_t l) { 
		return tcp_send(fd, p, l, 0); 
	}
	static inline int send_lf(DSCRPTR fd) { return send(fd, "\r\n", 2); }
	static inline int get(DSCRPTR fd, const char *url, const char *hd[], const char *hv[],
			int n_hd, bool chunked/* = false */)
	{
		return send_request_common(fd, "GET", url, NULL, hd, hv, n_hd, false);
	}

	static inline int post(DSCRPTR fd, const char *url, const char *hd[], const char *hv[],
			const char *body, int blen, int n_hd, bool chunked/* = false */)
	{
		return send_request_common(fd, "POST", url, 
			new buffer_sendable(body, blen), hd, hv, n_hd, chunked);
	}

	static int send_request_common(DSCRPTR fd, const char *method,
			const char *url, sendable *body,
			const char *hd[], const char *hv[], int n_hd, bool chunked/* = false */)
	{
		char data[1024], host[1024], path[1024];
		U16 port;
		int r, len;
		if ((r = util::str::parse_url(url, sizeof(host), host, &port, path)) < 0) {
			return r;
		}
		if (path[0] == '\0') {
			len = util::str::printf(data, sizeof(data),
					"%s / HTTP/1.1\r\n"
					"Host: %s\r\n",
					method, host);
		}
		else {
			len = util::str::printf(data, sizeof(data),
					"%s %s HTTP/1.1\r\n"
					"Host: %s:%hu\r\n",
					method, path, host, port);
		}
		if ((len = send(fd, data, len)) < 0) {
			log(ERROR, "send request fail (%d)\n", len);
			return len;
		}
		TRACE("request header: <%s", data);
		if ((r = send_header(fd, hd, hv, n_hd, chunked)) < 0) {
			log(ERROR, "send emptyline1 fail (%d)\n", r);
			return r;
		}
		len += r;
		if ((r = send_lf(fd)) < 0) {
			return r;
		}
		len += r;
		TRACE(":%u>\n", len);
		if (body) {
			int willsend = body->will_send(sendable::NO_LIMIT);
			if (chunked && willsend > CHUNK_SIZE) {
	//		TRACE("body transfer as chunk (%d)\n", blen);
				return entry(fd, body) >= 0 ? len : NBR_EEXPIRE;
			}
			else {
				if ((r = body->send(fd, CHUNK_SIZE)) < 0) {
					log(ERROR, "send body fail (%d)\n", r);
					delete body;
					return r;
				}
				len += r;
				if ((r = send(fd, "\r\n\r\n", 4)) < 0) {
					log(ERROR, "send last crlfcrlf fail (%d)\n", r);
					delete body;
					return r;
				}
				len += r;
				delete body;
			}
		}
		return len;
	}

	static int send_header(DSCRPTR fd, const char *hd[], const char *hv[], int n_hd, bool chunked)
	{
		int r, len = 0, hl;
		char header[4096];
		if (chunked) {
			const char chunk_header[] = "Transfer-Encoding: chunked\r\n";
			if ((r = send(fd, chunk_header, sizeof(chunk_header) - 1)) < 0) {
				return r;
			}
			TRACE("%s", chunk_header);
		}
		for (int i = 0; i < n_hd; i++) {
			hl = util::str::printf(header, sizeof(header) - 1,
					"%s: %s\r\n", hd[i], hv[i]);
			if ((r = send(fd, header, hl)) < 0) {
				return r;
			}
			TRACE("%s", header);
			len += r;
		}
		return len;
	}

	static int send_result_code(DSCRPTR fd, result_code rc, int cf, int v)
	{
		char buffer[1024];
		int len = sprintf(buffer, "HTTP/%u.%u %03u\r\n\r\n", v / 10, v % 10, rc);
		int r = send(fd, buffer, len);
		if (r > 0 && cf) {
			tcp_close(fd);
		}
		return r;

	}

	static int send_result_and_body(DSCRPTR fd, result_code rc, int v, 
			sendable *s, const char *mime)
	{
		char hd[1024], *phd = hd;
		int hl = 0, hl2, r, tl = 0;
		hl += util::str::printf(phd + hl, sizeof(hd) - hl, "HTTP/%u.%u %03u\r\n",
				v / 10, v % 10, rc);
		hl += util::str::printf(phd + hl, sizeof(hd) - hl, "Content-Type: %s\r\n", mime);
		int limit = CHUNK_SIZE;
		if (limit < hl) {
			return NBR_ESHORT;
		}
		if ((r = send(fd, hd, hl)) < 0) { return r; }
		tl += r;
		int bl = s->will_send(sendable::NO_LIMIT);
		hl2 = util::str::printf(hd, sizeof(hd) - 1, "Content-Length: %u\r\n\r\n", bl);
		if (limit >= (hl + hl2 + bl)) {
			if ((r = send(fd, hd, hl2)) < 0) { return r; }
			tl += r;
			if ((r = s->send(fd, sendable::NO_LIMIT)) < 0) { return r; }
			tl += r;
			tcp_close(fd);
		}
		else {
	//		TRACE("body length too long %d, chunk sending\n", bl);
			const char chunk_header[] = "Transfer-Encoding: chunked\r\n\r\n";
			if ((r = send(fd, chunk_header, sizeof(chunk_header) - 1)) < 0) { return r; }
			tl += r;
			if ((r = entry(fd, s)) < 0) { return r; }
			return tl + r;
		}
		return tl;
	}

	static inline int send_handshake_request(DSCRPTR fd,
		const char *host, const char *key, const char *origin, const char *protocol) {
		/*
		 * send client handshake
		 * ex)
		 * GET / HTTP/1.1
		 * Host: server.example.com
		 * Upgrade: websocket
		 * Connection: Upgrade
		 * Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
		 * Origin: http://example.com
		 * Sec-WebSocket-Protocol: chat, superchat
		 * Sec-WebSocket-Version: 13
		 */
		 char buff[CHUNK_SIZE], proto_header[CHUNK_SIZE];
		 if (protocol) {
			 util::str::printf(proto_header, sizeof(proto_header),
					 "Sec-WebSocket-Protocol: %s", protocol);
		 }
		 size_t sz = util::str::printf(buff, sizeof(buff), 
				 "GET / HTTP/1.1\r\n"
				 "Host: %s\r\n"
				 "Upgrade: websocket\r\n"
				 "Connection: Upgrade\r\n"
				 "Sec-WebSocket-Key: %s\r\n"
				 "Origin: %s\r\n"
				 "%s\r\n"
				 "Sec-WebSocket-Version: 13\r\n\r\n",
				 host, key, origin, proto_header);
		 return send(fd, buff, sz);
	}
	static inline int send_handshake_response(DSCRPTR fd, const char *accept_key) {
		/*
		 * send server handshake
		 * ex)
		 * HTTP/1.1 101 Switching Protocols
		 * Upgrade: websocket
		 * Connection: Upgrade
		 * Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
		 */
		 char buff[CHUNK_SIZE];
		 size_t sz = util::str::printf(buff, sizeof(buff), 
				 "HTTP/1.1 101 Switching Protocols\r\n"
				 "Upgrade: websocket\r\n"
				 "Connection: Upgrade\r\n"
				 "Sec-WebSocket-Accept: %s\r\n\r\n",
				 accept_key);
		 return send(fd, buff, sz);
	}

	static handler::timerfd::task *m_t;
	static int entry(DSCRPTR fd, sendable *s) {
		if (!m_t) { return NBR_EINVAL; }
		return (ms_sbufs.alloc(fd, s) != NULL) ? 
			s->will_send(sendable::NO_LIMIT) : NBR_EEXPIRE;
	}
	static int sender_task_iterater(sender *s, int) {
		if (s->send_body() != NBR_OK) {
			ms_sbufs.free(s);
		}
		return NBR_OK;
	}
	static int sender_task(handler::timerfd::task *) {
		int tmp;
		int (*fn)(sender *, int) = sender_task_iterater;
		return ms_sbufs.iterate(fn, tmp);
	}

	int send_body()
	{
		char length[16];
		int n_send, n_limit = (CHUNK_SIZE
				- 10/* chunk max is FFFFFFFF\r\n */ - 2/* for last \r\n after body */
				- 5/* if send finish, we need to sent 0\r\n\r\n also */);
		if (n_limit <= 0) {
			ASSERT(false);
			return NBR_ESHORT;
		}
		n_send = m_sendbuf->will_send(sendable::NO_LIMIT);
	//	TRACE("writable limit = %d\n", n_limit);
		if (n_send > n_limit) {
			n_send = n_limit;
		}
		int n_length = util::str::printf(length, sizeof(length) - 1, "%x\r\n", n_send);
		if ((n_length = send(m_fd, length, n_length)) < 0) {
			log(ERROR, "send_body: too long chunk prefix %u\n", n_length);
			return n_length;
		}
		if (n_send <= 0) { /* chunk transfer but body is 0 byte */
			if ((n_send = send_lf(m_fd)) < 0) {/* send last empty line */
				return n_send;
			}
	//		TRACE("no more chunk need to transfer\n");
			return 0;
		}
		if ((n_send = m_sendbuf->send(m_fd, n_limit)) < 0) {
			log(ERROR, "send_body: too long body %u\n", n_send);
			return n_send;
		}
		if ((n_send = send_lf(m_fd)) < 0) {
			return n_send;
		}
		if (m_sendbuf->finished()) {
			/* send last 0 byte chunk and empty line */
			if ((n_send = send(m_fd, "0\r\n\r\n", 5)) < 0) {
				return n_send;
			}
	//		TRACE("no more chunk need to transfer2\n");
			return NBR_SUCCESS;
		}
		return NBR_OK;
	}
};

/*
 * receiver finite state machine
 */
class fsm {
public:
	enum state { /* http fsm state */
		state_invalid,
		/* recv state */
		state_recv_header,
		state_recv_body,
		state_recv_body_nochunk,
		state_recv_bodylen,
		state_recv_footer,
		state_recv_comment,
		state_recv_finish,
		/* upgrade to websocket */
		state_websocket_establish,
		/* error */
		state_error = -1,
	};
	enum {
		version_1_0 = 10,
		version_1_1 = 11,
	};
	static const U16 lflf = 0x0a0a;
	static const U16 crlf = 0x0d0a;
	static const U32 crlfcrlf = 0x0d0a0d0a;
	static const int MAX_HEADER = 64;
protected:
	struct context {
		U8		method, version, n_hd, padd;
		S16		state, res;
		const char	*hd[MAX_HEADER], *bd;
		U32		bl;
		U16		hl[MAX_HEADER];
	}	m_ctx;
	U32 m_max, m_len;
	DSCRPTR m_fd;
	const char *m_buf;
	char m_p[0];
public:
	fsm() {}
	~fsm() {}
	state 	append(char *b, int bl);
	void 	reset(DSCRPTR fd, U32 chunk_size);
public:
	void	set_state(state s) { m_ctx.state = s; }
	state	get_state() const { return (state)m_ctx.state; }
	bool	error() const { return get_state() == state_error; }
	void	setrc(result_code rc) { m_ctx.res = (S16)rc; }
	void	setrc_from_close_reason(int reason);
public:	/* for processing reply */
	int			version() const { return m_ctx.version; }
	char 		*hdrstr(const char *key, char *b, int l, int *outlen = NULL) const;
	bool		hashdr(const char *key) {
		char tok[256];
		return hdrstr(key, tok, sizeof(tok)) != NULL;
	}
	int 		hdrint(const char *key, int &out) const;
	const char 	*body() const { return m_ctx.bd; }
	result_code		rc() const { return (result_code)m_ctx.res; }
	int			bodylen() const { return m_ctx.bl; }
	int			url(char *b, int l);
public:	/* for sending */
	inline int	get(const char *url, const char *hd[], const char *hv[],
			int n_hd, bool chunked = false) {
		return sender::get(m_fd, url, hd, hv, n_hd, chunked);
	}
	inline int	post(const char *url, const char *hd[], const char *hv[],
			const char *body, int blen, int n_hd, bool chunked = false) {
		return sender::post(m_fd, url, hd, hv, body, blen, n_hd, chunked);
	}
	inline int send_request_common(const char *method,
			const char *url, const char *body, int blen,
			const char *hd[], const char *hv[], int n_hd, bool chunked) {
		return sender::send_request_common(m_fd, method, url, 
			new sender::buffer_sendable(body, blen), hd, hv, n_hd, chunked);
	}
	inline int send_result_code(result_code rc, int cf/* close after sent? */) {
		return sender::send_result_code(m_fd, rc, cf, version());
	}
	inline int send_result_and_body(result_code rc, const char *b, int bl, const char *mime) {
		return sender::send_result_and_body(m_fd, rc, version(), 
			new sender::buffer_sendable(b, bl), mime);
	}
	inline int send_lf() { return sender::send(m_fd, "\r\n", 2); }
	int send_handshake_request(const char *host, const char *key, 
		const char *origin, const char *protocol) {
		return sender::send_handshake_request(m_fd, host, key, origin, protocol);
	}
	inline int send_handshake_response(const char *accept_key) {
		return sender::send_handshake_response(m_fd, accept_key);
	}

protected: /* receiving */
	state 	recv_header();
	state	recv_body_nochunk();
	state 	recv_body();
	state 	recv_bodylen();
	state 	recv_footer();
	state	recv_comment();
	state	recv_ws_frame();
protected:
	int		recv_lflf() const;
	int		recv_lf() const;
	char 	*current() { return m_p + m_len; }
	const char *current() const { return m_p + m_len; }
	context	&recvctx() { return m_ctx; }
	context &sendctx() { return m_ctx; }
	result_code	putrc();
};

void
fsm::reset(DSCRPTR fd, U32 chunk_size)
{
	m_fd = fd;
	m_buf = m_p;
	m_len = 0;
	m_max = chunk_size;
	m_ctx.version = version_1_1;
	m_ctx.n_hd = 0;
	m_ctx.state = state_recv_header;
}

fsm::state
fsm::append(char *b, int bl)
{
//	TRACE("append %u byte <%s>\n", bl, b);
	state s = get_state();
	char *w = b;
	U32 limit = (m_max - 1);
	while (s != state_error && s != state_recv_finish) {
		if (m_len >= limit) {
			s = state_error;
			break;
		}
		m_p[m_len++] = *w++;
		m_p[m_len] = '\0';
#if defined(_DEBUG)
//		if ((m_len % 100) == 0) { TRACE("."); }
//		TRACE("recv[%u]:%u\n", m_len, s);
#endif
		switch(s) {
		case state_recv_header:
			s = recv_header(); break;
		case state_recv_body:
			s = recv_body(); break;
		case state_recv_body_nochunk:
			s = recv_body_nochunk(); break;
		case state_recv_bodylen:
			s = recv_bodylen(); break;
		case state_recv_footer:
			s = recv_footer(); break;
		case state_recv_comment:
			s = recv_comment(); break;
		case state_websocket_establish:
			goto end;
		default:
			break;
		}
		if ((w - b) >= bl) { break; }
	}
end:
	recvctx().state = (U16)s;
	return s;
}

char*
fsm::hdrstr(const char *key, char *b, int l, int *outlen) const
{
	for (int i = 0; i < m_ctx.n_hd; i++) {
		const char *k = key;
		const char *p = m_ctx.hd[i];
		/* key name comparison by case non-sensitive */
		while (*k && tolower(*k) == tolower(*p)) {
			if ((k - key) > m_ctx.hl[i]) {
				ASSERT(false);
				return NULL;	/* key name too long */
			}
			k++; p++;
		}
		if (*k) {
			continue;	/* key name and header tag not match */
		}
		else {
			/* seems header is found */
			while (*p) {
				/* skip [spaces][:][spaces] between [tag] and [val] */
				if (*p == ' ' || *p == ':') { p++; }
				else { break; }
				if ((m_ctx.hd[i] - p) > m_ctx.hl[i]) {
					ASSERT(false);
					return NULL;	/* too long space(' ') */
				}
			}
			char *w = b;
			while (*p) {
				*w++ = *p++;
				if ((w - b) >= l) {
					ASSERT(false);
					return NULL;	/* too long header paramter */
				}
			}
			if (outlen) {
				*outlen = (w - b);
			}
			*w = 0;	/* null terminate */
			return b;
		}
	}
	return NULL;
}

int
fsm::hdrint(const char *key, int &out) const
{
	char b[256];
	if (NULL != hdrstr(key, b, sizeof(b))) {
		int r;
		if ((r = util::str::_atoi(b, &out, 256)) < 0) {
			return r;
		}
		return NBR_OK;
	}
	return NBR_ENOTFOUND;
}

int
fsm::recv_lf() const
{
	const char *p = current();
//	if (m_len > 1) {
//		TRACE("now last 2byte=<%s:%u>%u\n", (p - 2), GET_16(p - 2), htons(crlf));
//	}
	if (m_len > 2 && GET_16(p - 2) == htons(crlf)) {
		return 2;
	}
	if (m_len > 1 && *(p - 1) == '\n') {
		return 1;
	}
	return 0;
}

int
fsm::recv_lflf() const
{
	const char *p = current();
	if (m_len > 4 && GET_32(p - 4) == htonl(crlfcrlf)) {
		return 4;
	}
	if (m_len > 2 && GET_16(p - 2) == htons(lflf)) {
		return 2;
	}
	return 0;
}

fsm::state
fsm::recv_header()
{
	char *p = current();
	int nlf, tmp;
	if ((nlf = recv_lf())) {
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		tmp = nlf;
		for (;tmp > 0; tmp--) {
			*(p - tmp) = '\0';
		}
		if ((p - nlf) == m_buf) {
			int cl; char tok[256];
			/* get result code */
			m_ctx.res = putrc();
			/* if content length is exist, no chunk encoding */
			if (hdrint("Content-Length", cl) >= 0) {
				recvctx().bd = p;
				recvctx().bl = cl;
				return state_recv_body_nochunk;
			}
			/* if chunk encoding, process as chunk */
			else if (hdrstr("Transfer-Encoding", tok, sizeof(tok)) != NULL &&
						util::mem::cmp(tok, "chunked", sizeof("chunked") - 1) == 0) {
				m_buf = recvctx().bd = p;
				recvctx().bl = 0;
				return state_recv_bodylen;
			}
			else if (hdrstr("Sec-WebSocket-Key", tok, sizeof(tok)) ||
				hdrstr("Sec-WebSocket-Accept", tok, sizeof(tok))) {
				return state_websocket_establish;
			}
			else if (rc() == HRC_OK){
				return state_error;
			}
			else { return state_recv_finish; }
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf) - nlf;
			m_buf = p;
			recvctx().n_hd++;
		}
		else {	/* too much header. */
			return state_error;
		}
	}
	return state_recv_header;
}

fsm::state
fsm::recv_body()
{
	int nlf;
	if ((nlf = recv_lf())) {
		/* some stupid web server contains \n in its response...
		 * so we check actual length is received */
		int n_diff = (recvctx().bd + recvctx().bl) - (m_p + m_len - nlf);
		if (n_diff > 0) {
			/* maybe \r\n will come next */
			return state_recv_body;
		}
		else if (n_diff < 0) {
			/* it should not happen even if \n is contained */
			return state_error;
		}
		m_len -= nlf;
		m_buf = current();
		return state_recv_bodylen;
	}
	return state_recv_body;
}

fsm::state
fsm::recv_body_nochunk()
{
	int diff = (recvctx().bd + recvctx().bl) - (m_p + m_len);
	if (diff > 0) {
		return state_recv_body_nochunk;
	}
	else if (diff < 0) {
		return state_error;
	}
	return state_recv_finish;
}

fsm::state
fsm::recv_bodylen()
{
	char *p = current();
	state s = state_recv_bodylen;

	int nlf;
	if ((nlf = recv_lf())) {
		s = state_recv_body;
	}
	else if (*p == ';') {
		/* comment is specified after length */
		nlf = 1;
		s = state_recv_comment;
	}
	if (s != state_recv_bodylen) {
		int cl;
		for (;nlf > 0; nlf--) {
			*(p - nlf) = '\0';
		}
		if (util::str::htoi(m_buf, &cl, (p - m_buf)) < 0) {
			return state_error;
		}
		/* 0-length chunk means chunk end -> next footer */
		if (cl == 0) {
			m_buf = p;
			return state_recv_footer;
		}
		recvctx().bl += cl;
		m_len -= (p - m_buf);
	}
	return s;
}

fsm::state
fsm::recv_footer()
{
	char *p = current();
	int nlf, tmp;
	if ((nlf = recv_lf())) {
		tmp = nlf;
		for (;tmp > 0; tmp--) {
			*(p - tmp) = '\0';
		}
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		if ((p - nlf) == m_buf) {
			return state_recv_finish;
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf) - nlf;
			*p = '\0';
			m_buf = p;
			recvctx().n_hd++;
		}
		else {	/* too much footer + header. */
			return state_error;
		}
	}
	return state_recv_footer;
}

fsm::state
fsm::recv_comment()
{
	int nlf;
	if ((nlf = recv_lf())) {
		char *p = current();
		m_len -= (p - m_buf);
		return state_recv_body;
	}
	return state_recv_comment;
}

int
fsm::url(char *b, int l)
{
	const char *w = m_ctx.hd[0];
	/* skip first spaces */
	while (*w != ' ') {
		w++;
		if ((w - m_ctx.hd[0]) > m_ctx.hl[0]) {
			return NBR_EFULL;
		}
		/* reach to end of string: format error */
		if (*w == '\0') { return NBR_EFORMAT; }
	}
	w++;
	if (*w == '/') { w++; }
	char *wb = b;
	while (*w != ' ') {
		*wb++ = *w++;
		if ((wb - b) > l) {
			return NBR_EFULL;
		}
		if (*w == '\0') { return NBR_EFORMAT; }
	}
	*wb = '\0';
	return wb - b;
}

result_code
fsm::putrc()
{
	const char *w = m_ctx.hd[0], *s = w;
	w += 5;	/* skip first 5 character (HTTP/) */
	if (util::mem::cmp(w, "1.1", sizeof("1.1") - 1) == 0) {
		m_ctx.version = 11;
		w += 3;
	}
	else if (util::mem::cmp(w, "1.0", sizeof("1.0") - 1) == 0) {
		m_ctx.version = 10;
		w += 3;
	}
	else {
		return HRC_ERROR;
	}
	char tok[256];
	char *t = tok;
	while(*w) {
		w++;
		if (*w != ' ') { break; }
		if ((w - s) > m_ctx.hl[0]) {
			return HRC_ERROR;
		}
	}
	while(*w) {
		if (*w == ' ') { break; }
		*t++ = *w++;
		if ((w - s) > m_ctx.hl[0]) {
			return HRC_ERROR;
		}
		if ((unsigned int )(t - tok) >= sizeof(tok)) {
			return HRC_ERROR;
		}
	}
	int sc;
	*t = '\0';
	if (util::str::_atoi(tok, &sc, sizeof(tok)) < 0) {
		return HRC_ERROR;
	}
	return (result_code)sc;
}
}
}
}
#endif//__HTTP_H__
