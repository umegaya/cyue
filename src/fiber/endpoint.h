/***************************************************************
 * endpoint.h : definition of endpoints which can initiate/resume fiber
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#if !defined(__ENDPOINT_H__)
#define __ENDPOINT_H__

#include "socket.h"

namespace yue {
class server;
class fiber;
namespace rpc {
/* fiber endpoint */
struct endpoint {
	typedef handler::socket socket;
	typedef net::wbuf wbuf;
	typedef net::address address;
	enum {
		type_nop,
		type_callback,
		type_datagram,
		type_stream,
		type_local,
	};
	struct base {
		inline void *operator new (size_t, void *p) { return p; }
		inline void fin() {}
	};
	struct nop : public base {
		template <class SR, class OBJ>
		inline int send(SR &, OBJ &) { return NBR_OK; }
		inline bool authorized() const { return true; }
	};
	struct callback : public base {
		typedef util::functional<int (void *, int)> body;
		body m_body;
		inline callback(body &cb) : m_body(cb) {}
		inline callback(int (*fn)(void *, int)) { m_body.set(fn); }
		template <class SR>
		inline int send(SR &sr, fiber &f) {
			return m_body(&f, NBR_OK);
		}
		template <class SR>
		inline int send(SR &sr, error &e) {
			return m_body(e.m_fb, e.m_errno);
		}
		inline bool authorized() const { return true; }
		inline void fin() { m_body.fin(); }
	};
	struct remote : public base {
		emittable::wrap m_wrap;
		inline remote(socket *s) : m_wrap(s) {}
		inline const socket *s() const { return m_wrap.unwrap<const socket>(); }
		inline socket *s() { return m_wrap.unwrap<socket>(); }
		inline bool valid() const { return s()->valid(); }
		inline const emittable *ns_key() const { return s()->ns_key(); }
		inline bool authorized() const { return (!s()->is_server_conn()) || s()->authorized(); }
		inline void fin() { UNREF_EMWRAP(m_wrap); }
	};
	struct datagram : public remote {
		address m_a;
		inline datagram(socket *s, net::address &a) : remote(s), m_a(a) {}
		inline const address &addr() const { return m_a; }
		template <class SR, class OBJ>
		inline int send(SR &sr, OBJ &o) {
			if (!valid()) { return NBR_EINVAL; }
			return s()->wbf().send<wbuf::template obj2<OBJ, SR, wbuf::dgram> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::dgram>::arg_dgram(o, sr, m_a), *s());
		}
	};
	struct stream : public remote {
		inline stream(socket *s) : remote(s) {}
		template <class SR, class OBJ>
		inline int send(SR &sr, OBJ &o) {
			if (!valid()) { return NBR_EINVAL; }
			return s()->wbf().send<wbuf::template obj2<OBJ, SR, wbuf::raw> >(
				typename wbuf::template obj2<OBJ, SR, wbuf::raw>::arg(o, sr), *s());
		}
	};
	struct local : public base {
		server *m_l;
		local(server *l) : m_l(l) {}
		template <class SR, class OBJ> inline int send(SR &sr, OBJ &o);
		inline bool authorized() const { return true; }
		inline server *svr() { return m_l; }
	};
	U8 m_type, padd[3];
	union {
		U8 m_nop[sizeof(nop)];
		U8 m_callback[sizeof(callback)];
		U8 m_datagram[sizeof(datagram)];
		U8 m_stream[sizeof(stream)];
		U8 m_local[sizeof(local)];
	};
	template <class T>
	endpoint(T &ep) { set(ep); }
	~endpoint() { fin(); }
	template <class SR, class OBJ>
	int send(SR &sr, OBJ &o) {
		switch(type()) {
		case type_nop:			return nop_ref().send(sr, o);
		case type_callback:		return callback_ref().send(sr, o);
		case type_datagram:		return datagram_ref().send(sr, o);
		case type_stream:		return stream_ref().send(sr, o);
		case type_local:		return local_ref().send(sr, o);
		default:				ASSERT(false); return NBR_EINVAL;
		}
	}
	bool authorized() const {
		switch(type()) {
		case type_nop:			return nop_ref().authorized();
		case type_callback:		return callback_ref().authorized();
		case type_datagram:		return datagram_ref().authorized();
		case type_stream:		return stream_ref().authorized();
		case type_local:		return local_ref().authorized();
		default:				ASSERT(false); return false;
		}
	}
	void fin() {
		switch(type()) {
		case type_nop:			return nop_ref().fin();
		case type_callback:		return callback_ref().fin();
		case type_datagram:		return datagram_ref().fin();
		case type_stream:		return stream_ref().fin();
		case type_local:		return local_ref().fin();
		default:				ASSERT(false); return;
		}
	}
public:
	int type() const { return m_type; }
	nop &nop_ref() { return *(reinterpret_cast<nop *>(m_nop)); }
	datagram &datagram_ref() { return *(reinterpret_cast<datagram *>(m_datagram)); }
	stream &stream_ref() { return *(reinterpret_cast<stream *>(m_stream)); }
	local &local_ref() { return *(reinterpret_cast<local *>(m_local)); }
	callback &callback_ref() { return *(reinterpret_cast<callback *>(m_callback)); }
	const nop &nop_ref() const { return *(reinterpret_cast<const nop *>(m_nop)); }
	const datagram &datagram_ref() const { return *(reinterpret_cast<const datagram *>(m_datagram)); }
	const stream &stream_ref() const { return *(reinterpret_cast<const stream *>(m_stream)); }
	const local &local_ref() const { return *(reinterpret_cast<const local *>(m_local)); }
	const callback &callback_ref() const { return *(reinterpret_cast<const callback *>(m_callback)); }
protected:
	void set(nop &ep) { m_type = type_nop; new (m_nop) nop(ep); }
	void set(datagram &ep) { m_type = type_datagram; new (m_datagram) datagram(ep); }
	void set(stream &ep) { m_type = type_stream; new (m_stream) stream(ep); }
	void set(local &ep) { m_type = type_local; new (m_local) local(ep); }
	void set(callback &ep) { m_type = type_callback; new (m_callback) callback(ep); }
};
}
}
#endif
