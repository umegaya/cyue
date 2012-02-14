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
	//apple OS
	#define	__NBR_OSX__
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
#endif

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
#elif defined(__NBR_OSX__)
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

#define OSDEP_EMERG (0)
#define OSDEP_ERROR (1)
#define OSDEP_INFO	(2)
extern int osdep_last_error();
extern void osdep_set_last_error(int);
#define LASTERR	osdep_last_error()
#define OSDEP_ERROUT(level,code,format,...)	\
	osdep_set_last_error(NBR_E##code);	\
	TRACE("%s(%u):%u:%d:"format"\n", __FILE__, __LINE__, OSDEP_##level, NBR_E##code, __VA_ARGS__)

#endif
