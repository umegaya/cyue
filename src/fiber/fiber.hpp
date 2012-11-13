/***************************************************************
 * fiber.hpp : implementation of fiber related inline functions
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/

namespace yue {
/* implementation of class fiber::watcher */
inline bool fiber::watcher::operator () (
	emittable::wrap &, emittable::event_id id, emittable::args p) {
	if (stopped()) { return emittable::STOP; }
	if (!filter(id, p)) { return emittable::KEEP; }
	if (m_fb) { m_fb->finish_wait(this); }	//fiber::wait finished
	return fabric::tlf().recv(*this, id, p);
}
inline MSGID fiber::watcher::msgid() const {
#if defined(_DEBUG) || defined(__NBR_OSX__)
	return bound() ? serializer::invalid_id() : m_msgid;
#else
	return bound() ? serializer::INVALID_MSGID : m_msgid;
#endif
}
inline void fiber::watcher::bind() {
	m_owner = server::tlsv();
}
inline bool fiber::watcher::filter(emittable::event_id id, emittable::args p) {
	if (event_id() != id) {
		return false;
	}
	switch (id) {
	case event::ID_TIMER:
	case event::ID_SIGNAL:
	case event::ID_LISTENER: {
		return true;
	}
	case event::ID_SESSION: {
		event::session *ev = emittable::cast<event::session>(p);
		return (m_flag & (1 << ev->m_state));
	}
	case event::ID_PROC: {
		event::proc *ev = emittable::cast<event::proc>(p);
		return util::str::cmp(ev->m_object.cmd(), m_procname) == 0;
	}
	case event::ID_FILESYSTEM: {
		event::fs *ev = emittable::cast<event::fs>(p);
		return (ev->m_notify->flag(m_flag));
	}
	default:
		ASSERT(false);
		return false;
	}

}
inline void fiber::watcher::unref() {
	TRACE("fiber::watcher::unref %p\n", this);
	fiber::watcher_pool().free(this);
}
/* implementation for class fiber */
template <class ENDPOINT>
inline fiber::fiber(ENDPOINT &ep) : m_msgid(serializer::INVALID_MSGID), m_endp(ep), m_flag(0), m_w(NULL), m_co() {
	m_owner = server::tlsv(); 
}
inline int fiber::init() {
	return (NULL != m_owner->fbr().lang().create(this) ? NBR_OK : NBR_EMALLOC);
}
inline void fiber::fin() { 
	if (m_owner != server::tlsv()) {
		fabric::task t(this, fabric::task::type_destroy_fiber);
		if (m_owner->fque().mpush(t)) { return; }
		ASSERT(false);
		return;
	}
	if (m_w) {
		m_w->unwatch();
		/* m_w freed inside unwatch (via functional::m_destroy ( == fiber::watcher::unref) */
		m_w = NULL;
	}
	m_owner->fbr().lang().destroy(&m_co);
	m_owner->fbr().destroy(this); 
}
inline int fiber::respond(int result) {
	switch(result) {
	case fiber::exec_yield:	 return NBR_OK;
	case fiber::exec_finish: {
		result = m_endp.send(m_owner->fbr().packer(), *this);
		fin();
		return result;	
	}	break; 
	case fiber::exec_error:	{
		rpc::error e(rpc::error::E_APP_RUNTIME, m_msgid, this);
		result = m_endp.send(m_owner->fbr().packer(), e);
		fin();
		return result;	
	}	break;
	default: ASSERT(false);  return NBR_EINVAL;
	}
}
inline int fiber::raise(rpc::error &e) {
	return m_endp.send(m_owner->fbr().packer(), e);
}
template <class EVENT>
inline int fiber::start(EVENT &ev) {
	m_msgid = ev.msgid();
	return respond(m_co.start(ev));
}
inline int fiber::wait(emittable::event_id id, emittable *e, U32 timeout) {
	if (!(m_w = m_watcher_pool.alloc(id, this, e))) {
		return NBR_EMALLOC;
	}
	TRACE("fiber::wait %p\n", m_w);
	m_w->wait();
	return wait(timeout);
}
template <class ARG>
inline int fiber::wait(emittable::event_id id, emittable *e, ARG a, U32 timeout) {
	if (!(m_w = m_watcher_pool.alloc(id, this, e, a))) {
		return NBR_EMALLOC;
	}
	TRACE("fiber::wait2 %p\n", m_w);

	m_w->wait();
	return wait(timeout);
}
inline int fiber::wait(U32 timeout) {
	if (fabric::yield(this, m_w->msgid(), timeout) < 0) {
		m_w->unref();
		m_w = NULL;
		ASSERT(false);
		return NBR_EMALLOC;
	}
	if (m_w->watch() < 0) {
		m_w->unwatch();
		m_w = NULL;
		ASSERT(false);
		return NBR_EPTHREAD;
	}
	return NBR_OK;
}
inline int fiber::bind(emittable::event_id id, emittable *e, fiber *wfb, U32 timeout) {
	watcher *w; fiber *fb = NULL;
	if (!(w = m_watcher_pool.alloc(id, fb, e))) {
		return NBR_EMALLOC;
	}
	TRACE("fiber::bind %p (%p)\n", w, wfb);
	MSGID msgid = serializer::INVALID_MSGID;
	if (wfb) {
		msgid = serializer::new_id();
		if (fabric::yield(wfb, msgid, timeout) < 0) {
			w->unref();
			w = NULL;
			ASSERT(false);
			return NBR_EMALLOC;
		}
		TRACE("bind complete watched by %p (%u)\n", wfb, msgid);
	}
	w->bind();
	if (w->watch(msgid) < 0) {
		w->unwatch();
		return NBR_EPTHREAD;
	}
	return NBR_OK;
}
template <class ARG>
inline int fiber::bind(emittable::event_id id, emittable *e, ARG a, fiber *wfb, U32 timeout) {
	watcher *w; fiber *fb = NULL;
	if (!(w = m_watcher_pool.alloc(id, fb, e, a))) {
		return NBR_EMALLOC;
	}
	TRACE("fiber::bind2 %p (%p)\n", w, wfb);
	MSGID msgid = serializer::INVALID_MSGID;
	if (wfb) {
		msgid = serializer::new_id();
		if (fabric::yield(wfb, msgid, timeout) < 0) {
			w->unref();
			w = NULL;
			ASSERT(false);
			return NBR_EMALLOC;
		}
		TRACE("bind2 complete watched by %p (%u)\n", wfb, msgid);
	}
	w->bind();
	if (w->watch(msgid) < 0) {
		w->unwatch();
		return NBR_EPTHREAD;
	}
	return NBR_OK;
}
template <class EVENT>
inline int fiber::resume(EVENT &ev) {
	if (m_owner != server::tlsv()) {
		fabric::task t(this, ev);
		if (m_owner->fque().mpush(t)) { return NBR_OK; }
		ASSERT(false);
		return NBR_EMALLOC;
	}
	return respond(m_co.resume(ev));
}
inline int fiber::resume() {
	if (m_owner != server::tlsv()) {
		fabric::task t(this, fabric::task::type_delegate_fiber);
		if (m_owner->fque().mpush(t)) { return NBR_OK; }
		ASSERT(false);
		return NBR_EMALLOC;
	}
	return respond(m_co.resume());
}
inline int fiber::resume(emittable::event_id id, emittable::args args) {
	switch (id) {
	case event::ID_TIMER: {
		event::timer *ev = emittable::cast<event::timer>(args);
		return resume(*ev);
	}
	case event::ID_SIGNAL: {
		event::signal *ev = emittable::cast<event::signal>(args);
		return resume(*ev);
	}
	case event::ID_SESSION: {
		event::session *ev = emittable::cast<event::session>(args);
		return resume(*ev);
	}
	case event::ID_LISTENER: {
		event::listener *ev = emittable::cast<event::listener>(args);
		return resume(*ev);
	}
	case event::ID_EMIT: {
		event::emit *ev = emittable::cast<event::emit>(args);
		return resume(*ev);
	}
	case event::ID_PROC:	//proc never act as emit now
	default:
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
}
template <class EVENT>
inline int fabric::delegate_or_start_fiber(server *owner, emittable::args args) {
	EVENT *ev = emittable::cast<EVENT>(args);
	if (m_server != owner) {
		fabric::task t(NULL, *ev);
TRACE("delegate_or_start_fiber delegate:%p %u\n", owner, t.type());
		return owner->fque().mpush(t) ? NBR_OK : NBR_EEXPIRE;
	}
TRACE("delegate_or_start_fiber start:%p\n", owner);
	return start(*ev);
}
inline int fabric::start_fiber(server *owner, emittable::event_id id, emittable::args args) {
	switch (id) {
	case event::ID_TIMER: return delegate_or_start_fiber<event::timer>(owner, args);
	case event::ID_SIGNAL: return delegate_or_start_fiber<event::signal>(owner, args);
	case event::ID_SESSION: return delegate_or_start_fiber<event::session>(owner, args);
	case event::ID_LISTENER: return delegate_or_start_fiber<event::listener>(owner, args);
	case event::ID_EMIT: return delegate_or_start_fiber<event::emit>(owner, args);
	case event::ID_PROC:	//proc never act as emit now
	default:
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
}
inline fabric &fabric::tlf() {
	return server::tlsv()->fbr();
}


/* implementation for class endpoint */
template <class SR, class OBJ>
inline int rpc::endpoint::local::send(SR &sr, OBJ &o) {
	return m_l->template send<SR, OBJ>(sr, o);
}



/* implementation for class server */
template <class SR, class OBJ>
inline int server::send(SR &sr, OBJ &obj) {
	int r;
	if ((r = SR::pack_as_object(obj, sr)) < 0) {
		return r;
	}
	object o = sr.result();
	fabric::task t(server::tlsv(), o);
	r = (fque().mpush(t) ? NBR_OK : NBR_EEXPIRE);
	o.fin();
	return r;
	
}



/* implementation for class fabric */
inline void fabric::task::operator () (server &s) {
	switch (m_type) {
	case type_event_proc:
		m_fiber->resume(proc_ref()); 
		proc_ref().fin();
		break;
	case type_event_emit:
		m_fiber->resume(emit_ref());
		emit_ref().fin();
		break;
	case type_event_session:
		m_fiber->resume(session_ref());
		session_ref().fin();
		break;
	case type_event_timer:
		m_fiber->resume(timer_ref());
		timer_ref().fin();
		break;
	case type_event_signal:
		m_fiber->resume(signal_ref());
		signal_ref().fin();
		break;
	case type_event_listener:
		m_fiber->resume(listener_ref());
		listener_ref().fin();
		break;
	case type_event_fs:
		m_fiber->resume(fs_ref());
		fs_ref().fin();
		break;
	case type_event_thread:
		m_fiber->resume(thread_ref());
		thread_ref().fin();
		break;
	case type_start_proc: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(proc_ref()); }
		proc_ref().fin();
	} break;
	case type_start_emit: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(emit_ref()); }
		emit_ref().fin();
	} break;
	case type_start_session: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(session_ref()); }
		session_ref().fin();
	} break;
	case type_start_timer: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(timer_ref()); }
		timer_ref().fin();
	} break;
	case type_start_signal: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(signal_ref()); }
		signal_ref().fin();
	} break;
	case type_start_listener: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(listener_ref()); }
		listener_ref().fin();
	} break;
	case type_start_fs: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(fs_ref()); }
		fs_ref().fin();
	} break;
	case type_start_thread: {
		fiber *f = fabric::tlf().create();
		if (f) { f->start(thread_ref()); }
		thread_ref().fin();
	} break;
	case type_emit:
		emitter_ref().unwrap<emittable>()->process_commands();
		UNREF_EMWRAP(emitter_ref());
		break;
	case type_thread_message: {
		rpc::endpoint::local ep(m_server);
		s.fbr().recv(ep, proc_ref());
	} break;
	case type_destroy_fiber: {
		m_fiber->fin();
	} break;
	case type_unref_emitter: {
		UNREF_EMWRAP(emitter_ref());
	} break;
	case type_delegate_fiber: {
		m_fiber->resume();
	} break;
	default: ASSERT(false); break;
	}
}



