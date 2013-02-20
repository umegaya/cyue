/***************************************************************
 * fiber.h : programmable coroutine
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__FIBER_H__)
#define __FIBER_H__

#include "loop.h"
#include "ll.h"
#include "serializer.h"
#include "error.h"
#include "endpoint.h"
#include "constant.h"
#include "emittable.h"

namespace yue {
class fiber : public constant::fiber {
public:
	struct watcher {
		static const MSGID INVALID_MSGID = serializer::INVALID_MSGID;
		volatile emittable::event_id m_id;
		union {
			const char *m_procname;
			U32 m_flag;
			U64 m_lflag;
		};
		union {
			MSGID m_msgid;
			server *m_owner;
		};
		fiber *m_fb;
		emittable::wrap m_emitter;
		emittable::watcher *m_watcher;
	public:
		inline watcher(emittable::event_id id, fiber *fb, emittable *p) : m_id(id), m_fb(fb), m_emitter(p) {
			ASSERT(m_id != event::ID_STOP && m_id != event::ID_PROC && m_id != event::ID_SESSION);
			m_watcher = NULL;
		}
		inline watcher(emittable::event_id id, fiber *fb, emittable *p, const char *procname)
			: m_id(id), m_fb(fb), m_emitter(p){
			m_procname = procname;
			m_watcher = NULL;
		}
		inline watcher(emittable::event_id id, fiber *fb, emittable *p, U32 flag)
			: m_id(id), m_fb(fb), m_emitter(p) {
			m_flag = flag;
			m_watcher = NULL;
		}
		inline int watch(MSGID msgid = serializer::INVALID_MSGID) {
			return ((m_watcher = m_emitter.unwrap<emittable>()->add_watcher(*this, msgid))) ? NBR_OK : NBR_EEXPIRE;
		}
		inline void unwatch(MSGID msgid = serializer::INVALID_MSGID) {
			stop();	/* indicate stop watching in case any emit occurs
			before remove_watcher event processed */
			if (m_watcher) { m_emitter.unwrap<emittable>()->remove_watcher(m_watcher, msgid); }
		}
		inline bool filter(emittable::event_id id, emittable::args p);
		int operator () (emittable::wrap &e, emittable::event_id id, emittable::args p);
		inline void refer() {}
		inline void unref();
		inline void wait() { m_msgid = serializer::new_id(); }
		inline void bind();
	public:
		inline emittable::event_id event_id() const { return m_id; }
		inline bool bound() const { return m_fb == NULL; }
		inline void stop() { m_id = event::ID_STOP; }
		inline bool stopped() const { return m_id == event::ID_STOP; }
		inline MSGID msgid() const;
		inline server *owner() const { return bound() ? m_owner : NULL; }
	};
protected:
	static util::array<watcher> m_watcher_pool;
public:
	MSGID m_msgid;
	rpc::endpoint m_endp;
	server *m_owner;
	U16 m_flag, padd;
	watcher *m_w;
	//CAUTION: this should be last declaration of fiber class member.(for make coroutine::fb() work)
	ll::coroutine m_co;
public:
	template <class ENDPOINT>
	inline fiber(ENDPOINT &ep);
	inline int init();
	inline void fin();
	const rpc::endpoint& endp() const { return m_endp; }
	rpc::endpoint &endp() { return m_endp; }
	ll::coroutine *co() { return &m_co; }
	server &owner() { ASSERT(m_owner); return *m_owner; }
	MSGID msgid() const { return m_msgid; }
	void set_msgid(MSGID msgid) { m_msgid = msgid; }
	static inline util::array<watcher> &watcher_pool() { return m_watcher_pool; }
public:
	inline int respond(int result);
	inline int raise(event::error &e);
	template <class EVENT>
	inline int start(EVENT &ev);
	inline int start(int n_args = 0) { return resume(n_args); }
	inline int start(emittable::event_id id, emittable::args args);
	template <class EVENT>
	inline int resume(EVENT &ev);
	inline int resume(int n_args = 0);
	inline int resume(emittable::event_id id, emittable::args args);
	inline int wait(emittable::event_id id, emittable *e, U32 timeout = 0);
	template <class ARG>
	inline int wait(emittable::event_id id, emittable *e, ARG a, U32 timeout = 0);
	inline int wait(U32 timeout);
	static inline int bind(emittable::event_id id, emittable *e, fiber *wfb = NULL, U32 timeout = 0);
	template <class ARG>
	static inline int bind(emittable::event_id id, emittable *e, ARG a, fiber *wfb = NULL, U32 timeout = 0);
	inline int emit(emittable *p, event::emit &e, U32 timeout);
	inline int finish_wait(watcher *w) {
		//if fiber::wait called during emittable::emit processed in process_command, it is possible that
		//w and m_w different (because m_w is updated by fiber::wait
		//TODO: m_w should be list.
		if (m_w && (m_w == w)) { m_w = NULL; }
		return NBR_OK;
	}
public:	//pack callback
	inline int operator () (serializer &sr) const {
		ASSERT(serializer::INVALID_MSGID == 0);
		/* array len also packed in following method
		 * (eg. lua can return multiple value) */
		verify_success(m_co.pack_response(sr));
		return sr.len();
	}
	inline int pack(serializer &sr) const {
		return sr.pack_response(*this, m_msgid);
	}
	inline int pack_error(serializer &sr) {
		return m_co.pack_error(sr);
	}
public:
	enum {
		flag_unremovable = 0x1,
	};
	inline void set_unremovable(bool on) {
		if (on) { m_flag |= flag_unremovable; }
		else { m_flag &= ~(flag_unremovable); }
	}
	inline bool removable() const { 
		return !(m_flag & flag_unremovable);
	}
};
}
#endif

