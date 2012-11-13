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
	typedef util::pbuf pbuf;
	static util::msgid_generator<U32> m_gen;
	pbuf *m_pbuf;
public:
	typedef util::msgid_generator<U32>::MSGID MSGID;
	static const MSGID INVALID_MSGID = util::msgid_generator<U32>::INVALID_MSGID;
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
		argument &elem(int n) 				{ return (argument &)super::data::elem(n); }
		const argument &elem(int n) const 	{ return (const argument &)super::data::elem(n); }
		argument &key(int n) 				{ return (argument &)super::data::key(n); }
		const argument &key(int n) const 	{ return (const argument &)super::data::key(n); }
		argument &val(int n) 				{ return (argument &)super::data::val(n); }
		const argument &val(int n) const 	{ return (const argument &)super::data::val(n); }
	};
	struct object : public super::object {
		inline U8 type() const { return (U8)(U32)elem(0); }
		inline MSGID msgid() const { return (MSGID)elem(1); }
		inline const argument &cmd() const {
			ASSERT(is_request()); return reinterpret_cast<const argument &>(elem(2));
		}
		inline argument &method() { return arg(0); }
		inline const argument &method() const { return arg(0); }
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
		inline super::data &operator [] (const char *k) {
			super::data *ptr = NULL;
			for (size_t i = 0; i < super::object::size(); i++) {
				if (util::str::length(k) != super::object::key(i).len()) {
					continue;
				}
				if (util::mem::cmp(k, super::object::key(i), super::object::key(i).len()) == 0) {
					return super::object::val(i);
				}
			}
			return *ptr;
		}
		static inline bool valid(super::data &d) {
			return ((&d) != NULL);
		}
		template <class DEFAULT> inline DEFAULT operator () (const char *k, DEFAULT def) {
			super::data &d = this->operator [] (k);
			return valid(d) ? ((DEFAULT)d) : def;
		}
		inline bool operator () (const char *k, bool def) {
			super::data &d = this->operator [] (k);
			return valid(d) ? d.operator bool () : def;
		}
	};
	static inline MSGID invalid_id() { return INVALID_MSGID; }
	static inline MSGID new_id() { return m_gen.new_id(); }
	inline object &result() { return reinterpret_cast<object &>(super::result()); }
	inline pbuf *attached() { return m_pbuf; }
	inline void start_pack(pbuf &pbf) {
		super::start_pack(pbf);
	}
	inline int expand_buffer(size_t s) {
		return super::pack_buffer_reserve(s);
	}
	template <class O> inline int pack(const O &o, pbuf *pbf) {
		super::start_pack(*pbf);
		return o.pack(*this);
	}
	template <class DATA>
	static inline int pack_as_object(DATA &d, serializer &sr) {
		int r; pbuf pbf;
		if ((r = pbf.reserve(sizeof(DATA) * 2)) < 0) { return r; }
		sr.start_pack(pbf);
		if ((r = d.pack(sr)) < 0) { return r; }
		//pbf.commit(r);
		r = sr.unpack(sr.pack_buffer());
		ASSERT(r == serializer::UNPACK_SUCCESS);
		return r == serializer::UNPACK_SUCCESS ? NBR_OK : NBR_EINVAL;
	}
	template <class P> inline int pack_request(MSGID &msgid, const P &p) {
		verify_success(push_array_len(4));
		verify_success(super::operator << (rpc::command::request));
		verify_success(super::operator << ((msgid = m_gen.new_id())));
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
template <> inline int serializer::object::operator () <int> (const char *k, int def) {
	super::data &d = this->operator [] (k);
	if (valid(d)) {
		if (d.kind() == serializer::DOUBLE) {
			return (int)((double)d);
		}
		return (int)d;
	}
	return def;
}
typedef serializer::MSGID MSGID;
static const MSGID INVALID_MSGID = serializer::INVALID_MSGID;
typedef serializer::object object;
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
