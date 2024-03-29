/***************************************************************
 * wbuf.h : socket writer
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
#if !defined(__WBUF_H__)
#define __WBUF_H__

#include "selector.h"
#include "parking.h"
#include "handler.h"
#include "address.h"
#include "sbuf.h"

namespace yue {
class loop;
namespace handler {
class socket;
class write_poller;
}
namespace net {

#define WBUF_TRACE(...)

class wbuf {
	FORBID_COPY(wbuf);
friend class handler::socket;
friend class handler::write_poller;
friend class loop;
public:	/* writer */
	enum {
		WBUF_CMD_RAW,		/* ::write(2) */
		WBUF_CMD_IOVEC,		/* ::writev(2) */
		WBUF_CMD_FILE,		/* ::sendfile(2) */
		WBUF_CMD_DGRAM,		/* ::sendto(2) */

		WBUF_CMD_NUM,
	};
	template <class CMD>
	struct stream_command {
		static inline bool append(wbuf &wbf) {return CMD::cmd() == wbf.m_last_wcmd;}
		static inline void update_wbuf_send_info(wbuf &wbf) { 
			wbf.m_last_wcmd = CMD::cmd(); }
	};
	template <class CMD>
	struct dgram_command {
		address addr;
		/* TODO: compare address? */
		static inline bool append(wbuf &wbf) { return false; }
		static inline void update_wbuf_send_info(wbuf &wbf) {}
	};
	struct base_arg {
		pbuf *m_pbuf;
		inline void set_pbuf(pbuf *pbf) { m_pbuf = pbf; }
	};
	struct raw : public stream_command<raw> {
		size_t sz, pos;
		U8 p[0];
	public:
		struct arg : public base_arg {
			U8 *p; U32 sz;
			inline arg(U8 *b, U32 s) : p(b), sz(s) {ASSERT(sz > 0);}
		};
		static inline U8 cmd() { return WBUF_CMD_RAW; }
		static inline size_t required_size(arg &a, bool append) {
			return a.sz + (append ? 0 : sizeof(raw));
		}
		inline size_t chunk_size() const { return sizeof(raw) + sz; }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0; sz = a.sz;
				util::mem::copy(p, a.p, a.sz);
				a.m_pbuf->commit(chunk_size());
				return chunk_size();
			}
			util::mem::copy(p + sz, a.p, a.sz);
			sz += a.sz;
			a.m_pbuf->commit(a.sz);
			return a.sz;
		}
		inline int write(DSCRPTR fd, transport *t = NULL) {
			return syscall::write(fd, p + pos, sz - pos, t); }
		inline void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			pos += wb;
		}
		inline bool finish() const { ASSERT(pos <= sz); return sz <= pos; }
		inline void print() { printf("%p: raw: %u, %u\n", thread::current(), (U32)sz, (U32)pos); }
	};
	struct dgram : public dgram_command<dgram> {
		size_t sz, pos;
		U8 p[0];
	public:
		struct arg : public base_arg {
			U8 *p; U32 sz; address &addr;
			inline arg(U8 *b, U32 s, address &a) : p(b), sz(s), addr(a) {ASSERT(sz > 0);}
		};
		static inline U8 cmd() { return WBUF_CMD_DGRAM; }
		static inline size_t required_size(arg &a, bool append) {
			return a.sz + (append ? 0 : sizeof(dgram));
		}
		inline size_t chunk_size() const { return sizeof(dgram) + sz; }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0; sz = a.sz; addr = a.addr;
				util::mem::copy(p, a.p, a.sz);
				a.m_pbuf->commit(chunk_size());
				return chunk_size();
			}
			util::mem::copy(p + sz, a.p, a.sz);
			sz += a.sz;
			a.m_pbuf->commit(a.sz);
			return a.sz;
		}
		inline int write(DSCRPTR fd, transport *t = NULL) {
			return syscall::sendto(fd, p + pos, sz - pos, 
				addr.addr_p(), addr.len(),t);
		}
		inline void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			pos += wb;
		}
		inline bool finish() const { ASSERT(pos <= sz); return sz <= pos; }
		inline void print() { printf("dgram: %u, %u\n", (U32)sz, (U32)pos); }
	};
	struct iov : public stream_command<iov> {
		size_t sz, pos;
		struct iovec vec[0];
	public:
		struct arg : public base_arg {
			struct iovec *vec; U32 sz;
			inline arg(struct iovec *v, U32 s) : vec(v), sz(s) {ASSERT(sz > 0);}
		};
		static inline U8 cmd() { return WBUF_CMD_IOVEC; }
		static inline size_t required_size(arg &a, bool append) {
			return (a.sz * sizeof(struct iovec)) + (append ? 0 : sizeof(iov));
		}
		inline size_t chunk_size() const { return sizeof(iov) + (sz * sizeof(struct iovec)); }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0; sz = a.sz;
				util::mem::copy(vec, a.vec, (a.sz * sizeof(vec[0])));
				a.m_pbuf->commit(chunk_size());
				return chunk_size();
			}
			util::mem::copy(vec + sz, a.vec, (a.sz * sizeof(vec[0])));
			sz += a.sz;
			a.m_pbuf->commit(a.sz * sizeof(vec[0]));
			return a.sz * sizeof(vec[0]);
		}
		inline int write(DSCRPTR fd, transport *t = NULL) {
			return syscall::writev(fd, vec + pos, sz - pos, t); }
		void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			size_t i;
			for(i = pos; i < sz; i++) {
				if(wb >= vec[i].iov_len) { wb -= vec[i].iov_len; }
				else {
					vec[i].iov_base = (void*)(((U8*)vec[i].iov_base) + wb);
					vec[i].iov_len -= wb;
					pos = i;
					break;
				}
			}
		}
		inline bool finish() const { ASSERT(pos <= sz); return sz <= pos; }
		inline void print() { printf("iov: %u, %u\n", (U32)sz, (U32)pos); }
	};
	struct file : public stream_command<file> {
		U32 sz, pos;
		struct {
			DSCRPTR in_fd;
			off_t ofs; size_t cnt;
		} fds[0];
	public:
		struct arg : public base_arg {
			DSCRPTR in_fd; U32 ofs, sz;
			inline arg(DSCRPTR a1, U32 a2, U32 a3) : in_fd(a1), ofs(a2), sz(a3) {ASSERT(sz > 0);}
		};
		static inline U8 cmd() { return WBUF_CMD_FILE; }
		static inline size_t required_size(arg &a, bool append) {
			return sizeof(arg) + (append ? 0 : sizeof(file)); }
		inline size_t chunk_size() const { return sizeof(file) + (sz * sizeof(file::fds[0])); }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0;
				fds[0].in_fd = a.in_fd;
				fds[0].ofs = a.ofs;
				fds[0].cnt = a.sz;
				sz = 1;
				a.m_pbuf->commit(chunk_size());
				return chunk_size();
			}
			fds[sz].in_fd = a.in_fd;
			fds[sz].ofs = a.ofs;
			fds[sz].cnt = a.sz;
			sz++;
			a.m_pbuf->commit(sizeof(fds[0]));
			return sizeof(fds[0]);
		}
		inline int write(DSCRPTR out_fd, transport *t = NULL) {
			return syscall::sendfile(
				out_fd, fds[pos].in_fd, &(fds[pos].ofs), fds[pos].cnt, t);
		}
		void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			if (fds[pos].cnt > wb) { fds[pos].cnt -= wb; }
			else { pos++; }
		}
		inline bool finish() { ASSERT(pos <= sz); return sz <= pos; }
		inline void print() { printf("file: %u, %u\n", sz, pos); }
	};
	template <class O, class SR>
	struct obj : public raw {
		struct arg : public base_arg {
			static const U32 INITIAL_BUFFSIZE = 1024;
			O &object;
			SR &packer;
			inline arg(O &obj, SR &sr) : object(obj), packer(sr) {}
		};
		static inline size_t required_size(obj::arg &, bool) { return obj::arg::INITIAL_BUFFSIZE; }
		char *buff() { return reinterpret_cast<char *>(&(raw::p[0])); }
		inline int operator () (obj::arg &a, bool append) {
			int r;
			if (!append) {
				a.m_pbuf->commit(sizeof(raw));
				raw::pos = 0;
				if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
				return (raw::sz = r);
			}
			if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
			raw::sz += r;
			return r;
		}
	};
	template <class O, class SR, class CMD>
	struct obj2 : public CMD {
		struct arg {
			static const U32 INITIAL_BUFFSIZE = 1024;
			O &object;
			SR &packer;
			pbuf *m_pbuf;
			inline arg(O &obj, SR &sr) : object(obj), packer(sr) {}
			inline void set_pbuf(pbuf *p) { m_pbuf = p; }
		};
		struct arg_dgram : public arg {
			const address &addr;
			inline arg_dgram(O &obj, SR &sr, const address &a) : arg(obj, sr), addr(a) {}
		};
		static inline size_t required_size(arg &, bool) { return obj2::arg::INITIAL_BUFFSIZE; }
		static inline size_t required_size(arg_dgram &, bool) { return obj2::arg::INITIAL_BUFFSIZE; }
		inline int operator () (arg &a, bool append) {
			int r;
			if (!append) {
				a.m_pbuf->commit(sizeof(CMD));
				CMD::pos = 0;
				if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
				return (CMD::sz = r);
			}
			if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
			CMD::sz += r;
			return r;
		}
		inline int operator () (arg_dgram &a, bool append) {
			if (!append) { CMD::addr = a.addr; }
			return this->operator () (reinterpret_cast<arg &>(a), append);
		}
	};
	typedef msgid_generator<U32>::MSGID serial;
