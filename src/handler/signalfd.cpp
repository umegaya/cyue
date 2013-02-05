/***************************************************************
 * signalfd.cpp : handling signal
 * 			(thus it does reconnection, re-send... when connection closes)
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 * see license.txt about license detail
 **************************************************************/
#include "signalfd.h"

namespace yue {
namespace handler {
DSCRPTR signalfd::m_pair[2];
signalfd::handler m_hmap[signalfd::SIGMAX];
signalfd::handler *signalfd::hmap() {
    return m_hmap;
}
}
}

