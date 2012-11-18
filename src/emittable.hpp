/***************************************************************
 * emittable.hpp : basic definition of watchable object
 * 2012/01/07 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__EMITTABLE_HPP__)
#define __EMITTABLE_HPP__

namespace yue {
void emittable::push() {
	fabric::task t(this, false);
	server::tlsv()->fque().mpush(t);
}
int emittable::commit_emit(command *e) { 
	return add_command(e); 
}
int emittable::remove_watcher(watcher *w, MSGID msgid) {
	if (dying()) { ASSERT(false); return NBR_EINVAL; }
	watch_entry *tmp = reinterpret_cast<watch_entry *>(w);
	command *e = m_cl.alloc<watch_entry*, const bool>(tmp, false);
	TRACE("allocate command object %p %u\n", e, e->m_type);
	if (!e) { return NBR_EMALLOC; }
	e->set_respond_msgid(msgid);
	return add_command(e);
}
int emittable::remove_all_watcher(bool now) { 
	return now ? remove_watch_entry(NULL) : remove_watcher(NULL); 
}
int emittable::add_command(command *e) {
	if (m_mtx.lock() < 0) { ASSERT(false); return NBR_EPTHREAD; }
	if (m_tail) { m_tail->m_next = e; }
	else { m_head = e; }
	e->m_next = NULL;
	m_tail = e;
	m_mtx.unlock();
	if (__sync_bool_compare_and_swap(&m_owner, NULL, util::thread::current())) {
		push();
	}
	return NBR_OK;
}
void emittable::process_commands() {
	ASSERT(m_owner == util::thread::current());
	if (m_mtx.lock() < 0) { ASSERT(false); return; }
	command *e = m_head, *pe;
	m_head = m_tail = NULL;
	m_mtx.unlock();
	while((pe = e)) {
	TRACE("process_commands %p %u\n", pe, pe->m_type);
		e = e->m_next;
		switch (pe->m_type) {
		case command::ADD_WATCHER:
			add_watch_entry(pe->m_add.m_w);
			break;
		case command::REMOVE_WATCHER:
			remove_watch_entry(pe->m_remove.m_w);
			break;
		case command::EMIT:
			emit(pe->m_emit.m_id, pe->buffer());
			break;
		case command::EMIT_ONE:
			emit_one(pe->m_emit.m_id, pe->buffer());
			break;
		case command::DESTROY:
			ASSERT(!e);	//have to be last one
			m_finalizer(this);
			m_cl.free(pe);
			return;
		default:
			ASSERT(false);
			break;
		}
		if (pe->m_respond_to != serializer::INVALID_MSGID) {
		TRACE("respond to finish of command to %u\n", pe->m_respond_to);
			server::tlsv()->fbr().resume_fiber(pe->m_respond_to);
		}
		m_cl.free(pe);
	}
	//unlock m_owner
	m_owner = NULL;
	if (m_head) {
		//if new command pushed during processing commands, process this emittable by this thread again.
		//TODO: if emit freqency is so high,
		// this emitter will almost always bind to this thread. should choose another thread?
		if (__sync_bool_compare_and_swap(&m_owner, NULL, util::thread::current())) {
			push();
		}
	}
}

inline void emittable::command::fin() {
	switch(m_type) {
	case ADD_WATCHER:
	case REMOVE_WATCHER:
		break;
	case EMIT:
	case EMIT_ONE:
		switch(m_emit.m_id) {
		case event::ID_TIMER:
			reinterpret_cast<event::timer *>(buffer())->fin();
			break;
		case event::ID_SIGNAL:
			reinterpret_cast<event::signal *>(buffer())->fin();
			break;
		case event::ID_SESSION:
			reinterpret_cast<event::session *>(buffer())->fin();
			break;
		case event::ID_LISTENER:
			reinterpret_cast<event::listener *>(buffer())->fin();
			break;
		case event::ID_PROC:
			reinterpret_cast<event::proc *>(buffer())->fin();
			break;
		case event::ID_EMIT:
			reinterpret_cast<event::emit *>(buffer())->fin();
			break;
		case event::ID_FILESYSTEM:
			reinterpret_cast<event::fs *>(buffer())->fin();
			break;
		case event::ID_THREAD:
			reinterpret_cast<event::thread *>(buffer())->fin();
			break;
		default:
			ASSERT(false);
			break;
		}
	}
}

}

#endif
