/***************************************************************
 * popen.hpp : communicate with other process
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "osdep.h"
#include "util.h"
#include "http.h"


namespace yue {
namespace net {
struct ws_connection {
	/* web socket frame struct */
	/*---------------------------------------------------------------------------
	   	  0                   1                   2                   3
	      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	     +-+-+-+-+-------+-+-------------+-------------------------------+
	     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
	     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
	     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
	     | |1|2|3|       |K|             |                               |
	     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	     |     Extended payload length continued, if payload len == 127  |
	     + - - - - - - - - - - - - - - - +-------------------------------+
	     |                               |Masking-key, if MASK set to 1  |
	     +-------------------------------+-------------------------------+
	     | Masking-key (continued)       |          Payload Data         |
	     +-------------------------------- - - - - - - - - - - - - - - - +
	     :                     Payload Data continued ...                :
	     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
	     |                     Payload Data continued ...                |
	     +---------------------------------------------------------------+

	------------------------------------------------------------------------------*/
	struct frame {
		struct header {
		protected:
			union {
				U16 bits;
				struct { /* for GCC, we can use this but not portable */
					U8 opcode:4, rsv3:1, rsv2:1, rsv1:1, fin:1;
					U8 payload_len:7, mask:1;
				} quick_look;
			} data;
		public:
			/* we should do like below. */
			inline bool fin() const { return (data.bits & (1 << 7)); }
			inline bool rsv1() const { return (data.bits & (1 << 6)); }
			inline bool rsv2() const { return (data.bits & (1 << 5)); }
			inline bool rsv3() const { return (data.bits & (1 << 4)); }
			inline int opcode() const { return (data.bits & 0x000F); }
			inline bool mask() const { return (data.bits & (1 << 15)); }
			inline int payload_len() const { return ((data.bits & 0x7F00) >> 8); }
			inline void set_controls(bool f, bool m, U8 opc) {
				data.bits = 0;
				if (f) { data.bits |= (1 << 7); }
				if (m) { data.bits |= (1 << 15); }
				data.bits |= (opc & 0x0F);
			}
			inline void set_payload_len(U8 len) {
				data.bits |= ((len & 0x7F) << 8);
			}
		};
		union {
			header h;
			struct {
				U16 padd;
				U8 masking_key[4];
				U8 payload_data[0];
			} mask;
			struct {
				U16 padd;
				U8 payload_data[0];
			} nomask;
			struct {
				U16 padd;
				U16 ext_payload_len;
				U8 masking_key[4];
				U8 payload_data[0];
			} mask_0x7E;
			struct {
				U16 padd;
				U16 ext_payload_len;
				U8 payload_data[0];
			} nomask_0x7E;
			struct {
				U16 padd;
				U16 ext_payload_len[4];
				U8 masking_key[4];
				U8 payload_data[0];
			} mask_0x7F;
			struct {
				U16 padd;
				U16 ext_payload_len[4];
				U8 payload_data[0];
			} nomask_0x7F;
		} ext;

		inline U8 get_opcode() const { return ext.h.opcode(); }
		inline bool masked() const { return ext.h.mask(); }
	};
	static const U32 MAX_ADDR_LEN = 255;
	static const U32 CONTROL_FRAME_MAX = 125;
	static const U32 READSIZE = 512;
	struct control_frame {
		char m_buff[CONTROL_FRAME_MAX];
		U8 m_len, padd[2];
		control_frame() : m_len(0) {}
		inline int suck_out(DSCRPTR fd, size_t remain) {
			int r; 
			if ((r = sys_read(fd, m_buff + m_len, remain)) <= 0) {
				return r;
			}
			m_len += r;
			return m_len;
		}
	};
	enum state {
		state_init,
		state_client_handshake,
		state_client_handshake_2,
		state_server_handshake,
		state_established,
		state_recv_frame,
		state_recv_mask,
		state_recv_mask_0x7E,
		state_recv_mask_0x7F,
		state_recv,
		state_recv_0x7E,
		state_recv_0x7F,
	};
	enum opcode {
		opcode_continuation_frame, //*  %x0 denotes a continuation frame
		opcode_text_frame,	//*  %x1 denotes a text frame
		opcode_binary_frame,	//*  %x2 denotes a binary frame
		//*  %x3-7 are reserved for further non-control frames
		reserved_non_control_frame1,
		reserved_non_control_frame2,
		reserved_non_control_frame3,
		reserved_non_control_frame4,
		reserved_non_control_frame5,

		opcode_connection_close,	//*  %x8 denotes a connection close
		opcode_ping,	//*  %x9 denotes a ping
		opcode_pong,	// *  %xA denotes a pong
		//*  %xB-F are reserved for further control frames
		reserved_control_frame1,
		reserved_control_frame2,
		reserved_control_frame3,
		reserved_control_frame4,
	};
