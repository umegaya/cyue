struct misc {
	struct time {
		static int init(VM vm) {
			lua_newtable(vm);

			/* API 'clock' */
			lua_pushcfunction(vm, clock);
			lua_setfield(vm, -2, "clock");
			/* API 'now' */
			lua_pushcfunction(vm, now);
			lua_setfield(vm, -2, "now");
			/* API 'sleep' */
			lua_pushcfunction(vm, sleep);
			lua_setfield(vm, -2, "sleep");
			/* API 'suspend' */
			lua_pushcfunction(vm, suspend);
			lua_setfield(vm, -2, "suspend");

			lua_setfield(vm, -2, "time");
			return 0;
		}
		static int clock(VM vm) {
			lua_pushnumber(vm, util::time::clock());
			return 1;
		}
		static int now(VM vm) {
			lua_pushnumber(vm, util::time::now());
			return 1;
		}
		static int sleep(VM vm) {
			util::time::sleep((NTIME)(lua_tonumber(vm, -1) * 1000 * 1000 * 1000));
			return 0;
		}
		static int suspend(VM vm) {
			coroutine *co = coroutine::to_co(vm);
			lua_error_check(vm, co, "to_co");
			lua_error_check(vm, loop::timer().add_timer(*co,
				lua_tonumber(vm, -1), lua_tonumber(vm, -1)), "fail to create timer task");
			return co->yield();
		}
	};
	struct net {
		static int init(VM vm) {
			lua_newtable(vm);

			/* API 'localaddr' */
			lua_pushcfunction(vm, localaddr);
			lua_setfield(vm, -2, "localaddr");

			lua_setfield(vm, -2, "net");
			return 0;
		}
		static int localaddr(VM vm) {
			if ((!lua_isnumber(vm, -2)) || (!lua_isstring(vm, -1))) {
				ASSERT(false);
				lua_pushfstring(vm, "type error %d %d", lua_type(vm, -2), lua_type(vm, -1));
				lua_error(vm);
			}
			int r; char la[256];
			size_t n_la = 256;
			if ((r = yue::util::syscall::get_if_addr(
				lua_tointeger(vm, -2), lua_tostring(vm, -1), la, n_la)) < 0) {
				return r;
			}
			lua_pushlstring(vm, la, r);
			return 1;
		}
	};
	struct shm {
		static const size_t INITIAL_MAX_SHM_ENT = 4;
		struct ent {
			void *m_p;
			util::thread::rwlock m_lock;
			ent() : m_p(NULL), m_lock() {}
			~ent() {
				if (m_p) { util::mem::free(m_p); }
				m_lock.fin();
			}
			int init(size_t sz) {
				if (m_p) {
					return NBR_OK;
				}
				if (!(m_p = util::mem::alloc(sz))) {
					return NBR_EMALLOC;
				}
				if (m_lock.init() < 0) {
					return NBR_EPTHREAD;
				}
				return NBR_OK;
			}
		};
		static util::map<ent, const char*> m_shmm;
		static int static_init() {
			if (!m_shmm.init(INITIAL_MAX_SHM_ENT, INITIAL_MAX_SHM_ENT,
				-1, util::opt_threadsafe | util::opt_expandable)) {
				return NBR_EMALLOC;
			}
			return NBR_OK;
		}
		static int init(VM vm) {
			lua_newtable(vm);

			/* API 'insert' */
			lua_pushcfunction(vm, insert);
			lua_setfield(vm, -2, "insert");
			/* API 'wrlock' */
			lua_pushcfunction(vm, wrlock);
			lua_setfield(vm, -2, "wrlock");
			/* API 'rdlock' */
			lua_pushcfunction(vm, rdlock);
			lua_setfield(vm, -2, "rdlock");
			/* API 'unlock' */
			lua_pushcfunction(vm, unlock);
			lua_setfield(vm, -2, "unlock");
			/* API 'fetch' */
			lua_pushcfunction(vm, fetch);
			lua_setfield(vm, -2, "fetch");

			lua_setfield(vm, -2, "shm");
			return 0;
		}
		static void static_fin() {
			m_shmm.fin();
		}
		static int insert(VM vm) {
			if (!lua_isnumber(vm, -1) || !lua_isstring(vm, -2)) {
				lua_pushfstring(vm, "type error %d %d",
					lua_type(vm, -1), lua_type(vm, -2));
				lua_error(vm);
			}
			size_t sz = lua_tointeger(vm, -1);
			const char *key = lua_tostring(vm, -2);
			ent *e; bool exist;
			if (!(e = m_shmm.alloc(key, &exist))) {
				lua_pushfstring(vm, "insert map");
				lua_error(vm);
			}
			int r;
			if (!exist && (r = e->init(sz)) < 0) {
				m_shmm.erase(key);
				lua_pushfstring(vm, "init ent %d", r);
				lua_error(vm);
			}
			lua_pushlightuserdata(vm, e->m_p);
			lua_pushboolean(vm, exist);
			return 2;
		}
		static inline int lock(VM vm, bool rd) {
			if (!lua_isstring(vm, -1)) {
				lua_pushfstring(vm, "type error %d", lua_type(vm, -1));
				lua_error(vm);
			}
			ent *e; int r;
			if (!(e = m_shmm.find(lua_tostring(vm, -1)))) {
				lua_pushfstring(vm, "key not found %s", lua_tostring(vm, -1));
				lua_error(vm);
			}
			if ((r = rd ? e->m_lock.rdlock() : e->m_lock.wrlock()) < 0) {
				lua_pushfstring(vm, "lock fails %d", r);
				lua_error(vm);
			}
			lua_pushlightuserdata(vm, e->m_p);
			return 1;
		}
		static int wrlock(VM vm) {
			return lock(vm, false);
		}
		static int rdlock(VM vm) {
			return lock(vm, true);
		}
		static int unlock(VM vm) {
			if (!lua_isstring(vm, -1)) {
				lua_pushfstring(vm, "type error %d", lua_type(vm, -1));
				lua_error(vm);
			}
			ent *e;
			if (!(e = m_shmm.find(lua_tostring(vm, -1)))) {
				lua_pushfstring(vm, "key not found %s", lua_tostring(vm, -1));
				lua_error(vm);
			}
			e->m_lock.unlock();
			return 0;
		}
		static int fetch(VM vm) {
			if (!lua_isstring(vm, -1)) {
				lua_pushfstring(vm, "type error %d", lua_type(vm, -1));
				lua_error(vm);
			}
			ent *e;
			if (!(e = m_shmm.find(lua_tostring(vm, -1)))) {
				lua_pushfstring(vm, "key not found %s", lua_tostring(vm, -1));
				lua_error(vm);
			}
			lua_pushlightuserdata(vm, e->m_p);
			return 0;
		}
	};
	static int static_init() {
		shm::static_init();
		return NBR_OK;
	}
	static int init(VM vm) {
		lua_newtable(vm);
		time::init(vm);
		shm::init(vm);
		net::init(vm);
		return 0;
	}
	static void static_fin() {
		shm::static_fin();
	}
	static void fin() {
	}
};
yue::util::map<misc::shm::ent, const char*> misc::shm::m_shmm;

