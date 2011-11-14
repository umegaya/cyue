/***************************************************************
 * strh.h : streaming handler
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
#if !defined(__HANDLER_H__)
#define __HANDLER_H__

#include "hresult.h"
#include "actor.h"
#include "map.h"
#include "dgsock.h"

namespace yue {
namespace module {
namespace net {
namespace eio {
#define HANDLER_TRACE(...)
template <class SR, class RECIPIENT>
class stream_dispatcher {
public:
	typedef struct {
		accept_handler ah;
		connect_handler ch;
	} handler;
	typedef loop::poller poller;
	typedef loop::template processor<stream_dispatcher<SR, RECIPIENT> > proc;
	typedef loop::basic_processor base;
	typedef loop::template em<proc> event_machine;
	typedef poller::event event;
	typedef loop::basic_processor::fd_type fd_type;
	typedef typename SR::object object;
	typedef typename SR::lobject lobject;
	typedef util::template functional<int (RECIPIENT &, object &)> delegatable;
	struct task {
		unsigned char type, padd[3];
		struct thread_msg {
			event_machine *m_em;
			object m_o;
		};
		struct delegated_handler {
			delegatable m_h;
			object m_o;
		};
		struct delegated_fiber {
			class fiber *m_f;
			object m_o;
		};
		union {
			poller::event m_ev;
			DSCRPTR m_fd;
			/* HACK: to declare struct msg in union.
			 * (structure which has assignment operator
			 * does not allow to declare in union.) */
			U8 m_tmsg[sizeof(thread_msg)];
			U8 m_dh[sizeof(delegated_handler)];
			U8 m_df[sizeof(delegated_fiber)];
		};
		enum {
			WRITE_AGAIN,
			READ_AGAIN,
			CLOSE,
			THREAD_MSG,
			DELEGATE_FIBER,
			DELEGATE_HANDLER,
			TYPE_MAX,
		};
		inline task() {}
		inline task(DSCRPTR fd) : type(CLOSE), m_fd(fd) {}
		inline task(poller::event &ev, U8 t) : type(t), m_ev(ev) {}
		inline task(class fiber *f, object &o) : type(DELEGATE_FIBER) {
			delegated_fb().m_f = f;
			delegated_fb().m_o = o;
		}
		inline task(delegatable &fh, object &o) : type(DELEGATE_HANDLER) {
			delegated().m_h = fh;
			delegated().m_o = o;
		}
		inline task(event_machine &em, object &o) : type(THREAD_MSG) {
			tmsg().m_em = &em;
			tmsg().m_o = o;
		}
		inline void operator () (event_machine &em, poller &p) {
			switch(type) {
			case WRITE_AGAIN: {
				em.proc().wp().write(em, m_ev);
			} break;
			case READ_AGAIN: {
				em.proc().process(em, m_ev);
			} break;
			case CLOSE: {
				TRACE("fd=%d closed\n", m_fd);
				em.proc().close(m_fd);
			} break;
			case THREAD_MSG: {
				local_actor la(tmsg().m_em);
				em.proc().dispatcher().recipient().recv(la, tmsg().m_o);
			} break;
			case DELEGATE_FIBER: {
				delegated_fb().m_f->resume(
					em.proc().dispatcher().recipient(), 
					delegated_fb().m_o);
			} break;
			case DELEGATE_HANDLER: {
				delegated().m_h(em.proc().dispatcher().recipient(), 
					delegated().m_o);
			} break;
			default: ASSERT(false); break;
			}
		}
	private:
		inline thread_msg &tmsg() {
			return *reinterpret_cast<thread_msg *>(m_tmsg);
		}
		inline delegated_handler &delegated() {
			return *reinterpret_cast<delegated_handler *>(m_dh);
		}
		inline delegated_fiber &delegated_fb() {
			return *reinterpret_cast<delegated_fiber *>(m_df);
		}
	};