protected:
	struct context {
		pbuf dbf[2];
		pbuf *curr, *next;
		size_t cs;
		thread::mutex mtx;
		char *p_last_writer;
	} m_wpbf;
	U8 m_widx, m_flag, m_last_wcmd, padd;
	static msgid_generator<U32> m_gen;
	static const size_t INITIAL_WBUF_SIZE = 16 * 1024; /* 16KB */
	static const serial INVALID_SERIAL_ID = msgid_generator<U32>::INVALID_MSGID;
	serial m_serial;
	int init() {
		int r;
		if (false == initialized()) {
			if ((r = m_wpbf.dbf[0].reserve(INITIAL_WBUF_SIZE)) < 0) { return r; }
			if ((r = m_wpbf.dbf[1].reserve(INITIAL_WBUF_SIZE)) < 0) { return r; }
			m_wpbf.curr = &(m_wpbf.dbf[0]);
			m_wpbf.next = &(m_wpbf.dbf[1]);
			if ((r = m_wpbf.mtx.init()) < 0) { return r; }
		}
		else {
			m_wpbf.dbf[0].reset();
			m_wpbf.dbf[1].reset();
		}
		update_serial();
		m_last_wcmd = WBUF_CMD_NUM;
		return NBR_OK;
	}
	inline void update_serial() { m_serial = m_gen.new_id(); }
	inline int wb_reserve(int size, U8 kind) { return m_wpbf.curr->reserve(size); }
	inline void invalidate() { m_serial = INVALID_SERIAL_ID; }
	inline bool initialized() const { return m_serial != INVALID_SERIAL_ID; }
