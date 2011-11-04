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
#include "hresult.h"

namespace yue {
namespace module {
namespace net {
namespace eio {

#define WBUF_TRACE(...)

class writer;
class wbuf {
	FORBID_COPY(wbuf);
friend class writer;
friend class loop;
public:	/* writer */
	enum {
		WBUF_CMD_RAW,		/* ::write(2) */
		WBUF_CMD_IOVEC,		/* ::writev(2) */
		WBUF_CMD_FILE,		/* ::sendfile(2) */

		WBUF_CMD_NUM,
	};
	struct raw {
		size_t sz, pos;
		U8 p[0];
	public:
		struct arg {
			U8 *p; U32 sz;
			inline arg(U8 *b, U32 s) : p(b), sz(s) {ASSERT(sz > 0);}
			inline void set_pbuf(pbuf *) {}
		};
		static inline U8 cmd() { return WBUF_CMD_RAW; }
		static inline size_t required_size(arg &a, bool append) {
			return a.sz + (append ? 0 : sizeof(U32));
		}
		inline size_t chunk_size() const { return sizeof(raw) + sz; }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0; sz = a.sz;
				util::mem::copy(p, a.p, a.sz);
				return chunk_size();
			}
			util::mem::copy(p + sz, a.p, a.sz);
			sz += a.sz;
			return a.sz;
		}
		inline int write(DSCRPTR fd, transport *t = NULL) { return syscall::write(fd, p + pos, sz - pos, t); }
		void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			pos += wb;
		}
		bool finish() { ASSERT(pos <= sz); return sz <= pos; }
	};
	struct iov {
		U32 sz, pos;
		struct iovec iov[0];
	public:
		struct arg {
			struct iovec *iov; U32 sz;
			inline arg(struct iovec *v, U32 s) : iov(v), sz(s) {ASSERT(sz > 0);}
			inline void set_pbuf(pbuf *) {}
		};
		static inline U8 cmd() { return WBUF_CMD_IOVEC; }
		static inline size_t required_size(arg &a, bool append) {
			return (a.sz * sizeof(struct iovec)) + (append ? 0 : sizeof(U32));
		}
		inline size_t chunk_size() const { return sizeof(iov) + (sz * sizeof(struct iovec)); }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0; sz = a.sz;
				util::mem::copy(iov, a.iov, (a.sz * sizeof(iov[0])));
				return chunk_size();
			}
			util::mem::copy(iov + sz, a.iov, (a.sz * sizeof(iov[0])));
			sz += a.sz;
			return a.sz * sizeof(iov[0]);
		}
		int write(DSCRPTR fd, transport *t = NULL) { return syscall::writev(fd, iov + pos, sz - pos, t); }
		void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			size_t i;
			for(i = pos; i < sz; i++) {
				if(wb >= iov[i].iov_len) { wb -= iov[i].iov_len; }
				else {
					iov[i].iov_base = (void*)(((U8*)iov[i].iov_base) + wb);
					iov[i].iov_len -= wb;
					pos = i;
					break;
				}
			}
		}
		bool finish() { ASSERT(pos <= sz); return sz <= pos; }
	};
	struct file {
		U32 sz, pos;
		struct {
			DSCRPTR in_fd;
			off_t ofs; size_t cnt;
		} file[0];
	public:
		struct arg {
			DSCRPTR in_fd; U32 ofs, sz;
			inline arg(DSCRPTR a1, U32 a2, U32 a3) : in_fd(a1), ofs(a2), sz(a3) {ASSERT(sz > 0);}
			inline void set_pbuf(pbuf *) {}
		};
		static inline U8 cmd() { return WBUF_CMD_FILE; }
		static inline size_t required_size(arg &a, bool) { return sizeof(arg); }
		inline size_t chunk_size() const { return sizeof(file) + (sz * sizeof(file[0])); }
		inline int operator () (arg &a, bool append) {
			if (!append) {
				pos = 0;
				file[0].in_fd = a.in_fd;
				file[0].ofs = a.ofs;
				file[0].cnt = a.sz;
				sz = 1;
				return chunk_size();
			}
			file[sz].in_fd = a.in_fd;
			file[sz].ofs = a.ofs;
			file[sz].cnt = a.sz;
			sz++;
			return sizeof(file[0]);
		}
		int write(DSCRPTR out_fd, transport *t = NULL) {
			return syscall::sendfile(out_fd, file[pos].in_fd, &(file[pos].ofs), file[pos].cnt, t);
		}
		void sent(size_t wb) {
			ASSERT(wb <= (sz - pos) && sz >= pos);
			if (file[pos].cnt > wb) { file[pos].cnt -= wb; }
			else { pos++; }
		}
		bool finish() { ASSERT(pos <= sz); return sz <= pos; }
	};
	template <class O, class SR>
	struct obj : public raw {
		struct arg {
			O &object;
			SR &packer;
			U32 size;
			pbuf *m_pbuf;
			inline arg(O &obj, SR &sr, U32 sz) : object(obj), packer(sr), size(sz) {}
			inline void set_pbuf(pbuf *p) { m_pbuf = p; }
		};
		static inline size_t required_size(obj::arg &a, bool) { return sizeof(raw) + a.size; }
		char *buff() { return reinterpret_cast<char *>(&(raw::p[0])); }
		inline int operator () (obj::arg &a, bool append) {
			int r;
			if (!append) {
#if defined(__USE_OLD_BUFFER)
				raw::pos = 0;
				if ((r = a.packer.pack(a.object, buff(), a.size, a.m_pbuf)) < 0) { return r; }
				raw::sz = r;
				return raw::chunk_size();
#else
				a.m_pbuf->commit(sizeof(raw));
				raw::pos = 0;
				if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
				return (raw::sz = r);
#endif
			}
#if defined(__USE_OLD_BUFFER)
			if ((r = a.packer.pack(a.object, buff() + raw::sz, a.size, a.m_pbuf)) < 0) { return r; }
#else
			if ((r = a.packer.pack(a.object, a.m_pbuf)) < 0) { return r; }
#endif
			raw::sz += r;
			return r;
		}
	};
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
	typedef msgid_generator<U32>::MSGID serial;
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
	wbuf() : m_widx(0), m_flag(0), m_last_wcmd(WBUF_CMD_NUM), m_serial(INVALID_SERIAL_ID) {}
	serial serial_id() const { return m_serial; }
	thread::mutex &mtx() { return m_wpbf.mtx; }
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
	template <class WRITER, class SENDER>
	int send(typename WRITER::arg a, SENDER &sender) {
		/* if kind of write command is same as previous one,
		 * just append it after previous chunk, and send it together at
		 * 1 system call. */
		bool append = (m_last_wcmd == WRITER::cmd());
		pbuf *pbf = m_wpbf.next;
		thread::scoped<thread::mutex> lk(m_wpbf.mtx);
		if (lk.lock() < 0) { return NBR_EPTHREAD; }
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
		if ((s = (*w)(a, append)) < 0) { return NBR_ESHORT; }
		pbf->commit(s);/* commit written byte */
		TRACE("wbuf send: commit %d byte\n", (int)s);
		m_last_wcmd = WRITER::cmd();
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
	inline int write_command(DSCRPTR fd, context &ctx, transport *t) {
		ASSERT(ctx.next && ctx.curr);
		if (ctx.curr->last() == 0) {
			thread::scoped<thread::mutex> lk(ctx.mtx);
			if (lk.lock() < 0) {
				ASSERT(false);
				return handler_result::destroy;
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
				return handler_result::nop;
			}
			/* TODO: load balance: if too much wbuf handled by 1 thread,
			 * may need to pass this wbuf to another thread
			 * ex) if (this_thread.number_of_wbuf_processed >= some_threshold) {
			 * 		pass(this, least_load_thread);
			 * 		return handler_result::nop;
			 * } or, just check number of processed
			 * if (this->number_of_continuous_processed_count_by_1thread >= some_threshold) {
			 * 		pass(this, least_load_thread);
			 * 		return handler_result::nop;
			 * }
			 * */
			return handler_result::keep;
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
						TRACE("errno = %d\n", error_no());		\
						ASSERT(error_again() || error_conn_reset() || error_pipe());\
						return error_again() ? handler_result::again :	\
								handler_result::destroy;				\
					}													\
					ASSERT(ctx.next && ctx.curr);	\
					TRACE("write(%d,%d): send %d byte\n",fd,CMD,r);		\
					w->sent(r);											\
					if (w->finish()) { 									\
						TRACE("write finish: proceed %lu\n", (long signed int)w->chunk_size()); 	\
						now += (1 + w->chunk_size()); 						\
					}													\
					else { goto shrink_buffer; }						\
				} break;

				WCMD(WBUF_CMD_RAW, raw);
				WCMD(WBUF_CMD_IOVEC, iov);
				WCMD(WBUF_CMD_FILE, file);
				default: ASSERT(false); return handler_result::destroy;
				}
			}
		shrink_buffer:
			ASSERT(ctx.next && ctx.curr);
			ASSERT(now >= ctx.curr->p());	/* if no w->finish(), == is possible */
			ctx.curr->shrink_used_buffer_size(now - ctx.curr->p());
			return handler_result::keep;
		}
	}
	inline int write(DSCRPTR fd, transport *t = NULL) {
		return write_command(fd, m_wpbf, t);
	}
};

