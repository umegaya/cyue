/***************************************************************
 * test.cpp
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
#if 0
#include "app.h"
#include "server.h"

using namespace yue;

/****************************************************************/
/* map test */
struct testdata {
	int data;
};
/* ./yuem -t=map */
static int map_test(int argc, char *argv[], bool all) {
	util::map<testdata, int> a;
	a.init(1000, 1000, -1, util::opt_expandable);
	testdata td;
	for (int i = 0; i < 5; i++) {
		td.data = i + 1;
		a.insert(td, i + 1);
	}
	testdata *p = a.begin();
	for (; p; p = a.next(p)) {
		TRACE("testdata::%p, data = %u\n", p, p->data);
	}
	return 0;
}

/****************************************************************/
/* timer test */
struct timer_handler {
	int m_cnt;
	util::app &m_a;
	timer_handler(util::app &a) : m_cnt(0), m_a(a) {}
	int operator () (loop::timer_handle t) {
		TRACE("start: %u: time=%llu\n", t->tick(), util::time::now());
		m_cnt++;
		if (t->tick() >= 5) { m_a.die(); }
		return t->tick() >= 5 ? NBR_EINVAL : NBR_OK;
	}
};
/* ./yuem -t=timer */
static bool timer_test(int argc, char *argv[], bool all) {
	util::app a;
	verify_success(a.init<server>(argc, argv));
	timer_handler th(a);
	verify_true(loop::timer().add_timer(th, 0.5, 0.2) != NULL);
	//util::time::sleep(1600LL * 1000 * 1000); /* sleep 1.6sec */
	a.run<server>(argc, argv);
	verify_true(th.m_cnt == 5);
	return 0;
}

/****************************************************************/
/* ping test */
static struct ping_handler *g_ph;
static int gn_ph = 0;
static int gn_iter = 100;
typedef yue::handler::socket session;
struct ping_handler {
	session *ss;
	int cnt, id, err;
	void set_watcher(session::watcher *) {}
	int send_ping(UTIME &tstamp, MSGID &msgid) {
		fiber::handler h(*this);
		PROCEDURE(keepalive)::args_and_cb<fiber::handler> a(h);
		tstamp = a.m_tstamp = util::time::now();
		TRACE("send_ping: %p: ", this);
		msgid = yue::rpc::call(*ss, a);
		if (msgid != serializer::INVALID_MSGID) {
			/* wait for the reply of keepalive. if reply arrived,
			 * then operator () (fabric&,object&) called. */
			return fiber::exec_yield;
		}
		return fiber::exec_error;
	}
	int operator () (fabric &, object &o) {
		//ASSERT(cnt < 100);
		MSGID msgid; UTIME ts;
		if (o.is_error()) {
			printf("response : error %d\n", (int)o.error().elem(0));
			err = o.error().elem(0);
			ss->close();
			return fiber::exec_finish;
		}
		ASSERT(ss->valid());
		rpc::keepalive::rval::accessor a(o);
		++cnt;
		if ((cnt % 5) == 0) {
			TRACE("response: %u: time=%llu, count=%u, msgid=%u\n", id, a.tstamp(), cnt, o.msgid());
			//printf(".");
		}
		if (cnt >= gn_iter) {
			bool ok = true;
			TRACE("%u: finish\n", id);
			TRACE("unfinised: ");
			for (int i = 0; i < gn_ph; i++) {
				if (g_ph[i].cnt < gn_iter) {
					TRACE("%u", g_ph[i].id);
					if (g_ph[i].err < 0) {
						TRACE("(e:%d)", g_ph[i].err);
					}
					else {
						ok = false;
					}
					TRACE(",");
				}
			}
			TRACE("\n");
			if (ok) {
				TRACE("OK: all connection (%u) correctly finished or end in failure\n", gn_ph);
			}
			return fiber::exec_finish;
		}
		if (send_ping(ts, msgid) != fiber::exec_yield) {
			err = NBR_ESEND;
			return fiber::exec_error;
		}
		TRACE("send_ping: %u: time=%llu, msgid=%u\n", id, ts, msgid);
		return fiber::exec_yield;
	}
	bool operator () (session *s, int state) {
	TRACE("operator (): %p, %u\n", s, state);
		if (!s->valid()) {
			TRACE("connect fail\n");
			return handler::monitor::STOP;
		}
		ss = s;
		if (state == session::CLOSED) {
			err = NBR_EINVAL;
		}
		TRACE("on_accept: %p: ", this);
		UTIME ts; MSGID msgid; int r;
		if ((r = send_ping(ts, msgid)) < 0) { err = r; return r; }
		TRACE("start: %u: time=%llu, msgid=%u\n", id, ts, msgid);
		return handler::monitor::STOP;
	}
};

