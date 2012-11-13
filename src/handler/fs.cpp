/***************************************************************
 * fs.cpp : file system watcher
 * 2012/09/09 iyatomi : create
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "server.h"
#include "fs.h"

namespace yue {
namespace handler {
#if defined(__ENABLE_INOTIFY__)
const char *inotify::UNDERLYING = "inotify";
inotify::event_map inotify::ms_mapping[] = {
	{ IN_ACCESS, "__access" },
	{ IN_MODIFY, "__modify" },
	{ IN_ATTRIB, "__attrib" },
	{ IN_CLOSE_WRITE, "__close_write" },
	{ IN_CLOSE_NOWRITE, "__close_norite" },
	{ IN_CLOSE, "__close" },
	{ IN_OPEN, "__open" },
	{ IN_MOVED_FROM, "__moved_from" },
	{ IN_MOVED_TO, "__moved_to" },
	{ IN_MOVE, "__move" },
	{ IN_CREATE, "__create" },
	{ IN_DELETE, "__delete" },
	{ IN_DELETE_SELF, "__delete_self" },
	{ IN_MOVE_SELF, "__move_self" },
};
#elif defined(__ENABLE_KQUEUE__)
//TODO : support kqueue
const char *inotify::UNDERLYING = "kqueue";
inotify::event_map inotify::ms_mapping[] = {
	{ NOTE_WRITE, "__modify" },
	{ NOTE_ATTRIB, "__attrib" },
	{ NOTE_DELETE, "__delete" },
	{ NOTE_RENAME, "__rename" },
	{ NOTE_LINK, "__link" },
	{ NOTE_EXTEND, "__extend" },
};
#else
#error platform cannot support fs module
#endif
int inotify::ms_mapping_size = countof(ms_mapping);
}
}
