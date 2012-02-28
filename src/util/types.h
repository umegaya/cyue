/***************************************************************
 * types.h : basic types, error defs for eio
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
#if !defined(__TYPES_H__)
#define __TYPES_H__
/* error no */
enum {
	NBR_SUCCESS		= 1,
	NBR_OK			= 0,
	NBR_EMALLOC		= -1,
	NBR_EEXPIRE		= -2,
	NBR_EINVPTR		= -3,
	NBR_ERANGE		= -4,
	NBR_EALREADY	= -5,
	NBR_EINVAL		= -6,
	NBR_EINTERNAL	= -7,
	NBR_EIOCTL		= -8,
	NBR_ENOTFOUND	= -9,
	NBR_EPROTO		= -10,
	NBR_EHOSTBYNAME	= -11,
	NBR_EBIND		= -12,
	NBR_ECONNECT	= -13,
	NBR_EACCEPT		= -14,
	NBR_EFORMAT		= -15,
	NBR_ELENGTH		= -16,
	NBR_ELISTEN		= -17,
	NBR_EEPOLL		= -18,
	NBR_EPTHREAD	= -19,
	NBR_ESOCKET		= -20,
	NBR_ESHORT		= -21,
	NBR_ENOTSUPPORT = -22,
	NBR_EFORK		= -23,
	NBR_ESYSCALL	= -24,
	NBR_ETIMEOUT	= -25,
	NBR_ESOCKOPT	= -26,
	NBR_EDUP		= -27,
	NBR_ERLIMIT		= -28,
	NBR_ESIGNAL		= -29,
	NBR_ESEND		= -30,
	NBR_ECBFAIL		= -31,
	NBR_ECONFIGURE	= -32,
	NBR_EFULL		= -33,
	NBR_ERIGHT		= -34,
	NBR_EAGAIN		= -35,
	NBR_ECANCEL		= -36,
	NBR_EPENDING	= -37,
};

/* typedef */
/* size specific value */
typedef unsigned char		U8;
typedef char				S8;
typedef unsigned short		U16;
typedef short				S16;
typedef unsigned int		U32;
typedef int					S32;
typedef unsigned long long	U64;
typedef long long			S64;
typedef volatile U32		ATOMICINT;
typedef U64 				NTIME;	/* nanosec */
typedef U64					UTIME;

#endif
