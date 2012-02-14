/***************************************************************
 * session.cpp : abstruction of message stream
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "session.h"
#include "server.h"

namespace yue {
namespace handler {

/* session.h */
util::handshake session::m_hs;
/* monitor.h */
util::array<monitor::watch_entry> monitor::m_wl;
monitor::watch_entry *monitor::m_gtop = NULL;
thread::mutex monitor::m_gmtx;
array<session::session_event_message> session::m_msgpool;


base::result session::read(loop &l) {
	switch(m_type) {
	case STREAM:	return read_stream(l);
	case DGRAM:		return read_dgram(l);
	case RAW:		return read_raw(l);
	default:		ASSERT(false); return destroy;
	}
}
void session::close() {
	if (valid()) {
		task::io t(m_fd);
		loop::tls()->que().mpush(t);
	}
}
int session::connect(
	address &to, transport * t, handshake::handler &ch,
	double timeout, object *opt, bool raw) {
	SKCONF skc = { 120, 65536, 65536, NULL };
	if (opt) {
		skc.rblen = (*opt)("rblen",65536);
		skc.wblen = (*opt)("wblen",65536);
		skc.timeout = (*opt)("timeout",120);
		skc.proto_p = opt;
	}
	DSCRPTR fd = net::syscall::socket(NULL, &skc, t);
	if (fd < 0) { goto end; }
	if (net::syscall::connect(fd, to.addr_p(), to.len(), t) < 0) {
		goto end;
	}
	if (m_hs.start_handshake(fd, ch, timeout) < 0) {
		goto end;
	}
	ASSERT(((int)fd) >= 0 && (sizeof(DSCRPTR) == sizeof(int)));
	TRACE("session : connect success\n");
	return ((int)fd);
end:
	if (fd >= 0) { net::syscall::close(fd, t); }
	return NBR_ESYSCALL;
}

int session::sync_write(loop &l, int timeout) {
	int r; poller::event ev;
	do {
		if ((r = loop::sync().wait_event(
			m_fd, poller::EV_WRITE, timeout, ev)) < 0) {
			return r;
		}
		if ((r = wbf().write(m_fd, loop::tl()[m_fd])) == destroy) {
			return NBR_ESEND;
		}
	} while (r != nop);
	return NBR_OK;
}
int session::sync_read(loop &l, object &o, int timeout) {
	int r, parse_result;
	poller::event ev;
	do {
		do {
			if ((r = loop::sync().wait_event(
				m_fd, poller::EV_READ, timeout, ev)) < 0) {
				return r;
			}
		} while ((r = read_and_parse(l, parse_result)) == again);
		if (parse_result == serializer::UNPACK_SUCCESS) { break; }
		else if (parse_result == serializer::UNPACK_CONTINUE) { continue; }
		else {
			/* maybe broken packet. read remain packet and throw away.
			 * you can try sync_write/read after that. */
			char buffer[1024]; int cnt = 0;
			while (net::syscall::read(m_fd, buffer, sizeof(buffer), m_t) > 0) {
				cnt++; if (cnt > 100) { ASSERT(false); break; }
			}
			ASSERT(r == destroy);
			return NBR_EINVAL;
		}
	} while (true);
	o = m_sr.result();
	return NBR_OK;
}
int session::sync_connect(loop &l, int timeout) {
	poller::event ev;
	UTIME now = util::time::now();
	/* init wbuf for write */
	if (wbf().init() < 0) { return NBR_EMALLOC; }
	/* open connection (m_fd finally initialized by this value) */
	DSCRPTR fd = connect();
	if (fd < 0) { ASSERT(false); return NBR_ESYSCALL; }
	/* wait established */
	while (loop::sync().wait_event(fd,
		poller::EV_WRITE | poller::EV_READ, timeout, ev) >= 0) {
		TRACE("poller readable %d %s %s\n", fd,
				poller::readable(ev) ? "r" : "nr",
				poller::writable(ev) ? "w" : "nw");
		on_read(l, ev);
		TRACE("after read: %d %s\n", fd, valid() ? "valid" : "invalid");
		if (valid()) {
			ASSERT(fd == m_fd);
			return NBR_OK;
		}
		if ((now + timeout) < util::time::now()) {
			return NBR_ETIMEOUT;
		}
	}
	ASSERT(false);
	return NBR_ESYSCALL;
}
}
}

extern "C" {
int socket_read(void *s, char *p, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->read(p, sz);
}
int socket_write(void *s, char *p, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->write(p, sz);
}
int socket_writev(void *s, struct iovec *iov, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->writev(iov, sz);
}
int socket_writef(void *s, DSCRPTR fd, size_t ofs, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->writef(fd, ofs, sz);
}
int socket_sys_write(void *s, char *p, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->sys_write(p, sz);
}
int socket_sys_writev(void *s, struct iovec *iov, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->sys_writev(iov, sz);
}
int socket_sys_writef(void *s, DSCRPTR fd, off_t *ofs, size_t sz) {
	return (reinterpret_cast<yue::handler::session *>(s))->sys_writef(fd, ofs, sz);
}
}



