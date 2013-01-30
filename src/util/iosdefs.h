/***************************************************************
 * iosdefs.h : ios build options (__ENABLE_KQUEUE__, ... )
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 *	see license.txt for license detail
 ****************************************************************/
#if !defined(__IOSDEFS_H__)
#define __IOSDEFS_H__

#define __ENABLE_KQUEUE__
#define __NBR_BYTE_ORDER__ __NBR_LITTLE_ENDIAN__
#define __CPU_ARCH__ NBR_CPU_ARCH_armeabi 

#define _SERIALIZER mpak
#define _LL lua

#endif
