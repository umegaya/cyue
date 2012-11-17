/***************************************************************
 * watchable.h : basic definition of watchable object
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "emittable.h"

namespace yue {
void (*emittable::m_finalizer)(emittable *) = NULL;
size_t emittable::m_command_buffer_size = -1;
util::array<emittable::watch_entry> emittable::m_wl;
bool emittable::m_start_finalize = false;
util::array<emittable::command> emittable::m_cl;
}