struct timeout_handler {
	util::app &m_a;
	timeout_handler(util::app &a) : m_a(a) {}
	int operator () (loop::timer_handle t) {
		U32 c = t->tick();
		if (c <= 1) { return 0; }
		m_a.die();
		int i, cnt, ecnt;
		for (i = 0, cnt = 0, ecnt = 0; i < gn_ph; i++) { 
			cnt += g_ph[i].cnt;
			if (g_ph[i].err < 0) { ecnt++; }
			if (g_ph[i].cnt < 100) { printf("%u: stalled (%u)\n", g_ph[i].id, g_ph[i].cnt); }
		}
		printf("%u processed in 10sec (ecnt = %d)\n", cnt, ecnt);
		return 0;
	}
};

/* ./yuem -t=ping 0 1000 {comp mode} or ./yuem -t=ping 1 1000 {comp mode} */
static int ping_test(int argc, char *argv[], bool all) {
	if (all) { argv[2] = (char *)"1"; argv[3] = (char *)"1000"; }
	int sv, n_client, comp_mode;
	char *_argv[16]; int _argc = 3;
	_argv[0] = _argv[1] = NULL;
	_argv[2] = (char *)"1";
	util::app a;
	verify_success(a.init<server>(_argc, _argv));
	verify_success(util::str::atoi(argv[2], sv, 256));
	printf("argc = %d\n", argc);
	if (argc < 5 || util::str::atoi(argv[4], comp_mode, 256) < 0) {
		comp_mode = 1;
	}
	gn_iter = (comp_mode ? 1000 : 100);
	if (sv) {
		char arg3[256];
		verify_success(util::str::atoi(argv[3], n_client, 256));
		util::str::printf(arg3, sizeof(arg3), "%u", n_client);
		char *cli_argv[5] = { argv[0], argv[1], (char *)"0", arg3, argv[4] };
		verify_success(server::fork(argv[0], cli_argv));
		verify_success(server::listen("tcp://0.0.0.0:8888"));
		return a.run<server>(argc, argv, 4);
	}
	else {
		util::time::sleep(500LL * 1000 * 1000);
		verify_success(util::str::atoi(argv[3], n_client, 256));
		util::syscall::rlimit rl;
		if(util::syscall::getrlimit(RLIMIT_STACK, &rl) < 0) {
			return NBR_ESYSCALL;
		}
		/* if using 90% of current stack size, error. */
		if (rl.rlim_cur < (sizeof(session) * n_client)) {
			TRACE("too much client %u %u\n", (n_client * (int)sizeof(session)), (unsigned int)rl.rlim_cur);
			return NBR_ESHORT;
		}
		session ss[n_client];
		ping_handler h[n_client];
		g_ph = h;
		gn_ph = n_client;
		for (int i = 0; i < n_client; i++) {
			h[i].id = (i + 1);
			h[i].cnt = 0;
			h[i].err = 0;
			verify_success(ss[i].connect("tcp://localhost:8888", h[i]));
		}
		timeout_handler th(a);
		util::functional<int (loop::timer_handle)> hh(th);
		if (comp_mode) {
			verify_true(loop::timer().add_timer(hh, 0.0, 10.0) != NULL);
		}
		printf("gn_iter = %d, comp_mode = %d\n", gn_iter, comp_mode);
		return a.run<server>(_argc, _argv);
	}
}

