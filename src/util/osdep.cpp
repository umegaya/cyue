/***************************************************************
 * osdep.h : os dependent definition of network protocol related.
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 *
 * see license.text for license detail
 ****************************************************************/
#include "osdep.h"
#include "types.h"
#include "util.h"
#include <pthread.h>

struct error_info {
	int last_error;
};

static pthread_key_t g_last_error_tls_key;
static volatile int g_init_flag = 0;

void osdep_fin(void *p) {
	delete reinterpret_cast<error_info *>(p);
}
int osdep_init() {
	if (__sync_bool_compare_and_swap(&g_init_flag, 0, 1)) {
		int rc = pthread_key_create(&g_last_error_tls_key, osdep_fin);
		if (rc != 0) {
			g_init_flag = -1;
			return NBR_EPTHREAD;
		}
		g_init_flag = 2;
	}
	else {
		while (g_init_flag == 1) {
			yue::util::time::sleep(1 * 1000 * 1000);
		}
		if (g_init_flag < 0) { return NBR_EPTHREAD; }
	}
	error_info *p = new error_info;
	if (!p) { return NBR_EMALLOC; }
	pthread_setspecific(g_last_error_tls_key, p);
	return NBR_OK;
}
int osdep_last_error() {
	error_info *p = reinterpret_cast<error_info *>(pthread_getspecific(g_last_error_tls_key));
	return p->last_error;
}
void osdep_set_last_error(int e) {
	error_info *p = reinterpret_cast<error_info *>(pthread_getspecific(g_last_error_tls_key));
	p->last_error = e;
}
