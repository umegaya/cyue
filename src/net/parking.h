/***************************************************************
 * parking.h : transport manager
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
#if !defined(__TPMGR_H__)
#define __TPMGR_H__

#include "osdep.h"
#include "array.h"
#include "util.h"
#include "transport.h"
#include "types.h"
#include "map.h"
#if defined(__ENABLE_SENDFILE__)
#include <sys/sendfile.h>
#endif
#if defined(__NBR_OSX__)
#include <sys/uio.h>
#endif

namespace yue {
using namespace util;
namespace net {
class address;

class parking {
	array<transport> m_tl;
	map<transport*, const char *> m_tm;
	static const int MAX_TRANSPORTER_HINT = 4;
	static transport *INVALID_TRANSPORT;
public:
	parking() : m_tl(), m_tm() {}
	~parking() { fin(); }
	static inline transport *invalid() { return INVALID_TRANSPORT; }
	static inline bool stream(transport *t) { return (!t || !t->dgram); }
	int init() {
		if (!m_tl.init(MAX_TRANSPORTER_HINT, -1, opt_expandable)) {
			return NBR_EMALLOC;
		}
		return m_tm.init(MAX_TRANSPORTER_HINT, MAX_TRANSPORTER_HINT,
			-1, opt_expandable) ? NBR_OK : NBR_EMALLOC;
	}
	void fin() {
		if (m_tl.initialized()) {
			array<transport>::iterator pit = m_tl.begin();
			for (; pit != m_tl.end(); pit = m_tl.next(pit)) {
				/* if it has finalizer, call it. */
				if ((*pit).fin) { (*pit).fin((*pit).context); }
			}
			m_tl.fin();
		}
		m_tm.fin();
	}
	int add(const char *name, transport *t) {
		if (t) {
			t = m_tl.alloc(*t);
			if (!t) { return NBR_ESHORT; }
			int r = t->init ? t->init(t->context) : NBR_OK;
			if (r < 0) { return r; }
		}
		m_tm.insert(t, name);
		return m_tm.find(name) ? NBR_OK : NBR_ESHORT;
	}
	int remove(const char *name) {
		m_tm.erase(name);
		array<transport>::iterator pit = find(name);
		if (pit == m_tl.end()) { return NBR_ENOTFOUND; }
		/* if it has finalizer, call it. */
		if ((*pit).fin) { (*pit).fin((*pit).context); }
		m_tl.erase(pit);
		return NBR_OK;
	}
	array<transport>::iterator find(const char *name) {
		array<transport>::iterator pit = m_tl.begin();
		for (; pit != m_tl.end(); pit = m_tl.next(pit)) {
			if (str::cmp((*pit).name, name) == 0) { return pit; }
		}
		return m_tl.end();
	}
	transport *find_ptr(const char *name) {
		transport **pt = m_tm.find(name);
		return pt ? (*pt) : NULL;
	}
	static bool valid(transport *t) {
		return t != INVALID_TRANSPORT;
	}
	transport *divide_addr_and_transport(
		const char *addr, char *out, int len);

	transport *divide_addr_and_transport(
		const char *addr, address &a);
};

extern int tcp_str2addr(const char *str, void *addr, socklen_t *len);
extern int tcp_addr2str(void *addr, socklen_t len, char *str_addr, int str_len);
extern DSCRPTR tcp_socket(const char *addr, SKCONF *cfg);
extern int tcp_connect(DSCRPTR fd, void *addr, socklen_t len);
extern int tcp_handshake(DSCRPTR fd, int r, int w);
extern DSCRPTR tcp_accept(DSCRPTR fd, void *addr, socklen_t *len, SKCONF *cfg);
extern int tcp_close(DSCRPTR fd);

namespace syscall {
static inline int s2a(const char *str, void *a, socklen_t *al, transport *p = NULL) {
	return p && p->str2addr ? 
		p->str2addr(str, a, al) :  yue::net::tcp_str2addr(str, a, al);
}
static inline int a2s(void *a, socklen_t al, char *str, int sl, transport *p = NULL) {
	return p && p->addr2str ? 
		p->addr2str(a, al, str, sl) : yue::net::tcp_addr2str(a, al, str, sl);
}
static inline DSCRPTR socket(const char *a, void *param, transport *p = NULL) {
	return p && p->socket ? 
		p->socket(a, param) : yue::net::tcp_socket(a, (SKCONF *)param);
}
static inline int connect(DSCRPTR fd, void *a, socklen_t al, transport *p = NULL) {
	return p && p->connect ? 
		p->connect(fd, a, al) : yue::net::tcp_connect(fd, a, al);
}
static inline int handshake(DSCRPTR fd, int r, int w, transport *p = NULL) {
	return p && p->handshake ? 
		p->handshake(fd, r, w) : NBR_SUCCESS;
}
static inline int accept(DSCRPTR fd, void *a, socklen_t *al, 
	void *param, transport *p = NULL) {
	return p && p->accept ? 
	p->accept(fd, a, al, param) : yue::net::tcp_accept(fd, a, al, (SKCONF *)param);
}
static inline int close(DSCRPTR fd, transport *p = NULL) {
	//printf("%d %p %p %p\n", fd, p, p ? p->close : NULL, yue::net::tcp_close);
	return p && p->close ? 
		p->close(fd) : yue::net::tcp_close(fd);
}
static inline int read(DSCRPTR fd, void *data, size_t len, transport *p = NULL) {
	ASSERT(len > 0);
	return p && p->read ? 
		p->read(fd, data, len) : ::read(fd, data, len);
}
static inline int recvfrom(DSCRPTR fd, void *data, size_t len, void *ap, socklen_t *al, transport *p = NULL) {
	ASSERT(len > 0);
	return p && p->read ?
		p->read(fd, data, len, 0, ap, al) :
		::recvfrom(fd, data, len, 0, reinterpret_cast<struct sockaddr *>(ap), al);
}
static inline int write(DSCRPTR fd, void *data, size_t len, transport *p = NULL) {
	ASSERT(len > 0 && fd != INVALID_FD);
	return p && p->write ? 
		p->write(fd, data, len) : ::write(fd, data, len);
}
static inline int sendto(DSCRPTR fd, void *data, size_t len, const void *ap, socklen_t al, transport *p = NULL) {
	ASSERT(len > 0 && fd != INVALID_FD);
	return p && p->write ?
		p->write(fd, data, len, 0, ap, al) :
		::sendto(fd, data, len, 0, reinterpret_cast<const struct sockaddr *>(ap), al);
}
static inline int writev(DSCRPTR fd, struct iovec *iov, size_t l, transport *p = NULL) {
	ASSERT(l > 0);
	return p && p->writev ? p->writev(fd, iov, l) : ::writev(fd, iov, l);
}
static inline int sendfile(DSCRPTR in, DSCRPTR out, off_t *ofs, size_t len, 
	transport *p = NULL) {
	ASSERT(len > 0);
#if defined(__ENABLE_SENDFILE__)
	return p && p->sendfile ? 
		p->sendfile(in, out, ofs, len) : ::sendfile(in, out, ofs, len);
#else
	return p && p->sendfile ? 
		p->sendfile(in, out, ofs, len) : NBR_ENOTSUPPORT;
#endif
}
}

}
}

#endif