/****************************************************************/
/* serializer test */
static bool serializer_test1(int argc, char *argv[], bool all) {
	pbuf pbf;
	verify_success(pbf.reserve(65535));
	serializer sr;
	sr.start_pack(pbf);
	verify_success(sr.push_array_len(4));
	verify_success(sr << (U8)0);
	verify_success(sr << (U32)1001);
	verify_success(sr << (U32)9);
	verify_success(sr.push_array_len(5));
		verify_success(sr << "keepalive");
		verify_success(sr << 3.0f);
		verify_success(sr << 2.0f);
		verify_success(sr << 1.0f);
		verify_success(sr << 0.0f);
	//pbf.commit(sr.len());
	verify_true(sr.unpack(pbf) == serializer::UNPACK_SUCCESS);
	object &o = sr.result();
	verify_true(o.type() == 0);
	verify_true(o.msgid() == 1001);
	verify_true(o.cmd() == 9);
	verify_true(o.alen() == 5);
		verify_true(o.arg(0).kind() == rpc::datatype::STRING);
			verify_true(util::str::cmp(o.arg(0), "keepalive") == 0);
		verify_true(o.arg(1).kind() == rpc::datatype::DOUBLE);
			verify_true(((float)o.arg(1)) == 3.0f);
		verify_true(o.arg(2).kind() == rpc::datatype::DOUBLE);
			verify_true(((float)o.arg(2)) == 2.0f);
		verify_true(o.arg(3).kind() == rpc::datatype::DOUBLE);
			verify_true(((float)o.arg(3)) == 1.0f);
		verify_true(o.arg(4).kind() == rpc::datatype::DOUBLE);
			verify_true(((float)o.arg(4)) == 0.0f);
	o.fin();
	return true;
}

static bool serializer_test2(int argc, char *argv[], bool all) {
	pbuf pbf;
	verify_success(pbf.reserve(65535));
	serializer sr;
	sr.start_pack(pbf);
	verify_success(sr.push_array_len(4));
	verify_success(sr << (U8)0);
	verify_success(sr << (U32)1002);
	verify_success(sr << (U32)9);
	verify_success(sr.push_array_len(2));
		verify_success(sr.pushnil());
		verify_success(sr.push_raw("abcdef", sizeof("abcdef") - 1));
	verify_success(sr.pushnil());
	//pbf.commit(sr.len());
	verify_true(sr.unpack(pbf) == serializer::UNPACK_EXTRA_BYTES);
	object o = sr.result();
	verify_true(o.type() == 0);
	verify_true(o.msgid() == 1002);
	verify_true(o.cmd() == 9);
	verify_true(o.alen() == 2);
		verify_true(o.arg(0).kind() == rpc::datatype::NIL);
		verify_true(o.arg(1).kind() == rpc::datatype::BLOB);
			verify_true(util::mem::cmp(o.arg(1), "abcdef", sizeof("abcdef") - 1) == 0);

	verify_true(sr.unpack(pbf) == serializer::UNPACK_SUCCESS);
	object o2 = sr.result();
	verify_true(o2.kind() == rpc::datatype::NIL);
	o.fin();
	o2.fin();
	return true;
}

static bool serializer_test3(int argc, char *argv[], bool all) {
	pbuf pbf;
	verify_success(pbf.reserve(65535));
	serializer sr;
	sr.start_pack(pbf);
	verify_success(sr.push_array_len(4));
	verify_success(sr << (U8)0);
	verify_success(sr << (U32)1003);
	verify_success(sr << (U32)9);

	//size_t curlen = sr.len();
	//pbf.commit(sr.len());
	verify_true(sr.unpack(pbf) == serializer::UNPACK_CONTINUE);

	verify_success(sr.push_array_len(2));
		verify_success(sr.pushnil());
		verify_success(sr.push_raw("abcdef", sizeof("abcdef") - 1));
	verify_success(sr.pushnil());
	//pbf.commit(sr.len() - curlen);
	verify_true(sr.unpack(pbf) == serializer::UNPACK_EXTRA_BYTES);
	object o = sr.result();
	verify_true(o.type() == 0);
	verify_true(o.msgid() == 1003);
	verify_true(o.cmd() == 9);
	verify_true(o.alen() == 2);
		verify_true(o.arg(0).kind() == rpc::datatype::NIL);
		verify_true(o.arg(1).kind() == rpc::datatype::BLOB);
			verify_true(util::mem::cmp(o.arg(1), "abcdef", sizeof("abcdef") - 1) == 0);

	verify_true(sr.unpack(pbf) == serializer::UNPACK_SUCCESS);
	object o2 = sr.result();
	verify_true(o2.kind() == rpc::datatype::NIL);
	o.fin();
	o2.fin();
	return true;
}

