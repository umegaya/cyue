/***************************************************************
 * fbmacro.h : macro for generating fiber handler
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
#if !defined(__FBMACRO_H__)
#define __FBMACRO_H__

/* logger */
#define FBLOG(FMT,...) 		TRACE("%-16s(%04u):"FMT, __FILE__, __LINE__, __VA_ARGS__)
#define FBLOG_S(FMT,...)	TRACE("                      :"FMT, __VA_ARGS__)
#define SEND_REQUEST(PROTO, CHK, WBP, RES)				\
	case MAKE_ID(command::request, method::PROTO): {	\
		int __r;										\
		if ((__r = send_request<rpc::PROTO>(CHK, WBP, &RES)) < 0) {	\
			return __r;									\
		}												\
	} break;

#define SEND_RESPONSE(PROTO, CHK, WBP, RES) 			\
	case MAKE_ID(command::response, method::PROTO): {	\
		int __r;										\
		if ((__r = send_response<rpc::PROTO>(CHK, WBP, &RES)) < 0) {	\
			return __r;									\
		}												\
	} break;

/* NOTE : if change packet sending system so that sending packet once
 * pass to IO thread and packed there, we need to change:
 * 1. 'forward' packet not free inside PROCESS_RESPONSE/PROCESS_ERROR macro.
 * 2. but freed inside SEND_FORWARD macro. */
#define SEND_FORWARD SEND_RESPONSE

#define SEND_ERROR(PROTO, CHK, WBP, RES) 				\
	case MAKE_ID(command::response, method::PROTO): {	\
		int __r;										\
		if ((__r = send_error<rpc::PROTO>(CHK, WBP, &RES)) < 0) {	\
			return __r;									\
		}												\
	} break;


#define PROCESS_REQUEST(CH, DATA, PROTO, CHECK, RET)	\
	case method::PROTO: {								\
		process_request<rpc::PROTO>(CH, DATA, CHECK, RET); \
		if (RET < 0) { return RET; }					\
	} break;

#define ROUTING_REQUEST(CH, DATA, PROTO, CHECK, RET, BLOCK)	\
	case method::PROTO: {								\
		BLOCK __b;										\
		routing_request<rpc::PROTO, BLOCK>(CH, DATA, CHECK, __b, RET); \
		if (RET < 0) { return RET; }					\
	} break;

#define PROCESS_RESPONSE(FB, DATA, PROTO, RET) 			\
	case method::PROTO: {								\
		process_response<rpc::PROTO>(FB, DATA, RET);	\
		if (RET < 0) { return RET; }					\
	} break;


#define PROCESS_ERROR(FB, DATA, PROTO, RET)				\
	case method::PROTO: {								\
		process_error<rpc::PROTO>(FB, DATA, RET);		\
		if (RET < 0) { return RET; }					\
	} break;


#define CONVERT_REQUEST(PROTO, RET, SR, CHK)			\
	case MAKE_ID(command::request, method::PROTO): {	\
		convert_request<rpc::PROTO>(CHK, SR, RET);		\
		if (RET < 0) { return RET; }					\
	} break;

#define CONVERT_RESPONSE(PROTO, RET, SR, CHK)			\
	case MAKE_ID(command::response, method::PROTO): {	\
		convert_response<rpc::PROTO>(CHK, SR, RET);		\
		if (RET < 0) { return RET; }					\
	} break;

#define CONVERT_FORWARD CONVERT_RESPONSE

#define CONVERT_ERROR(PROTO, RET, SR, CHK)				\
	case MAKE_ID(command::response, method::PROTO): {	\
		convert_error<rpc::PROTO>(CHK, SR, RET);		\
		if (RET < 0) { return RET; }					\
	} break;

/* used inside kvs::session::rescue(basic_fiber*, packet &) */
#define RESCUE(FB, DATA, PROTO, RET) case method::PROTO: {	\
		fiber<rpc::PROTO> *f = reinterpret_cast<fiber<rpc::PROTO> *>(FB);	\
		RET = f->resume(DATA);								\
		/* DATA will be freed inside PROCESS_ERROR. */		\
	} break;

#define DESTROY_FIBER(PROTO,FB) case method::PROTO: {		\
	fiber<rpc::PROTO> *f = reinterpret_cast<fiber<rpc::PROTO> *>(FB);	\
	delete f;												\
} break;

#endif
