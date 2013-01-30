/***************************************************************
 * osdep.h : os dependent definition of network protocol related.
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 *
 * see license.text for license detail
 ****************************************************************/
#if !defined(__OSDEP_H__)
#define __OSDEP_H__

/* macro */
#if defined(WIN32)
	//windows
	#define	__NBR_WINDOWS__
	#define NBR_TLS __declspec(thread)
	#define NBR_STLS NBR_TLS static
#elif defined(linux)
	//linux
	#define	__NBR_LINUX__
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
#elif defined(__APPLE__)
	#if !defined(__NBR_IOS__)
		//apple OSX
		#define	__NBR_OSX__
	#endif
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
#endif

/* includes */
#if defined(__NBR_WINDOWS__)
	//windows
	#include	<sys/types.h>
	#include	<sys/stat.h>
	#include	<sys/socket.h>
	#include	<windows.h>
	#include	<netinet/in.h>
	#include	<net/if.h>
	#include	<sys/ioctl.h>
	#include	<errno.h>
	#include	<unistd.h>
	#include	<netdb.h>
	#include	<arpa/inet.h>
	#include	<fcntl.h>
#elif defined(__NBR_LINUX__)
	//linux
	#include	<sys/types.h>
	#include	<sys/stat.h>
	#include	<sys/socket.h>
	#include	<sys/time.h>
	#include	<sys/epoll.h>
	#include	<netinet/in.h>
	#include	<net/if.h>
	#include	<sys/ioctl.h>
	#include	<errno.h>
	#include	<unistd.h>
	#include	<netdb.h>
	#include	<arpa/inet.h>
	#include	<fcntl.h>
	#include 	<time.h>
#elif defined(__NBR_OSX__) || defined(__NBR_IOS__)
	//apple OS
	#include	<sys/types.h>
	#include	<sys/stat.h>
	#include	<sys/socket.h>
	#include	<sys/time.h>
	#include	<sys/event.h>
	#include	<netinet/in.h>
	#include	<net/if.h>
	#include	<sys/ioctl.h>
	#include	<errno.h>
	#include	<unistd.h>
	#include	<netdb.h>
	#include	<arpa/inet.h>
	#include	<fcntl.h>
	#include 	<time.h>
#else
	#error not supported os
#endif

/* target specific defs */
#if defined(__NBR_WINDOWS__)
	//windows
	#define 	DEFAULT_IF "eth0"
#elif defined(__NBR_LINUX__)
	//linux
	#if defined(__ANDROID_NDK__)
	#define		DEFAULT_IF "lo"
	#else
	#define 	DEFAULT_IF "eth0"
	#endif
#elif defined(__NBR_OSX__) || defined(__NBR_IOS__)
	//apple OS
	#define 	DEFAULT_IF "en1"
#else
	#error not supported os
#endif

#define OSDEP_EMERG (0)
#define OSDEP_ERROR (1)
#define OSDEP_INFO	(2)
extern int osdep_init();
extern int osdep_last_error();
extern void osdep_set_last_error(int);
#define LASTERR	osdep_last_error()
#define OSDEP_ERROUT(level,code,format,...)	\
	osdep_set_last_error(NBR_E##code);	\
	printf("%s(%u):%u:%d:" format"\n", __FILE__, __LINE__, OSDEP_##level, NBR_E##code, __VA_ARGS__)

#endif
