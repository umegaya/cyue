/***************************************************************
 * constant.cpp : constant definitions
 * 2013/02/06 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * see license.txt for license details.
 ****************************************************************/
#include "constant.h"
namespace yue {
namespace constant {
#if defined(DEFINE_ERROR)
#undef DEFINE_ERROR
#endif
#define DEFINE_ERROR(__error, __code) error::entry error::__error = { __code, #__error }
#include "rpcerrors.inc"
#undef DEFINE_ERROR
}
}