/* implementation for namespace yue::rpc */
namespace rpc {
typedef handler::socket remote;
typedef server::peer peer;
typedef server local;
/* senders */
template <class ARGS>
static inline MSGID call(remote &ss, fabric &fbr, ARGS &a) {
	if (ss.writeo(fbr.packer(), a) < 0) { return serializer::INVALID_MSGID; }
	return a.m_msgid;
}
template <class ARGS>
static inline MSGID call(local &la, fabric &fbr, ARGS &a) {
	if (la.send(fbr.packer(), a) < 0) { return serializer::INVALID_MSGID; }
	return a.m_msgid;
}
template <class ARGS>
static inline MSGID call(peer &p, fabric &fbr, ARGS &a) {
	if (p.s()->writeo(fbr.packer(), a, &(p.addr())) < 0) { return serializer::INVALID_MSGID; }
	return a.m_msgid;
}
template <class ARGS>
static inline MSGID call(remote &ss, ARGS &a) {
	return call(ss, fabric::tlf(), a);
}
template <class ARGS>
static inline MSGID call(local &la, ARGS &a) {
	return call(la, fabric::tlf(), a);
}
template <class ARGS>
static inline MSGID call(peer &p, ARGS &a) {
	return call(p, fabric::tlf(), a);
}
/* error object */
inline int error::operator () (serializer &sr) {
	verify_success(sr.push_array_len(2));
	verify_success(sr << m_errno);
	verify_success(m_fb->pack_error(sr));
	return sr.len();
}
}
}
