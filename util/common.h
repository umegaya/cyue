/***************************************************************
 * common.h : common definition
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
#if !defined(__COMMON_H__)
#define __COMMON_H__

#include "types.h"
#include "macro.h"
#define __USE_ORIGINAL_DEF__
#include "nbr/nbr.h"
#define NBR_INLINE	static inline
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
extern "C"
{
#endif
#include "nbr/osdep.h"
#include "nbr/str.h"
#include "nbr/mem.h"
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
}
#endif

#endif
