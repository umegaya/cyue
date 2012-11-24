/***************************************************************
 * watchable.h : basic definition of watchable object
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__EMITTABLE_H__)
#define __EMITTABLE_H__

#include "thread.h"
#include "functional.h"
#include "serializer.h"

#define EMIT_TRACE(...) //TRACE(__VA_ARGS__)
//#define DUMP_REFSTACK

namespace yue {

class emittable {
public:	/* emitter */
	typedef U32 event_id;
	typedef void *args;
	typedef serializer::MSGID MSGID;
	static inline bool check(emittable *) { return true; }
	class wrap {
	protected:
		emittable *m_p;
	public:
	#if defined(_DEBUG)
	#define REFER_EMWRAP(w)	{(w).refer(__FILE__,__LINE__);}
	#define UNREF_EMWRAP(w)	{(w).unref(__FILE__,__LINE__);}
		inline void refer(const char *file, int line) { if (m_p) { m_p->refer(file, line); } }
		inline void unref(const char *file, int line) { if (m_p) { m_p->unref(file, line); } }
	#else
	#define REFER_EMWRAP(w)	{(w).refer();}
	#define UNREF_EMWRAP(w)	{(w).unref();}
		inline void refer() { if (m_p) { m_p->refer(); } }
		inline void unref() { if (m_p) { m_p->unref(); } }
	#endif
		inline wrap(emittable *p) : m_p(p) { REFER_EMWRAP(*this); }
		inline wrap() : m_p(NULL) {}
		inline wrap(const wrap &cw) { 
			wrap *w = const_cast<wrap *>(&cw);
//			m_p = __sync_val_compare_and_swap(&(w->m_p), w->m_p, NULL);
			REFER_EMWRAP(*w); m_p = w->m_p;
		}
		inline ~wrap() { UNREF_EMWRAP(*this); }
		template <class T> T *unwrap() { ASSERT(T::check(m_p)); return reinterpret_cast<T*>(m_p); }
		template <class T> const T *unwrap() const { ASSERT(T::check(m_p)); return reinterpret_cast<const T*>(m_p); }
		inline bool valid () const { return m_p; }
		inline void set(emittable *p) { if (!m_p) { m_p = p; REFER_EMWRAP(*this); } }
	};
	typedef util::functional<bool (wrap, event_id, args), util::referer::counter> callback;
	class watcher : public callback {
	public:
		typedef callback super;
		template <class FUNCTOR> inline watcher(FUNCTOR &f) : super(f) {}
	};
	struct watch_entry : watcher {
		template <class FUNCTOR> inline watch_entry(FUNCTOR &f) : watcher(f) {}
		struct watch_entry *m_next;
	};
	struct nop {
		inline bool operator () (wrap, event_id, args) { return false; }
	};
	template <class T> static inline T *cast(args a) { return reinterpret_cast<T *>(a); }
	static const bool KEEP = true;
	static const bool STOP = false;
	enum {
		F_DYING = 0x01,
	};
protected:
	struct command {
		typedef enum {
			ADD_WATCHER,
			REMOVE_WATCHER,
			EMIT,
			EMIT_ONE,
			DESTROY,
		} code;
		struct add_watcher {
			watch_entry *m_w;
		};
		struct remove_watcher {
			watch_entry *m_w;
		};
		struct emit {
			event_id m_id;
			U8 m_buffer[0];
		};
		struct destroy {
			emittable *m_p;
		};
		struct command *m_next;
		U8 m_type, padd[3];
		MSGID m_respond_to;
		union {
			add_watcher m_add;
			remove_watcher m_remove;
			emit m_emit;
			destroy m_destroy;
		};
		inline void *buffer() { return reinterpret_cast<void *>(m_emit.m_buffer); }
	public:
		inline command(watch_entry *w, bool add) : 
			m_next(NULL), m_type(add ? ADD_WATCHER : REMOVE_WATCHER), m_respond_to(serializer::INVALID_MSGID) {
			add ? (m_add.m_w = w) : (m_remove.m_w = w);
		}
		inline command(emittable *p) : m_next(NULL), m_type(DESTROY), m_respond_to(serializer::INVALID_MSGID) { 
			m_destroy.m_p = p; 
		}
		inline command(event_id id, code type) : m_next(NULL), m_type(type), m_respond_to(serializer::INVALID_MSGID) { 
			m_emit.m_id = id; 
		}
		template <class T>
		inline command(event_id id, code type, const T &data) : 
			m_next(NULL), m_type(type), m_respond_to(serializer::INVALID_MSGID) {
			ASSERT(sizeof(T) <= emittable::m_command_buffer_size);
			m_emit.m_id = id;
			EMIT_TRACE("copy object\n");
			new(buffer()) T(data);
			EMIT_TRACE("end copy object\n");
		}
		inline ~command() { fin(); }
		inline void set_respond_msgid(MSGID msgid) { 
			ASSERT(m_respond_to == serializer::INVALID_MSGID);
			m_respond_to = msgid; 
		}
		void fin();
	};