public:
	wbuf() : m_widx(0), m_flag(0), m_last_wcmd(WBUF_CMD_NUM),
		m_serial(INVALID_SERIAL_ID) {}
	~wbuf() {}
	serial serial_id() const { return m_serial; }
	thread::mutex &mtx() { return m_wpbf.mtx; }
	template <class WRITER, class SENDER>
	int send(typename WRITER::arg a, SENDER &sender) {
		return sendraw<WRITER, typename WRITER::arg, SENDER>(a, sender);
	}
	template <class WRITER, class SENDER>
	int send(typename WRITER::arg_dgram a, SENDER &sender) {
		return sendraw<WRITER, typename WRITER::arg_dgram, SENDER>(a, sender);
	}
protected:
	enum {
		FLAG_WRITE_ATTACH,
		FLAG_CLOSED,
	};
	bool write_attach() { return __sync_bool_compare_and_swap(&m_flag, 0, 1); }
	bool write_detach() { return __sync_bool_compare_and_swap(&m_flag, 1, 0); }
	static inline int error_no() { return util::syscall::error_no(); }
	static inline bool error_pipe() { return util::syscall::error_pipe(); }
	static inline bool error_again() { return util::syscall::error_again(); }
	static inline bool error_conn_reset() { return util::syscall::error_conn_reset(); }
	template <class WRITER, class ARG, class SENDER>
	int sendraw(ARG a, SENDER &sender) {
		/* if kind of write command is same as previous one,
		 * just append it after previous chunk, and send it together at
		 * 1 system call. */
		thread::scoped<thread::mutex> lk(m_wpbf.mtx);
		if (lk.lock() < 0) { return NBR_EPTHREAD; }
		bool append = WRITER::append(*this);
		pbuf *pbf = m_wpbf.next;	//next may change if it retrieved before m_wpbf locked.
		size_t s = WRITER::required_size(a, append);
		char *org_p = pbf->p();
		if (pbf->reserve(s) < 0) { return NBR_EMALLOC; }
		ASSERT(pbf->available() >= s);
		a.set_pbuf(pbf);
		WRITER *w = append ?
						reinterpret_cast<WRITER *>(
							org_p == pbf->p() ?
								m_wpbf.p_last_writer :
								(m_wpbf.p_last_writer = (pbf->p() + (m_wpbf.p_last_writer - org_p)))
						)
						:
						reinterpret_cast<WRITER *>(
							m_wpbf.p_last_writer = pbf->push(WRITER::cmd())
						);
		if ((s = (*w)(a, append)) < 0) { ASSERT(false); return NBR_ESHORT; }
		//pbf->commit(s);/* internal pbf written size is autometically proceed. (for adhoc expanding buffer) */
		TRACE("wbuf send: commit %d byte %u\n", (int)s, (U32)m_wpbf.next->last());
		WRITER::update_wbuf_send_info(*this);
		if (write_attach()) {
			TRACE("wbuf send: write attached now\n");
			if (sender.attach() < 0) {
				TRACE("wbuf send: sender attach fails\n");
				write_detach();
				pbf->rollback(s);
				return NBR_ESYSCALL;
			}
			/* NOTE: should not touch this wbuf after attach to poller.
			 * because another thread may immediately touch it. */
		}
		ASSERT(m_wpbf.next && m_wpbf.curr);
		return s;
	}
	inline handler::base::result write_command(DSCRPTR fd, context &ctx, transport *t) {
		ASSERT(ctx.next && ctx.curr);
		if (ctx.curr->last() == 0) {
			thread::scoped<thread::mutex> lk(ctx.mtx);
			if (lk.lock() < 0) {
				ASSERT(false);
				return handler::base::destroy;
			}
	WBUF_TRACE("wbuf swap %p %p\n", ctx.curr, ctx.next);
			pbuf *pbf = ctx.next;
			ctx.next = ctx.curr;
			ctx.curr = pbf;
			m_last_wcmd = WBUF_CMD_NUM;
			/* NOTE: only here, we can check all packet sending 'finished'.
			 * because curr is only touched by this thread and
			 * next cannot touch by another thread
			 * at this timing (because mutex locked)
			 * now ctx.curr is finished(w->finish() == true) already,
			 * so if ctx.next(=pbf) is finished (below check),
			 * both buffer is empty. so we can 'release' this wbuf
			 * until next send() call calls retach(). */
			if (pbf->last() == 0) {
				write_detach();
				WBUF_TRACE("write: detached %d\n", m_flag);
				return handler::base::nop;
			}
			/* TODO: load balance: if too much wbuf handled by 1 thread,
			 * may need to pass this wbuf to another thread
			 * ex) if (this_thread.number_of_wbuf_processed >= some_threshold) {
			 * 		pass(this, least_load_thread);
			 * 		return handler::base::nop;
			 * } or, just check number of processed
			 * if (this->number_of_continuous_processed_count_by_1thread >= some_threshold) {
			 * 		pass(this, least_load_thread);
			 * 		return handler::base::nop;
			 * }
			 * */
			return handler::base::keep;
		}
		else {
			int r;
			char *now = ctx.curr->p(), *last = ctx.curr->last_p();
			while (now < last) {
				ASSERT(ctx.curr->last() > 1);
				switch(*now) {
#define WCMD(CMD,WRITER)	case CMD: { 								\
					WRITER *w = reinterpret_cast<WRITER *>(now + 1);	\
					ASSERT(!w->finish());								\
					if ((r = w->write(fd, t)) < 0) {					\
						TRACE("%s(%u) errno = %d\n", __FILE__, __LINE__, error_no());		\
						ASSERT(error_again() || error_conn_reset() || error_pipe());\
						if (error_pipe()) { return handler::base::nop; }	\
						return error_again() ? handler::base::write_again :	\
								handler::base::destroy;				\
					}													\
					ASSERT(ctx.next && ctx.curr);	\
					TRACE("%p: write(%d,%d): send %d byte\n",thread::current(), fd,CMD,r);		\
					w->sent(r);											\
					if (w->finish()) { 									\
						TRACE("%p: write finish: proceed %lu\n", thread::current(), (long signed int)w->chunk_size()); 	\
						now += (1 + w->chunk_size()); 						\
					}													\
					else { goto shrink_buffer; }						\
				} break;

				WCMD(WBUF_CMD_RAW, raw);
				WCMD(WBUF_CMD_IOVEC, iov);
				WCMD(WBUF_CMD_FILE, file);
				WCMD(WBUF_CMD_DGRAM, dgram);
				default: ASSERT(false); return handler::base::destroy;
				}
			}
		shrink_buffer:
			ASSERT(ctx.next && ctx.curr);
			ASSERT(now >= ctx.curr->p());	/* if no w->finish(), == is possible */
			ctx.curr->shrink_used_buffer_size(now - ctx.curr->p());
			ASSERT(ctx.curr->last() != 18);
			return handler::base::keep;
		}
	}
	inline handler::base::result write(DSCRPTR fd, transport *t = NULL) {
		return write_command(fd, m_wpbf, t);
	}
};
}
}

#endif