public:
	static util::array<control_frame> m_ctrl_frames;
	net::address m_a;
	U8 m_state, m_flen, m_mask_idx, padd;
	union {
		U32 m_key[4];
		char m_key_ptr[16];
	};
	control_frame *m_ctrl_frame;
	U64 m_read;
	union {
		frame m_frame;
		char m_frame_buff[sizeof(frame)];
	};
	http::fsm m_sm;
public:
	ws_connection() : m_state(state_init), m_sm() {}
	~ws_connection() {}
	static inline int init(int maxfd) {
		if (!g_wsock.init(maxfd, maxfd, sizeof(http::fsm) + READSIZE, 
			util::opt_threadsafe | util::opt_expandable)) {
			return NBR_EMALLOC;
		}
		return m_ctrl_frames.init(maxfd / 10, -1, 
			util::opt_expandable | util::opt_threadsafe) ?
			NBR_OK : NBR_EMALLOC;
	}
	static inline ws_connection *from(DSCRPTR fd) { return g_wsock.find(fd); }
	static inline ws_connection *alloc(DSCRPTR fd) { return g_wsock.alloc(fd); }
	static inline void free(DSCRPTR fd) { g_wsock.erase(fd); }
	inline void init_frame() { m_flen = 0; m_read = 0; m_mask_idx = 0; }
	inline void init_key() {
		m_key[0] = util::math::rand32();
		m_key[1] = util::math::rand32();
		m_key[2] = util::math::rand32();
		m_key[3] = util::math::rand32();
	}
	static inline int sys_read(DSCRPTR fd, char *p, size_t l) { return ::read(fd, p, l); }
	static inline int sys_write(DSCRPTR fd, const char *p, size_t l) { return ::write(fd, p, l); }
	static inline char *mask_payload(char *p, size_t l, U32 mask, U8 &mask_idx) {
		char *endp = (p + l);
		if (mask_idx > 0) {
			while (endp > p && mask_idx < sizeof(mask)) {
				*p = ((*p) ^ (reinterpret_cast<U8 *>(&mask))[mask_idx]);
				p++; mask_idx++;
			}
			if (mask_idx >= sizeof(mask)) {
				mask_idx = 0;
			}
		}
		while ((endp - p) >= (int)sizeof(U32)) {
			SET_32(p, (GET_32(p) ^ mask));
			p += sizeof(mask);
		}
		size_t remain = (endp - p);
		if (remain > 0) {
			for (; p < endp; p++) {
				mask_idx = (remain - (endp - p));
				*p = ((*p) ^ (reinterpret_cast<U8 *>(&mask))[mask_idx]);
			}
			mask_idx++;
		}
		return (endp - l);
	}
	inline state analyze_frame(size_t &over_read_length) {
		if (m_flen < sizeof(U16)) {
			return state_recv_frame;
		}
		if (m_frame.ext.h.mask()) {
			if (m_frame.ext.h.payload_len() == 0x7F) {
				if (m_flen < (sizeof(m_frame.ext.mask_0x7F))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.mask_0x7F)));
				return state_recv_mask_0x7F;
			}
			else if (m_frame.ext.h.payload_len() == 0x7E) {
				if (m_flen < (sizeof(m_frame.ext.mask_0x7E))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.mask_0x7E)));
				return state_recv_mask_0x7E;
			}
			else {
				if (m_flen < (sizeof(m_frame.ext.mask))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.mask)));
				return state_recv_mask;
			}
		}
		else {
			if (m_frame.ext.h.payload_len() == 0x7F) {
				if (m_flen < (sizeof(m_frame.ext.nomask_0x7F))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.nomask_0x7F)));
				return state_recv_0x7F;
			}
			else if (m_frame.ext.h.payload_len() == 0x7E) {
				if (m_flen < (sizeof(m_frame.ext.nomask_0x7E))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.nomask_0x7E)));
				return state_recv_0x7E;
			}
			else {
				if (m_flen < (sizeof(m_frame.ext.nomask))) {
					return state_recv_frame;
				}
				over_read_length = (m_flen - (sizeof(m_frame.ext.nomask)));
				return state_recv;
			}
		}
	}
	inline U32 get_mask() {
		switch(get_state()) {
		case state_recv_mask:
			return GET_32(m_frame.ext.mask.masking_key);
		case state_recv_mask_0x7E:
			return GET_32(m_frame.ext.mask_0x7E.masking_key);
		case state_recv_mask_0x7F:
			return GET_32(m_frame.ext.mask_0x7F.masking_key);
		default:
			ASSERT(false);
			return 0;
		}
	}
	inline size_t frame_size() {
		switch(get_state()) {
		case state_recv_mask:
			return m_frame.ext.h.payload_len();
		case state_recv_mask_0x7E:
			return ntohs(m_frame.ext.mask_0x7E.ext_payload_len);
		case state_recv_mask_0x7F:
			return ntohll(GET_64(m_frame.ext.mask_0x7F.ext_payload_len));
		case state_recv:
			return m_frame.ext.h.payload_len();
		case state_recv_0x7E:
			return ntohs(m_frame.ext.nomask_0x7E.ext_payload_len);
		case state_recv_0x7F:
			return ntohll(GET_64(m_frame.ext.nomask_0x7F.ext_payload_len));
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	}
	inline int suck_out_recv_data(DSCRPTR fd, bool &finished) {
		int r; size_t remain = frame_size() - m_read, n_read;
		if (!m_ctrl_frame) {
			if (!(m_ctrl_frame = m_ctrl_frames.alloc())) {
				return NBR_EEXPIRE;
			}
			analyze_frame(n_read);
			if (n_read > 0) {
				util::mem::copy(m_ctrl_frame->m_buff,
					m_frame_buff + (m_flen - n_read), n_read);
				m_ctrl_frame->m_len += n_read;
			}
		}
		while (remain > 0) {
			if ((r = m_ctrl_frame->suck_out(fd, remain)) <= 0) {
				return r;
			}
			m_read += r;
			remain -= r;
		}
		finished = (remain <= 0);
		return NBR_SUCCESS;
	}
	inline int read_frame(DSCRPTR fd, char *p, size_t l) {
		int r; size_t remain, n_read;
		char *orgp = p;
	retry:
	TRACE("length = %u\n", (int)l);
		switch(get_state()) {
		case state_established:
			init_frame(); /* fall through */
		case state_recv_frame: {
			if ((r = sys_read(fd, m_frame_buff + m_flen, sizeof(frame) - m_flen)) < 0) {
				if (r == 0) { return r; }
				if (util::syscall::error_again()) {
					goto again;
				}
				goto error;
			}
			m_flen += r;
			m_state = analyze_frame(n_read);
			if (m_state <= state_recv_frame) {
				goto again;
			}
			if (n_read > 0) {
				if (l < n_read) {
					ASSERT(false);
					goto error;
				}
				util::mem::copy(p, m_frame_buff + (m_flen - n_read), n_read);
				if (m_frame.masked()) {
					mask_payload(p, n_read, get_mask(), m_mask_idx);
				}
				p += n_read;
				l -= n_read;
				m_read += n_read;
				TRACE("read %u byte\n", (int)n_read);
			}
		}  /* fall through */
		case state_recv_mask:
		case state_recv_mask_0x7E:
		case state_recv_mask_0x7F:
		case state_recv:
		case state_recv_0x7E:
		case state_recv_0x7F: {
			TRACE("opcode = %u, flen=%u\n", m_frame.get_opcode(), (int)frame_size());
			switch(m_frame.get_opcode()) {
			case opcode_continuation_frame:
			case opcode_text_frame:
			case opcode_binary_frame: {
				remain = frame_size() - m_read;
				if (remain <= 0) {
					if (m_read <= 0) {
						TRACE("non-control frame has no data\n");
						ASSERT(false);
						goto error;
					}
					m_state = state_established;
					goto retry;
				}
				n_read = l;
				if (n_read > remain) { n_read = remain; }
				if ((r = sys_read(fd, p, n_read)) <= 0) {
					if (r == 0) { return r; }
					if (util::syscall::error_again()) {
						goto again;
					}
					goto error;
				}
				if (m_frame.masked()) {
					mask_payload(p, r, get_mask(), m_mask_idx);
				}
				m_read += r;
				p += r;
				l -= r;
				TRACE("read %u byte\n", r);
			} break;
			case opcode_connection_close: {
				/* body has 2 byte to indicate why connection close */
				bool finished;
				if ((r = suck_out_recv_data(fd, finished)) <= 0) {
					if (r == 0) { return r; }
					if (util::syscall::error_again()) {
						if (m_frame.masked()) {
							mask_payload(m_ctrl_frame->m_buff, m_ctrl_frame->m_len, get_mask(), m_mask_idx);
						}
						goto again;
					}
					goto error;
				}
				if (finished) {
					if (m_frame.masked()) {
						mask_payload(m_ctrl_frame->m_buff, m_ctrl_frame->m_len, get_mask(), m_mask_idx);
					}
					TRACE("close reason : %u\n", GET_16(m_ctrl_frame->m_buff));
					m_ctrl_frames.free(m_ctrl_frame);
					m_ctrl_frame = NULL;
				}
				return 0;/* return 0 byte to indicate connect close to caller */
			}
			case opcode_ping:
			case opcode_pong: {
				bool finished;
				if ((r = suck_out_recv_data(fd, finished)) <= 0) {
					if (r == 0) { return r; }
					if (util::syscall::error_again()) {
						goto again;
					}
					goto error;
				}
				if (finished) {
					if (m_frame.get_opcode() == opcode_ping) {
						if (m_frame.masked()) {
							mask_payload(m_ctrl_frame->m_buff, m_ctrl_frame->m_len,
								get_mask(), m_mask_idx);
						}
						/* return pong */
						write_frame(fd,
							m_ctrl_frame->m_buff,
							m_ctrl_frame->m_len, opcode_pong);
						/* if pong fails, keep on. */
					}
					m_ctrl_frames.free(m_ctrl_frame);
					m_ctrl_frame = NULL;
				}
			} break;
			}
			if (l > 0) {
				TRACE("%u byte remains. retry\n", (int)l);
				goto retry;
			}
			return p - orgp;
		} break;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	again:
		if (orgp < p) {
			return p - orgp;
		}
		return NBR_EAGAIN;
	error:
		return NBR_EINVAL;
	}
	/* no fragmentation support */
	static inline int write_frame(DSCRPTR fd, const char *p, size_t l,
		opcode opc = opcode_binary_frame, bool masked = true, bool fin = true) {
		char buff[sizeof(frame)]; U32 rnd; U8 idx = 0;
		frame *pf = reinterpret_cast<frame *>(buff);
		size_t hl; frame frm;
		pf->ext.h.set_controls(fin, masked, opc);
		ASSERT(fin == pf->ext.h.fin());
		if (l >= 0x7E) {
			if (l <= 0xFFFF) {
				pf->ext.h.set_payload_len(0x7E);
				if (pf->ext.h.mask()) {
					rnd = util::math::rand32();
					pf->ext.mask_0x7E.ext_payload_len = l;
					SET_32(pf->ext.mask_0x7E.masking_key, rnd);
					hl = sizeof(frm.ext.mask_0x7E);
				}
				else {
					pf->ext.nomask_0x7E.ext_payload_len = l;
					hl = sizeof(frm.ext.nomask_0x7E);
				}
			}
			else {
				pf->ext.h.set_payload_len(0x7F);
				if (pf->ext.h.mask()) {
					rnd = util::math::rand32();
					SET_64(pf->ext.mask_0x7F.ext_payload_len, l);
					SET_32(pf->ext.mask_0x7F.masking_key, rnd);
					hl = sizeof(frm.ext.mask_0x7F);
				}
				else {
					SET_64(pf->ext.nomask_0x7F.ext_payload_len, l);
					hl = sizeof(frm.ext.nomask_0x7F);
				}
			}
		}
		else {
			pf->ext.h.set_payload_len(l);
			if (pf->ext.h.mask()) {
				rnd = util::math::rand32();
				SET_32(pf->ext.mask.masking_key, rnd);
				hl = sizeof(frm.ext.mask);
			}
			else {
				hl = sizeof(frm.ext.nomask);
			}
		}
		if (sys_write(fd, buff, hl) < 0) {
			return NBR_ESEND;
		}
		if (masked) {
			return sys_write(fd,
				mask_payload(const_cast<char *>(p), l, rnd, idx), l);
		}
		return sys_write(fd, p, l);
	}
	inline int connect(DSCRPTR fd, void *addr, socklen_t len) {
		m_sm.reset(fd, READSIZE);
		setaddr(static_cast<char *>(addr));
		set_state(state_client_handshake);
		if (tcp_connect(fd, addr, len) < 0) {
			return NBR_ESYSCALL;
		}
		return NBR_OK;
	}
	inline int accept(DSCRPTR fd, const address &a) {
		setaddr(a);
		m_sm.reset(fd, READSIZE);
		m_state = ws_connection::state_server_handshake;
		return fd;
	}
	inline char *init_accept_key_from_header(char *accept_key, size_t accept_key_len) {
		/* get key from websocket header */
		char kbuf[256]; int kblen;
		if (!m_sm.hdrstr("Sec-Websocket-Key", kbuf, sizeof(kbuf), &kblen)) {
			return NULL;
		}
		char vbuf[256];	//it should be 16 byte
		if (sizeof(m_key_ptr) != util::base64::decode(kbuf, kblen, vbuf)) {
			return NULL;
		}
		util::mem::copy(m_key_ptr, vbuf, sizeof(m_key_ptr));
		return generate_accept_key(accept_key, accept_key_len, kbuf);
	}
	inline char *generate_accept_key_from_value(char *accept_key, size_t accept_key_len,
		char *key_buf/* must be 16byte */) {
		/* base64 encode */
		char enc[util::base64::buffsize(16)];
		util::base64::encode(key_buf, 16, enc);
		return generate_accept_key(accept_key, accept_key_len, enc);
	}
	static inline char *generate_accept_key(char *accept_key, size_t accept_key_len, const char *sec_key) {
		if (accept_key_len < util::base64::buffsize(util::SHA1_160BIT_RESULT_SIZE)) {
			ASSERT(false); return NULL;
		}
		/* add salt */
		char work[256];
		char salt[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		size_t l = util::str::printf(work, sizeof(work), "%s%s", sec_key, salt);
		/* encoded by SHA-1(160bit) */
		U8 enc[util::SHA1_160BIT_RESULT_SIZE];
		util::sha1::encode(work, l, enc);
		/* base64 encode */
		util::base64::encode(reinterpret_cast<char *>(enc), sizeof(enc), accept_key);
		return accept_key;
	}
	inline int send_handshake_request() {
		init_key();
		char out[util::base64::buffsize(sizeof(m_key_ptr))], host[256], origin[256];
		util::base64::encode(m_key_ptr, sizeof(m_key_ptr), out);
		m_a.get(host, sizeof(host));
		util::str::printf(origin, sizeof(origin), "http://%s", host);
		return m_sm.send_handshake_request(host, origin, out, NULL);
	}
	inline int send_handshake_response() {
		char buffer[util::base64::buffsize(util::SHA1_160BIT_RESULT_SIZE)], *p;
		if (!(p = init_accept_key_from_header(buffer, sizeof(buffer)))) {
			return NBR_EINVAL;
		}
		return m_sm.send_handshake_response(buffer);
	}
#define HS_CHECK(cond, ...)	if (!(cond)) { TRACE(__VA_ARGS__); return NBR_EINVAL; }
	inline int verify_handshake() {
		char tok[256];
		HS_CHECK(m_sm.hdrstr("Upgrade", tok, sizeof(tok)), "Upgrade header\n");
		HS_CHECK(util::str::cmp_nocase(tok, "websocket", sizeof(tok)) == 0,
			"Upgrade invalid %s\n", tok);
		HS_CHECK(m_sm.hdrstr("Connection", tok, sizeof(tok)), "Connection header\n");
		HS_CHECK(util::str::cmp_nocase(tok, "upgrade", sizeof(tok)) == 0,
			"Connection invalid %s\n", tok);
		switch(get_state()) {
		case state_client_handshake_2: {
			char calculated[util::base64::buffsize(util::SHA1_160BIT_RESULT_SIZE)];
			HS_CHECK(m_sm.rc() == http::HRC_SWITCHING_PROTOCOLS, "invalid response %d\n", m_sm.rc());
			HS_CHECK(m_sm.hdrstr("Sec-WebSocket-Accept", tok, sizeof(tok)) != NULL,
				"Sec-WebSocket-Accept header\n");
			HS_CHECK(NULL != generate_accept_key_from_value(calculated, sizeof(calculated), m_key_ptr),
				"cannot calculate accept key from client data\n");
			HS_CHECK(util::str::cmp(tok, calculated),
				"Sec-WebSocket-Accept Invalid: %s, should be %s\n", tok, calculated);
		} return NBR_OK;
		case state_server_handshake: {
			HS_CHECK(m_sm.hashdr("Host"), "Host header\n");
			HS_CHECK(m_sm.hashdr("Sec-WebSocket-Key"), "Sec-WebSocket-Key header\n");
			/* TODO: optional header check? */
			int v;
			HS_CHECK(m_sm.hdrint("Sec-WebSocket-version", v) >= 0, "Sec-WebSocket-version header");
			HS_CHECK(v == 13, "version invalid %u\n", v);
		} return NBR_OK;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	}
	int handshake(DSCRPTR fd, int r, int w)
	{
		char rbf[ws_connection::READSIZE]; int rsz;
		switch(get_state()) {
		case state_client_handshake: {
			if (!w) { return NBR_OK; }
			if (send_handshake_request() < 0) {
				return NBR_ESEND;
			}
			set_state(state_client_handshake_2);
			return NBR_OK;
		}
		case state_client_handshake_2:
		case state_server_handshake: {
			if (!r) { return NBR_OK; }
			if ((rsz = sys_read(fd, rbf, sizeof(rbf))) < 0) { return rsz; }
			http::fsm::state s = m_sm.append(rbf, rsz);
			if (s == http::fsm::state_recv_header) { return NBR_OK; }
			else if (s == http::fsm::state_websocket_establish) {
				if (verify_handshake() < 0) {
					return NBR_ERIGHT;
				}
				if (get_state() == state_server_handshake) {
					if (send_handshake_response() < 0) {
						return NBR_ESEND;
					}
				}
				set_state(state_established);
				return NBR_SUCCESS;
			}
			ASSERT(false);
			return NBR_EINVAL;
		}
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
	}
	inline void setaddr(const char *addr) { m_a.set(addr); }
	inline void setaddr(const net::address &a) { m_a = a; }
	inline void set_state(state s) { m_state = s; }
	inline state get_state() const { return (state)(m_state); }
public:
	static util::map<ws_connection, DSCRPTR> g_wsock;
	static void fin() { g_wsock.fin(); }
};

util::array<ws_connection::control_frame> ws_connection::m_ctrl_frames;
util::map<ws_connection, DSCRPTR> ws_connection::g_wsock;

int
ws_init(void *ctx) {
	char ak[256];
	ws_connection::generate_accept_key(ak, sizeof(ak), "dGhlIHNhbXBsZSBub25jZQ==");
	ASSERT(util::str::cmp(ak, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);
	util::syscall::rlimit rl;
	if(util::syscall::getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		return NBR_ESYSCALL;
	}
	if (ws_connection::init(rl.rlim_cur) < 0) { return NBR_EMALLOC; }
	return NBR_OK;
}
int
ws_fin(void *ctx) {
	ws_connection::fin();
	return NBR_OK;
}

/* ws related */
DSCRPTR
ws_socket(const char *addr, SKCONF *cfg)
{
	return tcp_socket(addr, cfg);
}

int
ws_connect(DSCRPTR fd, void *addr, socklen_t len)
{
	ws_connection *wsc = ws_connection::alloc(fd);
	if (!wsc) { return NBR_ENOTFOUND; }
	if (wsc->connect(fd, addr, len) < 0) {
		ws_connection::free(fd);
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

int
ws_handshake(DSCRPTR fd, int r, int w)
{
	TRACE("ws_handshake(%d)\n", fd);
	ws_connection *wsc = ws_connection::from(fd);
	if (!wsc) { return NBR_ENOTFOUND; }
	return wsc->handshake(fd, r, w);
}

DSCRPTR
ws_accept(DSCRPTR fd, void *addr, socklen_t *len, SKCONF *cfg)
{
	address a;
	DSCRPTR cfd = tcp_accept(fd, a.addr_p(), a.len_p(), cfg);
	if (cfd < 0) { return cfd; }
	ws_connection *wsc = ws_connection::alloc(cfd);
	if (!wsc) { return INVALID_FD; }
	return wsc->accept(cfd, a);
}

int
ws_close(DSCRPTR fd)
{
	ws_connection::write_frame(fd, "", 0,
		ws_connection::opcode_connection_close);
	ws_connection::free(fd);
	tcp_close(fd);
	return NBR_OK;
}

int
ws_recv(DSCRPTR fd, void *data, size_t len)
{
	int r; ws_connection *wsc = ws_connection::from(fd);
	if (!wsc) { return -1; }
	r = wsc->read_frame(fd, reinterpret_cast<char *>(data), len);
	return r >= 0 ? r : -1;
}

int
ws_send(DSCRPTR fd, const void *data, size_t len)
{
	return ws_connection::write_frame(fd, 
		reinterpret_cast<char *>(const_cast<void *>(data)), 
		len);
}
}
}

extern "C" {

static	transport
g_ws = {
	"ws",
	NULL,
	false,
	yue::net::ws_init,
	yue::net::ws_fin,
	NULL,
	yue::net::tcp_str2addr,
	yue::net::tcp_addr2str,
	(DSCRPTR (*)(const char *,void*))yue::net::ws_socket,
	yue::net::ws_connect,
	yue::net::ws_handshake,
	(DSCRPTR (*)(DSCRPTR, void *, socklen_t*, void*))yue::net::ws_accept,
	yue::net::ws_close,
	(RECVFUNC)yue::net::ws_recv,
	(SENDFUNC)yue::net::ws_send,
	NULL,
	NULL,
};

transport *ws_transport() { return &g_ws; }

}