protected:
	static util::array<watch_entry> m_wl;
	static util::array<command> m_cl;
	static size_t m_command_buffer_size;
	static bool m_start_finalize;
	static void (*m_finalizer)(emittable *);
	watch_entry *m_top, *m_last;
	command *m_head, *m_tail;
	util::thread::mutex m_mtx;
	volatile util::thread *m_owner;
	U8 m_type, m_flag:4, dbgf:4; S16 m_refc;
	U16 m_serial, padd;

	inline emittable(U8 type) : m_type(type), m_flag(0), m_refc(0) { init(); }
	inline ~emittable() { fin(); }
public:
	static int static_init(int maxfd, int max_command, size_t command_buffer_size, void (*finalizer)(emittable *)) {
		m_finalizer = finalizer;
		m_command_buffer_size = command_buffer_size;
		if (m_cl.init(max_command, sizeof(command) + m_command_buffer_size, 
			util::opt_threadsafe | util::opt_expandable) < 0) {
			return NBR_EMALLOC;
		}
		return m_wl.init(maxfd, -1, 
			util::opt_threadsafe | util::opt_expandable) ? NBR_OK : NBR_EMALLOC;
	}
	static void start_finalize() { m_start_finalize = true; }
	static void static_fin() {
		m_cl.fin();
		m_wl.fin();
	}
	inline int type() const { return m_type; }
	inline int refc() const { return m_refc; }
	inline U16 serial() const { return m_serial; }
	inline void update_serial() { m_serial++; }
	inline bool dying() const { return (m_flag & F_DYING); }
	inline int init() {
		m_top = m_last = NULL;
		m_head = m_tail = NULL;
		m_owner = NULL;
		/* CAUTION: this based on the util::array implementation 
		initially set all memory zero and never re-initialize when reallocation done. */
		update_serial();
		return m_mtx.init();
	}
#if defined(_DEBUG)
#define REFER_EMPTR(em)	{(em)->refer(__FILE__,__LINE__);}
#define UNREF_EMPTR(em)	{(em)->unref(__FILE__,__LINE__);}
	static const int CHECK_EMITTER = 3;
	inline void refer(const char *file, int line) {
		ASSERT(!dying());
		if (type() == CHECK_EMITTER) { TRACE("[%u:%p:refc %u -> ", m_type, this, m_refc); }
		__sync_add_and_fetch(&m_refc, 1);
		if (type() == CHECK_EMITTER) {
#if defined(DUMP_REFSTACK)
			char buff[65536];
			util::debug::btstr(buff, sizeof(buff), 0, 64);
			TRACE("%u from %s]\n", m_refc, buff);
#else
			TRACE("%u]\n", m_refc);
#endif
		}
	}
	inline void unref(const char *file, int line) {
		ASSERT(!dying());
		if (type() == CHECK_EMITTER) { TRACE("[%u:%p:refc %u -> ", m_type, this, m_refc); }
		if (__sync_add_and_fetch(&m_refc, -1) <= 0) {
			if (type() == CHECK_EMITTER) {
#if defined(DUMP_REFSTACK)
				char buff[65536];
				util::debug::btstr(buff, sizeof(buff), 0, 64);
				TRACE("%u call fzr from %s]\n", m_refc, buff);
#else
				TRACE("%u]\n", m_refc);
#endif
			}
			m_flag |= F_DYING;
			m_finalizer(this);
			return;
		}
		if (type() == CHECK_EMITTER) {
#if defined(DUMP_REFSTACK)
			char buff[65536];
			util::debug::btstr(buff, sizeof(buff), 0, 64);
			TRACE("%u from %s]\n", m_refc, buff);
#else
			TRACE("%u]\n", m_refc);
#endif
		}
	}
#else
#define REFER_EMPTR(em)	{(em)->refer();}
#define UNREF_EMPTR(em)	{(em)->unref();}
	inline void refer() { __sync_add_and_fetch(&m_refc, 1); }
	inline void unref() {
		if (__sync_add_and_fetch(&m_refc, -1) <= 0) { 
			m_flag |= F_DYING;
			m_finalizer(this);
		}
	}