protected:
	/* global */
	static class stream_handler {
		static const size_t MINIMUM_FREE_PBUF = 1024;
		pbuf m_pbf;
		writer m_wr;
		SR m_sr;
		union {
			U8 m_ch[sizeof(connect_handler)];
			U8 m_ah[sizeof(accept_handler)];
		};
		union {
			dgsock *m_dgram;
		};
		U8 m_state, padd[3];
	public:
		enum {
			INVALID,
			HANDSHAKE,
			SVHANDSHAKE,
			DGHANDSHAKE,
			ESTABLISH,
			SVESTABLISH,
			DGESTABLISH,
			DGLISTEN,
			LISTEN,
			SIGNAL,
			POLLER,
			TIMER,
		};
		stream_handler() : m_pbf(), m_wr(), m_sr(), m_state(INVALID) {}
		inline SR &sr() { return m_sr; }
		inline remote_actor &get_remote_actor() {
			return *(reinterpret_cast<remote_actor *>(&m_wr));
		}
		inline connect_handler &ch() {
			ASSERT(m_state == HANDSHAKE || m_state == SVHANDSHAKE ||
				m_state == ESTABLISH || m_state == SVESTABLISH ||
				m_state == DGHANDSHAKE || m_state == DGESTABLISH);
			return *reinterpret_cast<connect_handler*>(m_ch);
		}
		inline accept_handler &ah() {
			ASSERT(m_state == LISTEN || m_state == DGLISTEN);
			return *reinterpret_cast<accept_handler*>(m_ah);
		}
		inline bool reset(DSCRPTR fd, int state, const handler &h) {
			ASSERT(m_state == INVALID);
			m_state = state;
			switch(m_state) {
			case HANDSHAKE:  break;	/* after HANDSHAKE success */
			case SVHANDSHAKE:
			case DGHANDSHAKE:
				ch() = h.ch; break;
			case LISTEN:
				ah() = h.ah; break;
			case DGLISTEN:
				ah() = h.ah;
				if ((m_dgram = new dgsock()) && m_dgram->init() >= 0) {
					loop::basic_processor::wp().set_wbuf(fd, m_dgram);
					return true;
				}
				return false;
			case SIGNAL:
			case TIMER:
			case POLLER:
				break;
			default: ASSERT(false); break;
			}
			return true;
		}
		inline int establish(DSCRPTR fd, connect_handler &chd, bool success) {
			switch(m_state) {
			case HANDSHAKE: case DGHANDSHAKE: chd(fd,
				success ? S_ESTABLISH : S_EST_FAIL); break;
			case SVHANDSHAKE: chd(fd,
				success ? S_SVESTABLISH : S_SVEST_FAIL); break;
			default: ASSERT(false); return NBR_EINVAL;
			}
			return NBR_OK;
		}
		inline void close(DSCRPTR fd) {
			handshaker hs;
			if (handshakers().find_and_erase(fd, hs)) {
				/* closed during handshaking */
				TRACE("fd = %d, execute closed event\n", fd);
				ch() = hs.m_ch;
			}
			switch(m_state) {
			case DGHANDSHAKE: case DGESTABLISH:
			case HANDSHAKE: case ESTABLISH: ch()(fd, S_CLOSE); break;
			case SVHANDSHAKE: case SVESTABLISH: ch()(fd, S_SVCLOSE); break;
			case DGLISTEN: if (m_dgram) { delete m_dgram; m_dgram = NULL; } break;
			default: ASSERT(false); break;
			}
			m_state = INVALID;
		}
		inline int operator() (DSCRPTR fd, event_machine &em, event &ev) {
			int r; handshaker hs;
			switch(m_state) {
			case HANDSHAKE:
			case SVHANDSHAKE:
			case DGHANDSHAKE:
				TRACE("operator () (stream_handler)");
				if (poller::closed(ev)) {
					TRACE("closed detected: %d\n", fd);
					return em.destroy(fd);
				}
				/* do SSL negotiation or something like that. */
				else if ((r = em.proc().handshake(fd, ev)) <= 0) {
					TRACE("handshake called (%d)\n",r );
					/* back to poller or close fd */
					/* EV_WRITE for knowing connection establishment */
					if (r == NBR_OK) { return em.again_rw(fd); }
					else {
						if (handshakers().find_and_erase(fd, hs)) {
							establish(fd, hs.m_ch, false);
						}
						return em.destroy(fd);
					}
				}
				if (m_state == HANDSHAKE || m_state == DGHANDSHAKE) {
					//TRACE("fd=%d, handler=%p\n", fd, handshakers().find(fd));
					//handshakers().find(fd)->m_ch.dump();
					if (handshakers().find_and_erase(fd, hs)) {
						ASSERT(fd == hs.m_fd);
						TRACE("fd =%d, connect_handler success\n", fd);
						//hs.m_ch.dump();
						// set connect handler
						ch() = hs.m_ch;
						establish(fd, ch(), true);
						if (!poller::readable(ev)) {
							m_state = ((m_state == HANDSHAKE) ? ESTABLISH : DGESTABLISH);
							m_wr = em.proc().wp().get(fd);
							ASSERT(m_wr.valid());
							return em.again(fd);
						}
						TRACE("fd %d already readable, proceed to establish\n", fd);
					}
					/* already timed out. this fd will close soon. ignore. *OR*
					 * already closed. in that case, fd is closed. */
					else {
						/* establish(fd, ch(), false); */
						//ASSERT(false);
						return handler_result::nop;
					}
					m_state = ((m_state == HANDSHAKE) ? ESTABLISH : DGESTABLISH);
				}
				else {
					establish(fd, ch(), true);
					m_state = SVESTABLISH;
				}
				m_wr = em.proc().wp().get(fd);
				ASSERT(m_wr.valid());
			case ESTABLISH:
			case SVESTABLISH:
				//TRACE("stream_read: %p, fd = %d,", this, fd);
				if (poller::closed(ev)) {
					TRACE("closed detected: %d\n", fd);
					/* remote peer closed, close immediately this side connection.
					 * (if server closed connection before FIN from client,
					 * connection will be TIME_WAIT status and remains long time,
					 * but if not closed immediately, then CLOSE_WAIT and also remains long time...) */
					return em.destroy(fd);
					//return handler_result::nop;
				}
				r = read(em, fd);
				//TRACE("result: %d\n", r);
				return r;
			case DGESTABLISH:
			case DGLISTEN:
				if (poller::closed(ev)) {
					TRACE("dgest: closed detected: %d\n", fd);
					/* same reason as above, close immediately */
					return em.destroy(fd);
				}
				r = dgsock::read(em, fd, m_pbf, m_sr);
				//TRACE("result: %d\n", r);
				return r;
			case LISTEN:
				return accept(em, fd);
			case SIGNAL:
				ASSERT(fd == em.proc().sig().fd());
				return em.proc().sig()(em, ev);
			case POLLER:
				ASSERT(fd == em.proc().wp().fd());
				return em.proc().wp()(em, ev);
			case TIMER:
				ASSERT(fd == em.proc().timer().fd());
				return em.proc().timer()(em, ev);
			case INVALID:	/* already closed. maybe unprocessed event on em stack. */
				return handler_result::nop;
			default:
				ASSERT(false);
				return em.destroy(fd);
			}
		}
		inline int accept(event_machine &em, DSCRPTR afd) {
			address a; DSCRPTR fd;
			SKCONF skc = { 120, 65536, 65536, NULL };
			while(INVALID_FD !=
				(fd = syscall::accept(afd, a.addr_p(), a.len_p(), &skc))) {
				handler h;
				if (ah()(fd, h.ch) < 0 ||
					em.proc().attach(fd, fd_type::SERVERCONN, h, em.proc().from(afd)) < 0) {
					TRACE("accept: fail %d\n", fd);
					typename event_machine::task t(fd);
					em.que().mpush(t);
					ASSERT(false);
					return em.again(afd);
				}
			}
			/* after attach is success, it may access from another thread immediately.
			 * so should not perform any kind of write operation to m_clist[fd]. */
			return em.again(afd);
		}
		inline int read_and_parse(event_machine &em, DSCRPTR fd, int &parse_result) {
			int r;
			if (m_pbf.reserve(MINIMUM_FREE_PBUF) < 0) {
				ASSERT(false); return em.destroy(fd);
			}
#if defined(__NOUSE_PROTOCOL_PLUGIN__)
			if ((r = syscall::read(fd, m_pbf.last_p(), m_pbf.available(), NULL)) < 0) {
				ASSERT(util::syscall::error_again());
				return util::syscall::error_again() ? em.again(fd) : em.destroy(fd);
			}
#else
			/* r == 0 means EOF so we should destroy this DSCRPTR. */
			if ((r = syscall::read(fd,
				m_pbf.last_p(), m_pbf.available(), em.proc().from(fd))) <= 0) {
				TRACE("syscall::read: errno = %d %d\n", util::syscall::error_no(), r);
				ASSERT(util::syscall::error_again() || util::syscall::error_conn_reset() || r == 0);
				return (util::syscall::error_again() && r < 0) ? em.again(fd) : em.destroy(fd);
			}
#endif
			m_pbf.commit(r);
			parse_result = m_sr.unpack(m_pbf);
			return handler_result::nop;
		}
		inline int sync_write(event_machine &em, DSCRPTR fd, int timeout) {
			int r; poller::event ev;
			do {
				if ((r = loop::synchronizer().wait_event(
					fd, loop::poller::EV_WRITE, timeout, ev)) < 0) {
					return r;
				}
				if ((r = m_wr.flush_to(fd, loop::GET_TRANSPORT(fd))) == handler_result::destroy) {
					return NBR_ESEND;
				}
			} while (r != handler_result::nop);
			return NBR_OK;
		}
		inline int sync_read(event_machine &em, DSCRPTR fd, object &o, int timeout) {
			int r, parse_result;
			poller::event ev;
			do {
				do {
					if ((r = loop::synchronizer().wait_event(
						fd, loop::poller::EV_READ, timeout, ev)) < 0) {
						return r;
					}
				} while ((r = read_and_parse(em, fd, parse_result)) == handler_result::again);
				if (parse_result == SR::UNPACK_SUCCESS) { break; }
				else if (parse_result == SR::UNPACK_CONTINUE) { continue; }
				else {
					if (r == handler_result::destroy) {
						TRACE("closed connection\n");
						close(fd);
					}
					else { ASSERT(false); }
					return NBR_EINVAL;
				}
			} while (true);
			o = m_sr.result();
			return NBR_OK;
		}
		inline int read(event_machine &em, DSCRPTR fd) {
			int pres, r;
			if ((r = read_and_parse(em, fd, pres)) != handler_result::nop) {
				return r;
			}
		retry:
			switch(pres) {
			case SR::UNPACK_SUCCESS:
				if (em.proc().dispatcher().recipient().recv(get_remote_actor(), m_sr.result()) < 0) {
					return handler_result::destroy;
				}
				return handler_result::keep;
			case SR::UNPACK_EXTRA_BYTES:
				if (em.proc().dispatcher().recipient().recv(get_remote_actor(), m_sr.result()) < 0) {
					return handler_result::destroy;
				}
				pres = m_sr.unpack(m_pbf);
				goto retry;
			case SR::UNPACK_CONTINUE:
				return handler_result::keep;
			case SR::UNPACK_PARSE_ERROR:
			default:
				ASSERT(false);
				return handler_result::destroy;
			}
		}
	} *m_sh;
	static int m_nsh;
	/* connection handlers */
	struct handshaker {
		handshaker() {}
		handshaker(DSCRPTR fd, connect_handler &ch, UTIME limit) :
			m_fd(fd), m_limit(limit) { m_ch = ch; }
		DSCRPTR m_fd;
		connect_handler m_ch;
		UTIME m_limit;
	};
	static util::map<handshaker, DSCRPTR> m_hsm;
	/* thread local storage */
	RECIPIENT m_rcp;