static bool serializer_test4(int argc, char *argv[], bool all) {
	pbuf pbf;
	verify_success(pbf.reserve(65535));
	serializer sr;
	sr.start_pack(pbf);
	verify_success(sr.push_array_len(4));
	verify_success(sr << (U8)0);
	verify_success(sr << (U32)1004);
	verify_success(sr << (U32)9);
	verify_success(sr.push_array_len(1));
			verify_success(sr.pushnil());

	verify_success(sr.push_array_len(4));
	verify_success(sr << (U8)1);
	verify_success(sr << (U32)1005);
	verify_success(sr << (U32)10);
	verify_success(sr.push_array_len(1));
			verify_success(sr.pushnil());

	//pbf.commit(sr.len());
	verify_true(sr.unpack(pbf) == serializer::UNPACK_EXTRA_BYTES);
	object o = sr.result();
	verify_true(sr.unpack(pbf) == serializer::UNPACK_SUCCESS);
	object o2 = sr.result();

	verify_true(o.type() == 0);
	verify_true(o.msgid() == 1004);
	verify_true(o.cmd() == 9);

	verify_true(o2.type() == 1);
	verify_true(o2.msgid() == 1005);
	o.fin();
	o2.fin();
	return true;
}

static bool serializer_test(int argc, char *argv[], bool all) {
	verify_true(serializer_test1(argc, argv, all));
	verify_true(serializer_test2(argc, argv, all));
	verify_true(serializer_test3(argc, argv, all));
	verify_true(serializer_test4(argc, argv, all));
	return true;
}


/****************************************************************/
/* session test */
#define WATCHERS (4)
struct session_watcher {
	util::app *sv;
	int state[16];
	int n_ptr, n_id;
	session_watcher() : n_ptr(0) {}
	void set_watcher(session::watcher *) {}

	bool operator () (session *s, int st) {
		ASSERT(st != session::HANDSHAKE);
		//TRACE("ss=%p,id=%u,st=%u\n", s, n_id, st);
		state[n_ptr++] = st;
		verify_true(n_ptr <= 4);
		if (n_ptr == 0) { verify_true(st == session::ESTABLISH); }
		if (n_id == WATCHERS && st == session::WAITACCEPT) {
			TRACE("permit access\n");
			s->permit_access();
		}
		if (n_id == WATCHERS && st == session::ESTABLISH) {
			TRACE("closed!!\n");
			s->close();
		}
		else if (n_id == 1 && st == session::CLOSED) {
			sv->die();
		}
		return handler::monitor::KEEP;
	}
};
static bool session_test1(int argc, char *argv[], bool all) {
	util::app a;
	session ss;
	session_watcher _sw[WATCHERS];
	verify_success(a.init<server>(0, NULL));
	server::listen("tcp://0.0.0.0:8888");
	for (int i = 0; i < WATCHERS; i++) {
		_sw[i].n_id = (i + 1);
		_sw[i].sv = &a;
		verify_success(ss.connect("tcp://localhost:8888", _sw[i]));
	}

	int r = a.run<server>(0, NULL, 4);

	int correct_answer[] = {
			session::WAITACCEPT,
			session::ESTABLISH,
			session::CLOSED,
			session::FINALIZED,
	};
	for (int i = 0; i < WATCHERS; i++) {
		verify_true(util::mem::cmp(_sw[i].state,
			correct_answer, sizeof(correct_answer)) == 0);
	}

	verify_success(r);
	return true;
}