#endif
public:
	template <class T>
	inline int emit(event_id id, const T &data, MSGID msgid = serializer::INVALID_MSGID) {
	TRACE("emit for %p %u %u\n", this, id, msgid);
		if (dying()) { ASSERT(false); return NBR_EINVAL; }
		command *e = m_cl.alloc<event_id, const command::code, const T>(id, command::EMIT, data);
	TRACE("allocate command object %p %u\n", e, e->m_type);
		if (!e) { ASSERT(false); return NBR_EMALLOC; }
		e->set_respond_msgid(msgid);
		return add_command(e);
	}
	template <class T>
	inline int emit_one(event_id id, const T &data, MSGID msgid = serializer::INVALID_MSGID) {
		if (dying()) { ASSERT(false); return NBR_EINVAL; }
		command *e = m_cl.alloc<event_id, const command::code, const T>(id, command::EMIT_ONE, data);
	TRACE("allocate command object %p %u\n", e, e->m_type);
		if (!e) { ASSERT(false); return NBR_EMALLOC; }
		e->set_respond_msgid(msgid);
		return add_command(e);
	}
	inline command *start_emit(event_id id) {
		if (dying()) { ASSERT(false); return NULL; }
		return m_cl.alloc<event_id, const command::code>(id, command::EMIT);
	}
	inline int commit_emit(command *e);
	inline void cancel_emit(command *e) { m_cl.free(e); }
	template <class FUNCTOR>
	inline watcher *add_watcher(FUNCTOR &f, MSGID msgid = serializer::INVALID_MSGID) {
		if (dying()) { ASSERT(false); return NULL; }
		watch_entry *w = m_wl.alloc(f);
		if (!w) { ASSERT(false); return NULL; }
		command *e = m_cl.alloc<watch_entry *, const bool>(w, true);
	TRACE("allocate command object %p %u\n", e, e->m_type);
		if (!e) { ASSERT(false); return NULL; }
		e->set_respond_msgid(msgid);
		TRACE("fiber::add_watcher %p %p %p %u\n", this, w, &f, msgid);
		return add_command(e) < 0 ? NULL : w;
	}
	inline int remove_watcher(watcher *w, MSGID msgid = serializer::INVALID_MSGID);
	inline int remove_all_watcher(bool now = false);
	//void destroy();
	inline void process_commands();
	inline void immediate_emit(event_id id, args args) {
		emit(id, args);
	}
protected:
	inline void fin() {
		//ASSERT(m_refc <= 0);
		clear_commands_and_watchers();
		m_mtx.fin();
	}
	inline void clear_commands_and_watchers() {
		TRACE("clear_commands_and_watchers for %p %p\n", this, m_top);
		watch_entry *w, *pw; command *e, *pe;
		if (m_mtx.lock() < 0) { ASSERT(false); }
		w = m_top;
		m_top = m_last = NULL;
		e = m_head;
		m_head = m_tail = NULL;
		m_mtx.unlock();
		while((pw = w)) {
			w = w->m_next;
			m_wl.free(pw);
		}
		while((pe = e)) {
			e = e->m_next;
			m_cl.free(pe);
		}
	}
	inline int add_command(command *e);
	inline void emit(event_id id, args p) {
		watch_entry *w = m_top, *pw, *remain = NULL, *last = NULL;
		m_top = NULL;
		while((pw = w)) {
			w = w->m_next;
			if (!(*pw)(wrap(this), id, p)) {
				TRACE("fiber::remove_watcher1 :%p %p\n", this, pw);
				m_wl.free(pw);
			}
			else {
				if (remain) { pw->m_next = remain; }
				else { last = pw; last->m_next = NULL; }
				remain = pw;
			}
		}
		m_top = remain;
		m_last = last;
	}
	inline void emit_one(event_id id, args p) {
		EMIT_TRACE("emit_one %d\n", id);
		if (m_top) {
			watch_entry *w = m_top;
			if (m_top == m_last) {
				m_last = m_top = m_top->m_next;
			}
			else {
				m_top = m_top->m_next;
			}
			if (!(*w)(wrap(this), id, p)) {
				TRACE("fiber::remove_watcher2 :%p %p\n", this, w);
				m_wl.free(w);
			}
			else {
				w->m_next = NULL;
				if (m_last) { m_last->m_next = w; }
				else if (m_top == m_last) { m_top = w; }
				m_last = w;
			}
		}
		else {
			ASSERT(!m_last);
		}
	}
	inline int add_watch_entry(watch_entry *w) {
		TRACE("fiber::add_watcher2 %p %p\n", this, w);
		w->m_next = m_top;
		m_top = w;
		if (!m_last) { m_last = w; }
		ASSERT(!(m_last->m_next));
		return NBR_OK;
	}
	inline int remove_watch_entry(watch_entry *w) {
		if (!w) {
			//indicate remove all watcher
			w = m_top; 
			watch_entry *pw;
			while((pw = w)) {
				w = w->m_next;
				m_wl.free(pw);
			}
			m_top = m_last = NULL;
			return NBR_OK;
		}
		if (m_top == w) {
			if (m_top == m_last) {
				m_last = m_top = m_top->m_next;
			}
			else {
				m_top = m_top->m_next;
			}
			TRACE("fiber::remove_watcher4 :%p %p\n", this, w);
			m_wl.free(w);
			return NBR_OK;
		}
		/*
		* if fiber emit something about to finish its execution, it is possible that
		* watcher try to remove is no longer exist in list (because emit sometimes remove watcher)
		*/
		if (!m_top) { 
			TRACE("remove watch: already removed? %p\n", w); 
			return NBR_ENOTFOUND; 
		}
		watch_entry *curr = m_top->m_next, *prev = m_top;
		while (curr) {
			if (curr == w) {
				prev->m_next = curr->m_next;
				if (curr == m_last) {
					ASSERT(!curr->m_next);
					m_last = prev;
				}
				TRACE("fiber::remove_watcher3 :%p %p\n", this, w);
				m_wl.free(w);
				return NBR_OK;
			}
			prev = curr;
			curr = curr->m_next;
		}
		TRACE("remove watch2: already removed? %p\n", w); 
		return NBR_ENOTFOUND;
	}
	inline void push();
};

}