public:
	static int init(int maxfd) {
		int r;
		if ((r = RECIPIENT::init()) < 0) { return r; }
		if (!(m_sh = new stream_handler[maxfd])) { return NBR_EMALLOC; }
		m_nsh = maxfd;
		if (!m_hsm.init(maxfd / 10, maxfd / 10, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		handler hd;
		if ((r = proc::attach(base::wp().fd(), fd_type::POLLER, hd)) < 0) {
			return r;
		}
		if ((r = proc::attach(base::sig().fd(),fd_type::SIGNAL, hd)) < 0) {
			return r;
		}
#if defined(__ENABLE_TIMER_FD__)
		if ((r = proc::attach(base::timer().fd(),fd_type::TIMER, hd)) < 0) {
			return r;
		}
#endif
		util::functional<int (timer)> h(check_timeout);
		if (!proc::timer().add_timer(h, 0.0, 1.0)) { return NBR_EEXPIRE; }
		return NBR_OK;
	}
	static inline void fin() {
		m_hsm.fin();
		if (m_sh) {
			delete []m_sh;
			m_sh = NULL;
		}
		m_nsh = 0;
		RECIPIENT::fin();
	}
	static inline stream_handler &handler_from(DSCRPTR fd) { return m_sh[fd]; }
	static inline util::map<handshaker, DSCRPTR> &handshakers() { return m_hsm; }
	static int timeout_iterator(handshaker *hs, UTIME now) {
		TRACE("check_timeout: %u, limit=%llu, now=%llu\n", hs->m_fd, hs->m_limit, now);
		if (hs->m_limit < now) {
			TRACE("check_timeout: erased %u\n", hs->m_fd);
			if (handshakers().erase_if(hs->m_fd)) {
				handler_from(hs->m_fd).establish(hs->m_fd, hs->m_ch, false);
			}
			else {
				TRACE("timeout but establish happen during timeout processed.\n");
			}
		}
		return NBR_OK;
	}
	static int check_timeout(timer t) {
		UTIME now = util::time::now();
		handshakers().iterate(timeout_iterator, now);
		return 0;
	}
	static inline int start_handshake(DSCRPTR fd, connect_handler &ch, double timeout) {
		handshaker hs(fd, ch, util::time::now() + (UTIME)(timeout * 1000 * 1000));
		return handshakers().insert(hs, fd) ? NBR_OK : NBR_EEXPIRE;
	}
	int tls_init(loop &, poller &, int, local_actor &la) { return m_rcp.tls_init(la); }
	void tls_fin() { m_rcp.tls_fin(); }
	RECIPIENT &recipient() { return m_rcp; }
	static inline bool attach(DSCRPTR fd, int type, const handler &h) {
		switch(type) {
		case fd_type::CONNECTION: return m_sh[fd].reset(fd, stream_handler::HANDSHAKE, h);
		case fd_type::LISTENER: return m_sh[fd].reset(fd, stream_handler::LISTEN, h);
		case fd_type::SIGNAL: return m_sh[fd].reset(fd, stream_handler::SIGNAL, h);
		case fd_type::POLLER: return m_sh[fd].reset(fd, stream_handler::POLLER, h);
		case fd_type::TIMER: return m_sh[fd].reset(fd, stream_handler::TIMER, h);
		case fd_type::SERVERCONN: return m_sh[fd].reset(fd, stream_handler::SVHANDSHAKE, h);
		case fd_type::DGLISTENER: return m_sh[fd].reset(fd, stream_handler::DGLISTEN, h);
		case fd_type::DGCONN: return m_sh[fd].reset(fd, stream_handler::DGHANDSHAKE, h);
		default: m_sh[fd].reset(fd, stream_handler::INVALID, h); ASSERT(false); return false;
		}
	}
	static inline int read(DSCRPTR fd, event_machine &em, poller::event &e) {
		HANDLER_TRACE("read: fd = %d\n", fd);
		return m_sh[fd].operator() (fd, em, e);
	}
	static inline int sync_read(DSCRPTR fd, event_machine &em, object &o, int timeout) {
		return m_sh[fd].sync_read(em, fd, o, timeout);
	}
	static inline int sync_write(DSCRPTR fd, event_machine &em, int timeout) {
		return m_sh[fd].sync_write(em, fd, timeout);
	}
	static inline void close(DSCRPTR fd) {
		m_sh[fd].close(fd);
	}
};

template <class SR, class EXEC>
typename stream_dispatcher<SR, EXEC>::stream_handler *
stream_dispatcher<SR, EXEC>::m_sh = NULL;

template <class SR, class EXEC>
int
stream_dispatcher<SR, EXEC>::m_nsh = 0;

template <class SR, class EXEC>
util::map<typename stream_dispatcher<SR, EXEC>::handshaker, DSCRPTR>
stream_dispatcher<SR, EXEC>::m_hsm;

}
}
}
}

#endif
