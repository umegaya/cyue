/****************************************************************
 * nbr.h : common initialize/finalize/polling routines
 * 2009/08/25 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#if !defined(__NBR_H__)
#define __NBR_H__


/* include */
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>


/* macro */
#if defined(WIN32)
	//windows
	#define	__NBR_WINDOWS__
	#define NBR_API	__declspec(dllexport)
	#define NBR_TLS __declspec(thread)
	#define NBR_STLS NBR_TLS static
#elif defined(linux)
	//linux
	#define	__NBR_LINUX__
	#define NBR_API extern
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
#elif defined(__APPLE__)
	//mac osx
	#define	__NBR_OSX__
	#define NBR_API extern
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
#else
	#define NBR_API extern
	#define NBR_TLS __thread
	#define NBR_STLS static NBR_TLS
	#error not supported os
#endif
#define MTSAFE


#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
extern "C"
{
#endif

#if !defined(__USE_ORIGINAL_DEF__)
/* error */
enum {
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
};
#endif

enum {
	CLOSED_BY_INVALID		= 0,
	CLOSED_BY_REMOTE		= 1,
	CLOSED_BY_APPLICATION	= 2,
	CLOSED_BY_ERROR			= 3,
	CLOSED_BY_TIMEOUT		= 4,
};

/* option for primitive object (ARRAY/SEARCH/SOCKMGR) */
enum {
	NBR_PRIM_THREADSAFE		= 1 << 0,	/* thread safe (use rwlock)*/
	NBR_PRIM_EXPANDABLE		= 1 << 1,	/* expand limit size when expired*/
};


/* typedef */
/* size specific value */
#if !defined(__USE_ORIGINAL_DEF__)
typedef unsigned char		U8;
typedef char				S8;
typedef unsigned short		U16;
typedef short				S16;
typedef unsigned int		U32;
typedef int					S32;
typedef unsigned long long	U64;
typedef long long			S64;
typedef volatile U32		ATOMICINT;
#endif
/* object handle */
typedef void				*ARRAY;
typedef void				*SEARCH;
typedef void				*TASKMGR;
typedef void				*TASK;
typedef void				*SOCKMGR;
typedef void				*THREAD;
typedef void				*THPOOL;
typedef void				*MUTEX;
typedef void				*RWLOCK;
typedef void				*SPINLK;
typedef void				*CONHASH;
typedef void				*NODE;
typedef U32					THRID;
/* system type */
typedef U64					UTIME;
/* system struct */
typedef struct  node_s 
{
	char iden[28]; /* node name or some thing identifies the node */
	U16 replicas; /* number of replica virtual nodes */
	U16 flag;
}					CHNODE;
typedef struct	nbr_sock_t
{
	int		s;
	void	*p;
}							SOCK, VSCK;
typedef struct 	nbr_sock_worker_addr_t
{
	int 	type;
	void	*p;
}							SWKFROM;
typedef struct	nbr_nioconf
{
	int epoll_timeout_ms;
	int job_idle_sleep_us;
}							NIOCONF;
typedef struct 	nbr_nodeconf
{
	int	max_node, multiplex;
	int mcast_port;
}							NODECONF;
/* this config is given to proto modules */
typedef struct 	nbr_sockconf
{
	int timeout;
	int rblen, wblen;
	void *proto_p;
}							SKCONF;
/* these proto conf for proto_p parameter for
 * nbr_sockmgr_create / nbr_sockmgr_connect */
typedef struct 	nbr_udpconf
{
	char *mcast_addr;
	int	ttl;
}							UDPCONF;
typedef struct 	nbr_sockmgr_stat_t
{
	int n_connection;
	int total_send, total_recv;
	int send_sec, recv_sec;
}							SKMSTAT;
typedef struct	nbr_init_t
{
	int	max_array;
	int	max_search;
	int max_task;
	int	max_proto;
	int	max_sockmgr;
	int max_nfd;
	int max_thread;
	int max_worker;
	int sockbuf_size;
	int sockbuf_size_main;
	NIOCONF 	ioc;
	NODECONF	ndc;
}							CONFIG;
typedef int					DSCRPTR;
typedef int	(*RECVFUNC) (DSCRPTR, void*, size_t, ...);
typedef int	(*SENDFUNC)	(DSCRPTR, const void*, size_t, ...);
typedef long	(*STRHASHFUNC)	(const char *, size_t);
typedef void (*SIGFUNC)	(int);
typedef struct	nbr_proto_t
{
	char	*name;
	void	*context;
	int		dgram;		/* bool. is dgram protocol? */

	int		(*init)		(void *);
	int		(*fin)		(void *);
	int		(*fd)		(DSCRPTR);
	int		(*str2addr)	(const char*, void*, socklen_t*);
	int		(*addr2str)	(void*, socklen_t, char*, int);
	DSCRPTR	(*socket)	(const char*, SKCONF*);
	int		(*connect)	(DSCRPTR, void*, socklen_t);
	int		(*handshake)(DSCRPTR, int, int);
	DSCRPTR	(*accept)	(DSCRPTR, void*, socklen_t*, SKCONF*);
	int		(*close)	(DSCRPTR);
	int		(*recv) 	(DSCRPTR, void*, size_t, int, ...);
	int		(*send)		(DSCRPTR, const void*, size_t, ...);
}							PROTOCOL;
/* event for node data */
typedef enum {
	NDEV_INVALID,
	NDEV_GET_DATA,	/* system request for packing node data
					 to send it to another node */
	NDEV_PUT_DATA,	/* system request for unpacking node data
					 to update/create local node data */
	NDEV_DELETE,	/* node is delete (by linkdead or something) */
} NDEVENT;



/* nbr.c */
NBR_API int		nbr_init(CONFIG *c);
NBR_API void 	nbr_get_default(CONFIG *c);
NBR_API void	nbr_fin();
NBR_API void	nbr_stop_sock_io();
NBR_API void	nbr_poll();


/* err.c */
NBR_API int		nbr_err_get();
NBR_API void	nbr_err_out_stack();
NBR_API void	nbr_err_set_fp(int lv, FILE *fp);


/* osdep.c */
NBR_API int		nbr_osdep_get_macaddr(char *ifname, U8 *addr);
NBR_API int		nbr_osdep_daemonize();
NBR_API int		nbr_osdep_getpid();
NBR_API int		nbr_osdep_fork(char *cmd, char *argv[], char *envp[]);
NBR_API int		nbr_osdep_sleep(U64 nanosec);
NBR_API UTIME	nbr_clock();
NBR_API UTIME	nbr_time();


/* mem.c */
static inline void *nbr_malloc(size_t s) { return malloc(s); }
static inline void nbr_free(void *p) { free(p); }
static inline void *nbr_realloc(void *p, size_t ns) { return realloc(p, ns); }


/* rand.c */
NBR_API U32		nbr_rand32();
NBR_API	U64		nbr_rand64();


/* proto.c */
NBR_API PROTOCOL	*nbr_proto_regist(PROTOCOL proto);
NBR_API int			nbr_proto_unregist(PROTOCOL *proto_p);
NBR_API PROTOCOL	*nbr_proto_tcp();
NBR_API PROTOCOL	*nbr_proto_udp();
NBR_API PROTOCOL	*nbr_proto_from_name(const char *name);


/* str.c */
NBR_API int			nbr_str_atobn(const char* str, S64 *i, int max);
NBR_API int			nbr_str_htobn(const char* str, S64 *i, int max);
NBR_API int			nbr_str_atoi(const char* str, int *i, int max);
NBR_API int			nbr_str_htoi(const char* str, int *i, int max);
NBR_API int			nbr_str_cmp_nocase(const char *a, const char *b, int len);
NBR_API int			nbr_str_cmp_tail(const char *a, const char *b, int len, int max);
NBR_API char		*nbr_str_chop(char *buffer);
NBR_API int			nbr_str_parse_url(const char *in, int max, char *host, U16 *port, char *url);
NBR_API size_t		nbr_str_utf8_copy(char *dst, int dlen, const char *src, int smax, int len);
NBR_API const char	*nbr_str_divide_tag_and_val(char sep, const char *line, char *tag, int tlen);
NBR_API const char	*nbr_str_divide(const char *sep, const char *line, char *tag, int *tlen);
NBR_API int 		nbr_parse_http_req_str(const char *req, const char *tag, char *buf, int buflen);
NBR_API int 		nbr_parse_http_req_int(const char *req, const char *tag, int *buf);
NBR_API int 		nbr_parse_http_req_bigint(const char *req, const char *tag, long long *buf);
NBR_API const char	*nbr_str_rchr(const char *in, char sep, int max);


/* array.c */
NBR_API ARRAY	nbr_array_create(int max, int size, int option);
NBR_API int		nbr_array_destroy(ARRAY ad);
NBR_API void	*nbr_array_alloc(ARRAY ad);
NBR_API int		nbr_array_free(ARRAY ad, void *p);
NBR_API void	*nbr_array_get_first(ARRAY ad);
NBR_API void	*nbr_array_get_next(ARRAY ad, void *p);
NBR_API int		nbr_array_get_index(ARRAY ad, void *p);
NBR_API void	*nbr_array_get_from_index(ARRAY ad, int index);
NBR_API void	*nbr_array_get_from_index_if_used(ARRAY ad, int index);
NBR_API int		nbr_array_is_used(ARRAY ad, void *p);
NBR_API int		nbr_array_max(ARRAY ad);
NBR_API int		nbr_array_use(ARRAY ad);
NBR_API int		nbr_array_full(ARRAY ad);
NBR_API int		nbr_array_get_size(ARRAY ad);
#define ARRAY_SCAN(__array,__p)									\
		for (__p = nbr_array_get_first(__array); __p != NULL;	\
			 __p = nbr_array_get_next(__array, __p))


/* search.c */
NBR_API SEARCH	nbr_search_init_int_engine(int max, int option, int hushsize);
NBR_API SEARCH	nbr_search_init_mint_engine(int max, int option, int hushsize, int num_key);
NBR_API SEARCH	nbr_search_init_str_engine(int max, int option, int hushsize, int length);
NBR_API SEARCH	nbr_search_init_mem_engine(int max, int option, int hushsize, int length);
NBR_API int		nbr_search_destroy(SEARCH sd);
NBR_API int		nbr_search_int_regist(SEARCH sd, int key, void *data);
NBR_API int		nbr_search_int_unregist(SEARCH sd, int key);
NBR_API void	*nbr_search_int_get(SEARCH sd, int key);
NBR_API int		nbr_search_str_regist(SEARCH sd, const char *key, void *data);
NBR_API int		nbr_search_str_unregist(SEARCH sd, const char *key);
NBR_API void	*nbr_search_str_get(SEARCH sd, const char *key);
NBR_API int		nbr_search_mem_regist(SEARCH sd, const char *key, int kl, void *data);
NBR_API int		nbr_search_mem_unregist(SEARCH sd, const char *key, int kl);
NBR_API void	*nbr_search_mem_get(SEARCH sd, const char *key, int kl);
NBR_API int		nbr_search_mint_regist(SEARCH sd, int key[], int n_key, void *data);
NBR_API int		nbr_search_mint_unregist(SEARCH sd, int key[], int n_key);
NBR_API void	*nbr_search_mint_get(SEARCH sd, int key[], int n_key);


/* sock.c */
NBR_API void	nbr_sock_set_nioconf(NIOCONF ioc);
NBR_API NIOCONF nbr_sock_nioconf();
NBR_API SOCKMGR	nbr_sockmgr_create(int nrb, int nwb,
					int max_sock,
					int workmem,
					int timeout_sec,
					const char *addr,
					PROTOCOL *proto,
					void *proto_p,
					int option);
NBR_API int		nbr_sockmgr_destroy(SOCKMGR s);
NBR_API SOCK	nbr_sockmgr_connect(SOCKMGR s, const char *address,
					void *proto_p, void *p);
NBR_API int		nbr_sockmgr_mcast(SOCKMGR s, const char *address, char *data, int len);
NBR_API void	nbr_sockmgr_set_data(SOCKMGR s, void *p);
NBR_API void	*nbr_sockmgr_get_data(SOCKMGR s);
NBR_API int		nbr_sockmgr_get_addr(SOCKMGR s, char *buf, int len);
NBR_API int		nbr_sockmgr_event(SOCKMGR s, int type, char *p, int len);
NBR_API int		nbr_sockmgr_get_stat(SOCKMGR s, SKMSTAT *st);
NBR_API int		nbr_sockmgr_get_ifaddr(SOCKMGR s,
					const char *ifn, char *buf, int len);
NBR_API SOCKMGR	nbr_sock_get_mgr(SOCK s);
NBR_API int 	nbr_sock_close(SOCK s);
NBR_API int		nbr_sock_event(SOCK s, char *p, int len);
NBR_API void	nbr_sock_clear(SOCK *p);
NBR_API int		nbr_sock_valid(SOCK s);
NBR_API int		nbr_sock_is_same(SOCK s1, SOCK s2);
NBR_API int		nbr_sock_writable(SOCK s);
NBR_API int		nbr_sock_get_addr(SOCK s, char *buf, int len);
NBR_API int		nbr_sock_get_local_addr(SOCK s,char *buf, int len);
NBR_API const void *nbr_sock_get_ref(SOCK s, int *len);
NBR_API void	*nbr_sock_get_data(SOCK s, int *len);
NBR_API void 	nbr_sockmgr_set_callback(SOCKMGR s,
					char *(*rp)(char*, int*, int*),
					int (*aw)(SOCK),
					int (*cw)(SOCK, int),
					int (*pp)(SOCK, char*, int),
					int (*eh)(SOCK, char*, int),
					UTIME (*poll)(SOCK));
NBR_API void	nbr_sockmgr_set_connect_cb(SOCKMGR s,
					int (*oc)(SOCK, void*));
NBR_API void	nbr_sockmgr_set_mgrevent_cb(SOCKMGR s,
					void (*meh)(SOCKMGR, int, char *, int));
NBR_API int		nbr_sock_get_wbuf(SOCK s, char **ppwb, int **ppnwb);
NBR_API int		nbr_sock_send_bin16(SOCK s, char *p, int len);
NBR_API int		nbr_sock_send_bin32(SOCK s, char *p, int len);
NBR_API int		nbr_sock_send_text(SOCK s, char *p, int len);
NBR_API int		nbr_sock_send_raw(SOCK s, char *p, int len);
NBR_API char	*nbr_sock_rparser_bin16(char *p, int *len, int *rlen);
NBR_API char	*nbr_sock_rparser_bin32(char *p, int *len, int *rlen);
NBR_API char	*nbr_sock_rparser_text(char *p, int *len, int *rlen);
NBR_API char	*nbr_sock_rparser_raw(char *p, int *len, int *rlen);
NBR_API int		nbr_sock_get_worker(THREAD ths[], int n_th);
NBR_API void	*nbr_sock_get_worker_data(THREAD th);
NBR_API void	*nbr_sock_get_worker_data_from(SOCK sk);
NBR_API int		nbr_sock_set_worker_data(THREAD th, void *p,
					void (*on_event)(SWKFROM*, THREAD, char *, size_t));
NBR_API int		nbr_sock_worker_event(SWKFROM *from, THREAD to, char *p, size_t l);
NBR_API int		nbr_sock_worker_bcast_event(SWKFROM *from, char *p, size_t l);
NBR_API int		nbr_sock_worker_is_current(SOCK sk);


/* sig.c */
NBR_API int		nbr_sig_set_handler(int signum, SIGFUNC fn);
NBR_API void	nbr_sig_set_ignore_handler(SIGFUNC fn);
NBR_API void	nbr_sig_set_intr_handler(SIGFUNC fn);
NBR_API void	nbr_sig_set_fault_handler(SIGFUNC fn);
NBR_API void 	nbr_sig_set_stop_handler(SIGFUNC fn);
NBR_API void	nbr_sig_set_logger(void (*logger)(const char*));


/* thread.c */
NBR_API	THPOOL	nbr_thpool_create(int max)		MTSAFE;
NBR_API int		nbr_thpool_init_jobqueue(THPOOL thp, int max_job)			MTSAFE;
NBR_API	int		nbr_thpool_addjob(THPOOL thp, void *p, void *(*fn)(void *))	MTSAFE;
NBR_API	int		nbr_thpool_destroy(THPOOL thp)								MTSAFE;
NBR_API int 	nbr_thpool_size(THPOOL thp)									MTSAFE;
NBR_API	THREAD	nbr_thread_create(THPOOL thp, void *p, void *(*fn)(void *))	MTSAFE;
NBR_API	int		nbr_thread_destroy(THPOOL thp, THREAD th)				MTSAFE;
NBR_API int		nbr_thread_wait_signal(THREAD th, int mine, int ms)		MTSAFE;
NBR_API int		nbr_thread_join(THREAD th, int force, void **p)			MTSAFE;
NBR_API int		nbr_thread_signal(THREAD th, int mine)	MTSAFE;
NBR_API int		nbr_thread_signal_bcast(THPOOL thp)		MTSAFE;
NBR_API MUTEX	nbr_thread_signal_mutex(THREAD th)		MTSAFE;
NBR_API int		nbr_thread_setcancelstate(int enable) 	MTSAFE;
NBR_API void	nbr_thread_testcancel()					MTSAFE;
NBR_API THRID	nbr_thread_get_id(THREAD th) 			MTSAFE;
NBR_API THRID	nbr_thread_get_curid()					MTSAFE;
NBR_API THREAD	nbr_thread_get_current()				MTSAFE;
NBR_API int		nbr_thread_is_current(THREAD th)		MTSAFE;
NBR_API void	*nbr_thread_get_data(THREAD th)			MTSAFE;
NBR_API	MUTEX	nbr_mutex_create()				MTSAFE;
NBR_API	int		nbr_mutex_destroy(MUTEX mx)		MTSAFE;
NBR_API	int		nbr_mutex_lock(MUTEX mx)		MTSAFE;
NBR_API	int		nbr_mutex_unlock(MUTEX mx)		MTSAFE;
NBR_API	RWLOCK	nbr_rwlock_create()				MTSAFE;
NBR_API int 	nbr_rwlock_destroy(RWLOCK rw)	MTSAFE;
NBR_API	int		nbr_rwlock_rdlock(RWLOCK rw)	MTSAFE;
NBR_API	int		nbr_rwlock_wrlock(RWLOCK rw)	MTSAFE;
NBR_API	int		nbr_rwlock_unlock(RWLOCK rw)	MTSAFE;
NBR_API SPINLK		nbr_spinlock_create()	MTSAFE;
NBR_API int		nbr_spinlock_destroy(SPINLK s)	MTSAFE;
NBR_API int		nbr_spinlock_lock(SPINLK s)	MTSAFE;
NBR_API void		nbr_spinlock_unlock(SPINLK s)	MTSAFE;


/* exlib/libconhash */
NBR_API CONHASH		nbr_conhash_init(STRHASHFUNC fn, int max_node, int max_replica);
NBR_API void		nbr_conhash_fin(CONHASH ch);
NBR_API void		nbr_conhash_set_node(CHNODE *n, const char *id, U32 rep);
NBR_API int		nbr_conhash_add_node(CONHASH ch, CHNODE *n);
NBR_API int		nbr_conhash_del_node(CONHASH ch, CHNODE *n);
NBR_API CHNODE		*nbr_conhash_lookup(CONHASH ch, const char *obj, size_t sz);
NBR_API void		nbr_conhash_get_vnodes(CONHASH ch, long *values, int size);
NBR_API U32		nbr_conhash_get_vnodes_num(CONHASH ch);
NBR_API int		nbr_conhash_node_registered(const CHNODE *n);
NBR_API int		nbr_conhash_node_fault(const CHNODE *n);
NBR_API int		nbr_conhash_node_set_fault(CHNODE *n);

#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
}
#endif


#endif