/*===========================================================================================
 * typical emittable object life cycle and how server refer/unref it (with 2 worker thread)
 *
	A-permanent ref per process on runtime
	1. loop::m_hl
	2. server::m_listener_pool

	B-permanent ref per thread on runtime
	1. fiber::watcher::m_emitter (from accept watcher generated by fiber::bind)
	2. lua_State vm (through lua::emitter:base::refer)

	C-temporary ref per thread on runtime (mainly thread messaging)
	1. fabric::task::m_emitter
	2.

	D-temporary ref per process on runtime
	1. (6->7)event::listener::m_wrap (from hander::listener::emit (emittable::emit_one)) -> o (9->8)
	2. (7->8)event::listener::m_wrap (from emittable::command::command) -> o (8->7)
	3. (8->9)fabric::task::m_emit (from emittable::push (fabric::task::init_emitter) -> o (7->6)
	4. (8->9)emittable::wrap::wrap (from emittable::process_commands => emittable::emit_one) -> o (9->8)
	5. (9->10)from util::functional::operator ()??? -> ??? (11->10)
	6. (10->11)fabric::task::m_listener (from fabric::delegate_or_start_fiber) -> o (10->9)
	7. (9->10)event::proc::m_wrap (from handler::socket::read_stream) -> o (10->9)

	** start up
	+A1 0->1
	+A2 1->2
	+B1 2->3
	+C1 3->4
	+B2 4->5
	-C1 5->4
	+B1 4->5
	+C1 5->6
	+B2 6->7
	-C1 7->6
	** accept connection and process RPC
	+D1 6->7
	+D2 7->8
	+D3 8->9
	-D1 9->8
	+D4 8->9
	+D5 9->10
	+D6 10->11
	-D5 11->10
	-D6 10->9
	+D7 9->10
	-D7 10->9
	-D4 9->8
	-D2 8->7
	-D3 7->6
	** finalize
	-B2 6->5
	-B2 5->4
	-A1 4->3
	-A2 3->2
	-B1 2->1
	-B1 1->0
	finalizer call fin






 ======================================================================
 *  example 2: stable state of socket to close it.
 *

	A - stable state: reference
	1. inside VM (via lib.yue_emitter_refer)
	2. loop::m_hl (when fd connected)

	B - on close
	1. loop::read case destroy (task::io::ctor)
	2. event::session::ctor (session::emit)
	3. add_command (emittable::emit)
	4. fabric::task (fabric::task::type_emit)


	+B1 (2->3)
	+B2 (3->4)
	+B3 (4->5)
	+B4 (5->6)
	-B2 (6->5)
	-A2 (5->4)
	-B1 (4->3)
	-B3 (3->2)
	-B4 (2->1)
	-A1 (1->0)


 *===================================================================== */

#endif
