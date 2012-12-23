/***************************************************************
 * fabric.h : fiber factory
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__FABRIC_H__)
#define __FABRIC_H__

#include "map.h"
#include "ll.h"
#include "serializer.h"
#include "fiber.h"
#include "event.h"
#include "emittable.h"

namespace yue {
class server;
struct config;
class fabric {
	typedef emittable::event_id event_id;
	typedef emittable::args args;
	struct yielded {
		fiber *m_f;
		UTIME m_limit;
		MSGID m_msgid;
		inline void set(fiber *f, MSGID key, U32 t_o) {
			m_f = f; setkey(key);
			m_limit = (util::time::now() + t_o); 
		}
		inline bool timeout(UTIME now) const { return (now > m_limit); }
		inline bool removable() const { return m_f->removable(); }
		inline MSGID msgid() const { return m_msgid; }
		inline void setkey(MSGID msgid) { m_msgid = msgid; }
	};
	static util::map<yielded, MSGID> m_yielded_fibers;
	static int m_max_fiber, m_max_object, m_fiber_timeout_us, m_fiber_pool_size, m_timeout_check_intv;
	ll m_lang;
	serializer m_packer;
	util::array<fiber> m_fiber_pool;
	server *m_server;
protected:
	static int timeout_iterator(yielded *py, UTIME &now) {
		TRACE("check_timeout: %p thrs=%u\n", py, m_fiber_timeout_us);
		if (py->timeout(now)) {
			TRACE("check_timeout: erased %p, %u\n", py, py->msgid());
			yielded y;
			if (yielded_fibers().find_and_erase(py->msgid(), y)) {
				event::error e(NBR_ETIMEOUT);
				y.m_f->raise(e);
			}
		}
		return NBR_OK;
	}
	static int check_timeout(loop::timer_handle t) {
		UTIME now = util::time::now();
		yielded_fibers().iterate(timeout_iterator, now);
		return NBR_OK;
	}
	static inline yielded *alloc_yield_context(MSGID msgid) {
		return yielded_fibers().alloc(msgid);
	}
public:
	struct task {
	public:
		enum {
			type_invalid,
			type_event_proc,
			type_event_emit,
			type_event_session,
			type_event_timer,
			type_event_signal,
			type_event_listener,
			type_event_fs,
			type_event_thread,
			type_event_error,
			type_start_proc,
			type_start_emit,
			type_start_session,
			type_start_timer,
			type_start_signal,
			type_start_listener,
			type_start_fs,
			type_start_thread,
			type_emit,
			type_thread_message,
			type_destroy_fiber,
			type_unref_emitter,
			type_delegate_fiber,
		};
	protected:
		struct emitter : public emittable::wrap {
			inline emitter(emittable *p) : emittable::wrap(p) {}
			static inline void *operator new (size_t sz, void *p) { ASSERT(sz == sizeof(wrap)); return p; }
			inline void reset(emittable *p) { if (!m_p) { m_p = p; } }
		};
		U8 m_type, padd[3];
		union {
			server *m_server;
			fiber *m_fiber;
			U8 m_emitter[sizeof(emitter)];
		};
		union {
			U8 m_proc[sizeof(event::proc)];
			U8 m_emit[sizeof(event::emit)];
			U8 m_session[sizeof(event::session)];
			U8 m_timer[sizeof(event::timer)];
			U8 m_signal[sizeof(event::signal)];
			U8 m_listener[sizeof(event::listener)];
			U8 m_fs[sizeof(event::fs)];
			U8 m_thread[sizeof(event::thread)];
			U8 m_error[sizeof(rpc::error)];
			int m_args;
		};
		inline event::proc &proc_ref() { return *(reinterpret_cast<event::proc *>(m_proc)); }
		inline event::emit &emit_ref() { return *(reinterpret_cast<event::emit *>(m_emit)); }
		inline event::session &session_ref() { return *(reinterpret_cast<event::session *>(m_session)); }
		inline event::timer &timer_ref() { return *(reinterpret_cast<event::timer *>(m_timer)); }
		inline event::signal &signal_ref() { return *(reinterpret_cast<event::signal *>(m_signal)); }
		inline event::listener &listener_ref() { return *(reinterpret_cast<event::listener *>(m_listener)); }
		inline event::fs &fs_ref() { return *(reinterpret_cast<event::fs *>(m_fs)); }
		inline event::thread &thread_ref() { return *(reinterpret_cast<event::thread *>(m_thread)); }
		inline event::error &error_ref() { return *(reinterpret_cast<event::error *>(m_error)); }
		inline emitter &emitter_ref() { return *(reinterpret_cast<emitter *>(m_emitter)); }
		inline void init_emitter(emittable *e) { new (reinterpret_cast<void *>(m_emitter)) emitter(e); }
		inline void init_proc(object &o, emittable *thrd) { new (reinterpret_cast<void *>(m_proc)) event::proc(o, thrd); }
	public:
		inline task() : m_type(type_invalid) {}
		inline task(fiber *f, event::proc &ev) : m_type(f ? type_event_proc : type_start_proc), m_fiber(f) { new(m_proc) event::proc(ev); }
		inline task(fiber *f, event::emit &ev) : m_type(f ? type_event_emit : type_start_emit), m_fiber(f) { new(m_emit) event::emit(ev); }
		inline task(fiber *f, event::session &ev) : m_type(f ? type_event_session : type_start_session), m_fiber(f){ new (m_session) event::session(ev); }
		inline task(fiber *f, event::timer &ev) : m_type(f ? type_event_timer : type_start_timer), m_fiber(f) { new (m_timer) event::timer(ev); }
		inline task(fiber *f, event::signal &ev) : m_type(f ? type_event_signal : type_start_signal), m_fiber(f){ new (m_signal) event::signal(ev); }
		inline task(fiber *f, event::listener &ev) : m_type(f ? type_event_listener : type_start_listener), m_fiber(f){ new (m_listener) event::listener(ev); }
		inline task(fiber *f, event::fs &ev) : m_type(f ? type_event_fs : type_start_fs), m_fiber(f){ new (m_fs) event::fs(ev); }
		inline task(fiber *f, event::thread &ev) : m_type(f ? type_event_thread : type_start_thread), m_fiber(f){ new (m_thread) event::thread(ev); }
		inline task(fiber *f, event::error &e) : m_type(type_event_error), m_fiber(f){ new (m_error) event::error(e); }
		inline task(emittable *e, bool fin) : m_type(fin ? type_unref_emitter : type_emit) { init_emitter(e); }
		inline task(server *sender, event::proc &ev) : m_type(type_thread_message), m_server(sender) { new (m_proc) event::proc(ev); }
		inline task(server *sender, object &o, emittable *thrd) : m_type(type_thread_message), m_server(sender) { init_proc(o, thrd); }
		inline task(fiber *f) : m_type(type_destroy_fiber), m_fiber(f) {}
		inline task(fiber *f, int n_args) : m_type(type_delegate_fiber), m_fiber(f) { m_args = n_args; }
		inline void operator () (server &s);
		inline int type() const { return m_type; }
	};
public:	
	fabric() : m_lang(), m_packer(), m_server(NULL)  {}
	inline ll &lang() { return m_lang; }
	inline serializer &packer() { return m_packer; }
	inline server *served() { return m_server; }
	static inline U32 fiber_timeout_us() { return m_fiber_timeout_us; }
	static inline util::map<yielded, MSGID> &yielded_fibers() { return m_yielded_fibers; }
public:	
	static int static_init(struct config &cfg);
	static void static_fin() {
		ll::static_fin();
		m_yielded_fibers.fin();
	}
	int init(const util::app &a, server *l);
	void fin_lang() { lang().fin(); }
	void fin() {
		lang().fin();
		m_fiber_pool.fin();
	}
	static inline fabric &tlf();
public:
	template <class ENDPOINT>
	inline fiber *create(ENDPOINT endp) {
		int r; fiber *f = m_fiber_pool.alloc(endp);
		if (!f) { return NULL; }
		if ((r = f->init()) < 0) {
			destroy(f);
			return NULL;
		}
		return f;
	}
	inline fiber *create() {
		return create(rpc::endpoint::nop());
	}
	inline void destroy(fiber *f) {
		m_fiber_pool.free(f);
	}
	template <class EVENT>
	inline int start(EVENT &ev) {
		rpc::endpoint::nop np;
		return start(ev, np);
	}
	inline int execute(const char *code_or_file) {
		int r; rpc::endpoint::nop np;
		fiber *f = create(np);
		if ((r = lang().eval(code_or_file, f->co())) < 0) {
			return r;
		}
		return f->resume();
	}
	template <class EVENT, class ENDPOINT>
	inline int start(EVENT &ev, ENDPOINT &endp) {
		fiber *f = create(endp);
		return f ? f->start(ev) : NBR_EMALLOC;
	}
	template <class ENDPOINT>
	inline int start(event::proc &ev, ENDPOINT &endp) {
		fiber *f = create(endp);
		if (!f) { return NBR_EMALLOC; }
		f->set_msgid(ev.m_object.msgid());
		return f->start(ev);
	}
	static inline int yield(fiber *f, MSGID key, U32 timeout_us = 0) {
		TRACE("yield: %p %u %u\n", f, key, timeout_us);
		if (timeout_us == 0) { timeout_us = fiber_timeout_us(); }
		yielded *py = alloc_yield_context(key);
		if (!py) { return NBR_EMALLOC; }
		py->set(f, key, timeout_us);
		return NBR_OK;
	}
	template <class ENDPOINT, class EVENT>
	inline int recv(ENDPOINT &ep, EVENT &p) {
		object &o = p.m_object;
		if (o.is_request()) {
			start(p, ep);
			goto end;
		}
		else if (o.is_response()) {
			TRACE("recv resp: msgid = %u\n", o.msgid());
			yielded y;
			if (!yielded_fibers().find_and_erase_if(o.msgid(), y)) {
				TRACE("recv resp: fb for msgid = %u not found\n", o.msgid());
				goto end;
			}
			y.m_f->resume(p);
			goto end;
		}
		else if (o.is_notify() || true/* and others */) {
			ASSERT(false);
			goto end;
		}
	end:
		o.fin();
		return NBR_OK;
	}
	template <class EVENT>
	inline int delegate_or_start_fiber(server *owner, emittable::args args);
	inline int start_fiber(server *owner, event_id id, emittable::args p);
	inline bool recv(fiber::watcher &w, emittable::event_id id, emittable::args p) {
		TRACE("recv resp (emit): msgid = %u %s\n", w.msgid(), w.bound() ? "bind" : "wait");
		if (w.bound()) {
			start_fiber(w.owner(), id, p);
			return emittable::KEEP;
		}
		yielded y;
		if (!yielded_fibers().find_and_erase_if(w.msgid(), y)) {
			TRACE("recv resp: fb for watcher msgid = %u not found\n", w.msgid());
			return emittable::STOP;
		}
		y.m_f->resume(id, p);
		return emittable::STOP;
	}
	inline void resume_fiber(MSGID msgid) {
		yielded y;
		if (!yielded_fibers().find_and_erase_if(msgid, y)) {
			TRACE("resume_fiber: msgid = %u not found\n", msgid);
			return;
		}
		y.m_f->resume();
	}
};
}
#endif

