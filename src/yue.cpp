/***************************************************************
 * yue.cpp : boot server
 * 2012/10/15 iyatomi : create
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 * see license.txt for detail
 ****************************************************************/
#include "app.h"
#include "server.h"

extern "C" {

int yue_start(int argc, char *argv[]) {
	yue::util::app a;
	return a.run<yue::server>(argc, argv);
}

}
