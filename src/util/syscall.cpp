/***************************************************************
 * syscall.h : thin wrapper of system call functions.
 *  except event IO api (kqueue, epoll, ...) and sock APIs
 *                             Copyright (C) 2011-2012 Takehiro Iyatomi
 *
 *  see license.txt for license detail
 ****************************************************************/
#include "syscall.h"
#include "osdep.h"
#include "util.h"

#if defined(__NBR_OSX__)
extern "C" {
#include "osxmac.hpp"
}
#endif

namespace yue {
namespace util {
namespace syscall {

int getpid()
{
	return getpid();
}

/* process operation */
#define NULLDEV ("/dev/null")
int daemonize()
{
	/* be a daemon process */
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	fflush(stdout);
	fflush(stderr);
	switch(::fork()){
	case -1: return NBR_EFORK;
	case 0: break;
	default: _exit(0);
	}
	if(setsid() == -1) { return NBR_ESYSCALL; }
	switch(::fork()){
	case -1: return NBR_EFORK;
	case 0: break;
	default: _exit(0);
	}
	umask(0);
	if(chdir(".") == -1) return NBR_ESYSCALL;
	close(0);
	close(1);
	close(2);
	int fd = open(NULLDEV, O_RDWR, 0);
	if(fd != -1){
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if(fd > 2) close(fd);
	}
	return NBR_OK;
#else
	/* now only linux deamonize */
	return NBR_ENOTSUPPORT;
#endif
}

int forkexec(char *cmd, char *argv[], char *envp[])
{
#if defined(__NBR_LINUX__) || defined(__NBR_OSX__)
	int r;
	fflush(stdout);
	fflush(stderr);
	switch((r = fork())){
	case -1: return NBR_EFORK;
	case 0: break;
	default: return r;	/* this flow is for parent process
	(call fork) */
	}
	/* this flow is for child process (change to cmd) */
	if (-1 == execve(cmd, argv, envp)) {
		return NBR_ESYSCALL;
	}
	/* never reach here (cause execve never returns) */
	return 0;
#else
	/* now only linux fork */
	return NBR_ENOTSUPPORT;
#endif
}
/* common socket related */
int		get_sock_addr(DSCRPTR fd, char *addr, socklen_t *len)
{
	if (*len < sizeof(struct sockaddr)) {
		return NBR_ESHORT;
	}
	if (getsockname(fd, (struct sockaddr *)addr, len) != 0) {
		OSDEP_ERROUT(ERROR,SYSCALL,"getsockname : %d %d\n", errno, fd);
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

/* ipv4 only */
static int	_get_ifaddr(DSCRPTR fd, const char *ifn, char *buf, int *len,
						void *addr, int alen)
{
	struct ifreq ifr;
	struct sockaddr_in *saif, *sa;
	socklen_t slen = alen;
	if (slen < sizeof(struct sockaddr_in)) {
		return NBR_ESHORT;
	}
	sa = (struct sockaddr_in *)addr;
	util::mem::bzero(&ifr, sizeof(ifr));
	ifr.ifr_addr.sa_family = AF_INET;
	util::str::copy(ifr.ifr_name, ifn, IFNAMSIZ-1);
	if (-1 == ioctl(fd, SIOCGIFADDR, &ifr)) {
		OSDEP_ERROUT(ERROR,SYSCALL,"ioctl fail %u\n", errno);
		return NBR_ESYSCALL;
	}
	saif = (struct sockaddr_in *)&(ifr.ifr_addr);
	if (sa) {
		*len = util::str::printf(buf, *len, "%s:%hu",
				inet_ntoa(saif->sin_addr), ntohs(sa->sin_port));
	}
	else {
		*len = util::str::printf(buf, *len, "%s", inet_ntoa(saif->sin_addr));
	}
	return *len;
}

int get_if_addr(DSCRPTR fd, const char *ifn, char *addr, int alen) {
	int r = _get_ifaddr(fd, ifn, addr, &alen, NULL, 0);
	return r < 0 ? r : alen;
}

int get_macaddr(const char *ifname, U8 *addr)
{
#if defined(__NBR_OSX__)
    kern_return_t	kernResult = KERN_SUCCESS;
    io_iterator_t	intfIterator;
    UInt8			MACAddress[kIOEthernetAddressSize];

    kernResult = FindEthernetInterfaces(&intfIterator);

    if (KERN_SUCCESS != kernResult) {
        //OSDEP_ERROUT(ERROR,SYSCALL,"FindEthernetInterfaces returned 0x%08x\n", kernResult);
        return NBR_ESYSCALL;
    }
    else {
        kernResult = GetMACAddress(intfIterator, addr, sizeof(MACAddress));

        if (KERN_SUCCESS != kernResult) {
            //OSDEP_ERROUT(ERROR,SYSCALL,"GetMACAddress returned 0x%08x\n", kernResult);
        }
		else {
			TRACE("This system's built-in MAC address is %02x:%02x:%02x:%02x:%02x:%02x.\n",
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		}
    }

    (void) IOObjectRelease(intfIterator);	// Release the iterator.

    return kernResult == KERN_SUCCESS ? NBR_OK : NBR_ESYSCALL;
#else
	int				soc, ret;
	struct ifreq	req;

	if ((soc = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		//OSDEP_ERROUT(INFO,INTERNAL,"socket fail: ret=%d,errno=%d",soc,errno);
		goto error;
	}

	util::str::copy(req.ifr_name, ifname, sizeof(req.ifr_name));
	req.ifr_addr.sa_family = AF_INET;

	if ((ret = ioctl(soc, SIOCGIFHWADDR, &req)) < 0) {
		//OSDEP_ERROUT(INFO,IOCTL,"ioctl fail: ret=%d,errno=%d",ret,errno);
		goto error;
	}

	util::mem::copy(addr, &(req.ifr_addr.sa_data), 6);
	ret = 0;
//	TRACE("MAC ADDRESS (%s): [%02X:%02X:%02X:%02X:%02X:%02X]\n", ifname,
//		*addr, *(addr+1), *(addr+2), *(addr+3), *(addr+4), *(addr+5));
error:
	if (soc >= 0) {
		close(soc);
	}
	return ret;
#endif
}

}
}
}