class writer {
protected:
	typedef wbuf::serial serial;
	static selector::method *g_write_poller;
	wbuf *wb;
	DSCRPTR fd;
	serial sn;
private:
	inline writer(wbuf *ptr, DSCRPTR d, serial s) : wb(ptr), fd(d), sn(s)
		{ASSERT(s != msgid_generator<U32>::INVALID_MSGID);}
public:
	writer() : wb(NULL) {}
	static void init(selector::method *m) { g_write_poller = m; }
	inline void dump() { TRACE("writer: %p %p %u %u\n", this, wb, fd, sn); }
	inline bool valid() const { return wb && sn == wb->serial_id(); }
	inline int attach() {
		TRACE("write attach %d\n", fd);
		return g_write_poller->retach(fd, selector::method::EV_WRITE);
	}
	inline int write(char *p, size_t l) {
		return valid() ? wb->send<wbuf::raw>(
			wbuf::raw::arg(reinterpret_cast<U8 *>(p), l), *this) : NBR_EINVAL;
	}
	inline int writev(struct iovec *v, U32 s) {
		return valid() ? wb->send<wbuf::iov>(
			wbuf::iov::arg(v, s), *this) : NBR_EINVAL;
	}
	inline int writef(DSCRPTR s, size_t ofs, size_t sz) {
		return valid() ? wb->send<wbuf::file>(
			wbuf::file::arg(s, ofs, sz), *this) : NBR_EINVAL;
	}
	template <class SR, class OBJ>
	inline int writeo(SR &sr, OBJ &o,
		U32 estimate = ((1024) > (sizeof(OBJ) * 2) ? 1024 : (sizeof(OBJ) * 2))) {
		return valid() ? wb->send<wbuf::template obj<OBJ, SR> >(
			typename wbuf::template obj<OBJ, SR>::arg(o, sr, estimate),
			*this) : NBR_EINVAL;
	}
	static inline writer create(wbuf *wb, DSCRPTR fd) {
		ASSERT(wb);
		return writer(wb, fd, wb->serial_id());
	}
public:/* internal use only */
	inline int flush_to(DSCRPTR fd, transport *t = NULL) {
		return wb->write(fd, t);
	}
};

}
}
}
}

#endif
