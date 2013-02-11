/***************************************************************
 * event.h : definition of event which invoke coroutine
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__EVENT_H__)
#define __EVENT_H__

#include "emittable.h"
#include "constant.h"

namespace yue {
	struct event {
		struct base {
			emittable::wrap m_emitter;
			inline void *operator new (size_t, void *p) { return p; }
			inline base(emittable *p) : m_emitter(p) {}
			inline void *ns_key() { return m_emitter.unwrap<emittable>(); }
			inline bool has_emitter() const { return m_emitter.valid(); }
			inline MSGID msgid() { return serializer::INVALID_MSGID; }
			inline void fin() { UNREF_EMWRAP(m_emitter); }
		};
		enum {
			ID_STOP = 0xFFFFFFFF,
			ID_INVALID = 0,
			ID_TIMER,
			ID_SIGNAL,
			ID_SESSION,
			ID_LISTENER,
			ID_PROC,
			ID_EMIT,
			ID_FILESYSTEM,
			ID_THREAD,
		};
		struct timer : public base {
			inline timer(emittable *p = NULL) : base(p) {}
		};
		struct signal : public base {
			int m_signo;
			inline signal(int signo, emittable *p = NULL) : base(p), m_signo(signo) {}
		};
		struct session : public base {
			int m_state;
			inline session(int state, emittable *p = NULL) : base(p), m_state(state) {}
		};
		struct listener : public base {
			emittable::wrap m_accepted;
			inline listener(emittable *p, emittable *a) : base(p), m_accepted(a) {}
			inline void *accepted_key() { return m_accepted.unwrap<emittable>(); }
			inline void fin() { UNREF_EMWRAP(m_accepted); base::fin(); }
		};
		struct proc : public base {
			object m_object;
			inline proc(emittable *p = NULL) : base(p), m_object() {}
			inline proc(object &o, emittable *p = NULL) : base(p), m_object(o) {}
			inline MSGID msgid() const { return m_object.msgid(); }
			inline void fin() { m_object.fin(); base::fin(); }
		};
		struct emit : public base {
			object m_object;
			inline emit(emittable *p = NULL) : base(p), m_object() {}
			inline void fin() { m_object.fin(); base::fin(); }
		};
		struct fs : public base {
			handler::fs::notification *m_notify;
			inline fs(handler::fs::notification *n, emittable *p = NULL) : base(p), m_notify(n) {}
		};
		struct thread : public base {
			inline thread(emittable *p = NULL) : base(p) {}
		};
		struct error { 
			constant::error::entry *m_error;
			const char *m_msg;
			inline void *operator new (size_t, void *p) { return p; }
			inline error(constant::error::entry &err, const char *msg = NULL) : m_error(&err), m_msg(msg) {}
		};
	};
}
#endif
