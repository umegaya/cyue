/***************************************************************
 * serializer.h : data de/serializer (JSON, XML, msgpack, thrift..)
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
#if !defined(__SERIALIZER_H__)
#define __SERIALIZER_H__

#include "impl.h"
#include "uuid.h"

namespace yue {
namespace rpc {
namespace command {
/* rpc kind */
static const U8 request = 0;
static const U8 response = 1;
static const U8 notify = 2;
}
}
/* serializer class */
class serializer : public module::serializer::_SERIALIZER {
	static msgid_generator<U32> m_gen;
public:
	typedef msgid_generator<U32>::MSGID MSGID;
	static const MSGID INVALID_MSGID = msgid_generator<U32>::INVALID_MSGID;
	typedef module::serializer::_SERIALIZER super;
	enum {
		UNPACK_SUCCESS		=  super::UNPACK_SUCCESS,
		UNPACK_EXTRA_BYTES	=  super::UNPACK_EXTRA_BYTES,
		UNPACK_CONTINUE		=  super::UNPACK_CONTINUE,
		UNPACK_PARSE_ERROR	=  super::UNPACK_PARSE_ERROR,
	};
	struct argument : public super::data {
		operator const UUID &() const {
			ASSERT(super::data::len() == sizeof(UUID));
			return *(reinterpret_cast<const UUID*>(super::data::operator const void *()));
		}
		operator UUID &() {
			ASSERT(super::data::len() == sizeof(UUID));
			return *(reinterpret_cast<UUID*>(super::data::operator void *()));
		}
/*		operator const address &() const {
			ASSERT(super::data::len() == sizeof(address));
			return *(reinterpret_cast<const address*>(super::data::operator const void *()));
		}
		operator address &() {
			ASSERT(super::data::len() == sizeof(address));
			return *(reinterpret_cast<address*>(super::data::operator void *()));
		} */
		argument &elem(int n) 				{ return (argument &)super::data::elem(n); }
		const argument &elem(int n) const 	{ return (const argument &)super::data::elem(n); }
		argument &key(int n) 				{ return (argument &)super::data::key(n); }
		const argument &key(int n) const 	{ return (const argument &)super::data::key(n); }
		argument &val(int n) 				{ return (argument &)super::data::val(n); }
		const argument &val(int n) const 	{ return (const argument &)super::data::elem(n); }
	};
	/* thread transparency */
	struct lobject {
		sbuf *m_sbf;
		MSGID m_msgid;
		U8 m_type, m_cmd, padd[2];
		U8 m_p[0];
	public:
		lobject(sbuf *sbf, U8 type, U8 cmd) : m_sbf(sbf),
			m_msgid(serializer::m_gen.new_id()),
			m_type(type), m_cmd(cmd) {}
		lobject(sbuf *sbf, U8 type, MSGID msgid) : m_sbf(sbf),
			m_msgid(msgid), m_type(type), m_cmd(0) {}
		~lobject() {}
		inline void *operator new (size_t sz, sbuf *sbf) {
			return sbf->malloc(sz);
		}
		template <class T> operator T* () {
			return reinterpret_cast<T *>(m_p);
		}
		inline void operator delete (void *p) {}
		inline void fin() { if (m_sbf) { delete m_sbf; m_sbf = NULL; } }
		inline U8 type() const { return m_type; }
		inline MSGID msgid() const { return m_msgid; }
		inline U8 cmd() const {
			ASSERT(type() == rpc::command::request);
			return m_cmd;
		}
	};
	/* network transparency */
	struct object : public super::object {
		inline U8 type() const { return (U8)(U32)elem(0); }
		inline MSGID msgid() const { return (MSGID)elem(1); }
		inline U8 cmd() const {
			ASSERT(is_request()); return (U8)(U32)elem(2);
		}
		inline argument &arg(int idx) {
			ASSERT(is_request() || ((!is_error()) && is_response()));
			return reinterpret_cast<argument &>(elem(3).elem(idx));
		}
		inline const argument &arg(int idx) const {
			ASSERT(is_request() || ((!is_error()) && is_response()));
			return reinterpret_cast<const argument &>(elem(3).elem(idx));
		}
		inline int alen() const {
			ASSERT(is_request() || ((!is_error()) && is_response()));
			return elem(3).size();
		}
		inline const argument &resp() const {
			ASSERT(((!is_error()) && is_response()));
			return reinterpret_cast<const argument &>(elem(3));
		}
		inline const argument &error() const {
			ASSERT(is_response());
			return reinterpret_cast<const argument &>(elem(2));
		}
		inline void inherit(super::object &d) { super::object::operator = (d); }
		inline bool is_request() const { return type() == rpc::command::request; }
		inline bool is_response() const { return type() == rpc::command::response; }
		inline bool is_notify() const { return type() == rpc::command::notify; }
		inline bool is_error() const { return is_response() && error().kind() != super::NIL; }
	};
	static inline MSGID new_id() { return m_gen.new_id(); }
	inline object &result() { return reinterpret_cast<object &>(super::result()); }
	template <class O> inline int pack(const O &o, char *p, int l) {
		super::start_pack(p, l);
		return o.pack(*this);
	}
	template <class P> inline int pack_request(MSGID &msgid, U8 cmd, const P &p) {
		verify_success(push_array_len(4));
		verify_success(super::operator << (rpc::command::request));
		verify_success(super::operator << ((msgid = m_gen.new_id())));
		verify_success(super::operator << (cmd));
		verify_success(p(*this, msgid));
		return len();
	}
	template <typename E> inline int pack_error(MSGID msgid, E e) {
		verify_success(push_array_len(4));
		verify_success(super::operator << (rpc::command::response));
		verify_success(super::operator << (msgid));
		verify_success(e(*this));
		verify_success(pushnil());
		return len();
	}
	template <class P> inline int pack_response(const P &p, MSGID msgid) {
		verify_success(push_array_len(4));
		verify_success(super::operator << (rpc::command::response));
		verify_success(super::operator << (msgid));
		verify_success(pushnil());
		verify_success(p(*this));
		return len();
	}
	template <class T> inline int operator << (T &data) {
		verify_success(super::operator << (data));
		return len();
	}
	inline int operator << (const UUID &uuid) {
		return push_raw(reinterpret_cast<const char *>(&uuid), sizeof(uuid));
	}
	inline int pushnil() { return super::pushnil(); }
	inline int operator << (bool f) { return super::operator << (f); }
	inline int operator << (U8 u) { return super::operator << (u); }
	inline int operator << (S8 s) { return super::operator << (s); }
	inline int operator << (U16 u) { return super::operator << (u); }
	inline int operator << (S16 s) { return super::operator << (s); }
	inline int operator << (U32 u) { return super::operator << (u); }
	inline int operator << (S32 s) { return super::operator << (s); }
	inline int operator << (U64 u) { return super::operator << (u); }
	inline int operator << (S64 s) { return super::operator << (s); }
	inline int operator << (float f) { return super::operator << (f); }
	inline int operator << (double f) { return super::operator << (f); }
	inline int operator << (const super::data &d) { return super::operator << (d); }
	inline int operator << (const char *s) { return super::operator << (s); }
	inline int push_raw_len(size_t l) { return super::push_raw_len(l); }
	inline int push_raw_onlydata(const char *p, size_t l) { return super::push_raw_onlydata(p, l); }
	inline int push_raw(const char *p, size_t l) { return super::push_raw(p, l); }
	inline int push_string(const char *str, int l = -1) { return super::push_string(str, l); }
	inline int push_array_len(size_t l) { return super::push_array_len(l); }
	inline int push_array(const data *a, size_t l) { return super::push_array(a, l); }
	inline int push_map_len(size_t l) { return super::push_map_len(l); }
	inline int push_map(const data *a, size_t l) { return super::push_map(a, l); }

};
typedef serializer::MSGID MSGID;
static const MSGID INVALID_MSGID = serializer::INVALID_MSGID;
typedef serializer::object object;
typedef serializer::lobject lobject;
typedef serializer::object_struct object_struct;	/* can be used from C code */
typedef serializer::super::data data;
typedef serializer::argument argument;
namespace rpc {
namespace datatype {
static const int NIL = serializer::NIL;
static const int BOOLEAN = serializer::BOOLEAN;
static const int ARRAY = serializer::ARRAY;
static const int MAP = serializer::MAP;
static const int BLOB = serializer::BLOB;
static const int DOUBLE = serializer::DOUBLE;
static const int INTEGER = serializer::INTEGER;
static const int STRING = serializer::STRING;
}
}

}

#endif