struct accept_watcher {
	util::app &m_s;
	DSCRPTR m_fd;
	net::address m_a;
	accept_watcher(util::app &s) : m_s(s), m_fd(INVALID_FD), m_a() {}
	int operator () (DSCRPTR fd, DSCRPTR afd, net::address &a, handler::base **ch) {
		m_fd = fd;
		m_a = a;
		return server::spool()(fd, afd, a, ch);
	}
	session *attached() { return m_fd == INVALID_FD ? NULL : server::served_for(m_a, m_fd); }
};
struct session_watcher2 : public session_watcher {
	void set_watcher(session::watcher *) {}
	bool operator () (session *s, int st) {
		ASSERT(st != session::HANDSHAKE);
//		TRACE("ss2=%p,id=%u,st=%u\n", s, n_id, st);
		state[n_ptr++] = st;
		return handler::monitor::KEEP;
	}
};
struct session_watcher3 : public session_watcher {
	void set_watcher(session::watcher *) {}
	bool operator () (session *s, int st) {
		ASSERT(st != session::HANDSHAKE);
//		TRACE("ss2=%p,id=%u,st=%u\n", s, n_id, st);
		state[n_ptr++] = st;
		if (st == session::CLOSED) {
			s->reconnect();
		}
		return handler::monitor::KEEP;
	}
};
struct timer_force_close_handler {
	accept_watcher &m_ah;
	timer_force_close_handler(accept_watcher &ah) : m_ah(ah) {}
	int operator () (loop::timer_handle t) {
		U32 c = t->tick();
		if (c > ((U64)(session::MAX_CONN_RETRY + 1))) {
			TRACE("finally util::app died\n");
			m_ah.m_s.die();
			return -1;
		}
		TRACE("close %u times\n", c);
		if (m_ah.attached()) { m_ah.attached()->close(); }
		return 0;
	}
};
/* test session reconnection */
static bool session_test3(int argc, char *argv[], bool all) {
	util::app s;
	session ss;
	session_watcher3 _sw[WATCHERS];
	verify_success(s.init<server>(0, NULL));
	accept_watcher aw(s);
	handler::accept_handler ah(aw);
	server::listen("tcp://0.0.0.0:8888", ah);
	for (int i = 0; i < WATCHERS; i++) {
		_sw[i].n_id = (i + 1);
		_sw[i].sv = &s;
		verify_success(ss.connect("tcp://localhost:8888", _sw[i]));
	}

	timer_force_close_handler th(aw);
	util::functional<int (loop::timer_handle)> hh(th);
	verify_true(loop::timer().add_timer(hh, 1.0, 3.0) != NULL);

	int r = s.run<server>(0, NULL, 4);

	int correct_answer[] = {
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
			session::CLOSED,
			session::WAITACCEPT,
	};
	for (int i = 0; i < WATCHERS; i++) {
		verify_true(util::mem::cmp(_sw[i].state,
			correct_answer, sizeof(correct_answer)) == 0);
	}

	verify_success(r);
	return true;
}
/* test behavior of connecting unreachable host */
static bool session_test2(int argc, char *argv[], bool all) {
	util::app s;
	session ss;
	session_watcher2 _sw[WATCHERS];
	verify_success(s.init<server>(0, NULL));
	accept_watcher aw(s);
	handler::accept_handler ah(aw);
	server::listen("tcp://0.0.0.0:8888", ah);
	for (int i = 0; i < WATCHERS; i++) {
		_sw[i].n_id = (i + 1);
		_sw[i].sv = &s;
		verify_success(ss.connect("tcp://192.168.56.1:8888", _sw[i]));
	}

	/* timer never affect (because connection never establish) */
	timer_force_close_handler th(aw);
	util::functional<int (loop::timer_handle)> hh(th);
	verify_true(loop::timer().add_timer(hh, 1.0, 3.0) != NULL);

	int r = s.run<server>(argc, argv, 4);

	int correct_answer[] = {
			session::CLOSED,
	};
	for (int i = 0; i < WATCHERS; i++) {
		verify_true(util::mem::cmp(_sw[i].state,
			correct_answer, sizeof(correct_answer)) == 0);
	}

	verify_success(r);
	return true;
}
static bool session_test(int argc, char *argv[], bool all) {
	verify_true(session_test1(argc, argv, all));
	verify_true(session_test2(argc, argv, all));
	verify_true(session_test3(argc, argv, all));
	return true;
}


/****************************************************************/
/* main test routine */
#define TEST(name) if (util::str::cmp(module, #name) == 0 || all) {	\
	char *tmp_argv[argc];											\
	util::mem::copy(tmp_argv, argv, argc * sizeof(argv[0]));		\
	verify_success(name##_test(argc, tmp_argv, all));				\
}

int test(char *module, int argc, char *argv[]) {
	bool all = util::str::cmp(module, "all") == 0;
	TEST(map);
	TEST(timer);
	TEST(session);
	TEST(ping);
	TEST(serializer);
	return 0;
}
#endif

int main (int argc, char *argv[]) {
	return 0;//test(argv[1] + 3, argc, argv);
}
