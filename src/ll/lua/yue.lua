if not yue then require('libyue') end
local y = yue
yue = {}

-- @module_name:	yue.ffi
-- @desc:	the luajit ffi
yue.ffi = (function()
	local ok,r = pcall(require,'ffi')
	return ok and r or nil
end)()
local C = yue.ffi and yue.ffi.C or nil

	
-- @module_name:	yue.ld
-- @desc:	decide calling function from rpc command and caller context
yue.ld = (function()
	local m = {
		_add = function(f)
			local org = _G[y.ldname]
			_G[y.ldname] = function(s, c)
				return f(s, c) or org(s, c)
			end
			return org
		end
	}


	--	@name: local fetcher
	--	@desc: helper function to fetch actual object from rpc method name
	--			it resolve a.b.c => _G[a][b][c], and if symbol contains _ on its top,
	--			it cannot call from remote node (that is, when local_call == false)
	-- 	@args: 	t:	table to be resolve symbol
	--			k:	rpc method name
	--			local_call:	if true, it can call protected method which name is like _funcname. otherwise cannot.
	local fetcher = function(t, k, local_call)
		local kl, c, b, r, sk = #k, 0, nil, t, ''
		print(kl,c,b,r,'['..sk..']',k)
		while c < kl do
			b = string.char(k:byte(c + 1))
			c = (c + 1)
			if b == '.' then
				r = r[sk]
				if not r then 
					return function(...) error(k .. ' not found') end
				end
				sk = ''
			elseif (not local_call) and #sk == 0 and b == '_' then
				-- attempt to call protected method
				-- print('attempt to call protected method',local_call,sk,b)
				return function(...) error(k .. ' not found') end
			else
				sk = (sk .. b)
			end
		end
		print('fetcher finished', r[sk])
		r = r[sk];
		if not r then
			return function(...) error(k .. ' not found') end
		else
			return r
		end
	end
	local add_symbol = function (t, k, v)
		if type(v) == 'function' then
			local ok,r = pcall(setfenv, v, t.__symbols)
			if not ok then error(r) end
		end
		rawset(t.__symbols, k, v)
		return v
	end
	local importer = function (self, file)
		if type(file) == 'string' then
			local f,e = loadfile(file)
			if not f then error(e) 
			else 
				setfenv(f, self)
				f()
			end
		elseif type(file) == 'table' then
			for k,filename in pair(file) do
				self.import(filename)
			end
		else
			error('invalid type:' .. type(file))
		end
		return self
	end
	local remote_namespace_mt = {
		__newindex = add_symbol,
		__index = function(t, k)
			local r = fetcher(t.__symbols, k, false)
			t[k] = r
			return r
		end
	}
	local local_namespace_mt = {
		__index = function(t, k)
			local r = fetcher(t.__symbols, k, true)
			t[k] = r
			return r
		end
	}
	
	
	--	@name: create_namespace
	--	@desc: create namespace, which provide accessible symbol list for each listener 
	--	(thus, if listner fd = 7 defines func_fd7 in its namespace, 
	--	it cannot found in rpc call through listner fd = 8 and so on.)
	-- 	@args: fd:	listener fd
	local namespaces = {}
	m.create_namespace = function(fd)
		namespaces[fd] = setmetatable({
					__fd = fd, 
					__symbols = setmetatable({}, {__index = _G}),
					import = importer,
				}, remote_namespace_mt)
		return namespaces[fd]
	end
	
	
	--	replace ld
	local local_namespace = setmetatable({
					__symbols = setmetatable({}, {__index = _G})
				}, local_namespace_mt)
	m.ctx = -1
	_G[y.ldname] = function(s, c)
		m.ctx = c
		return c >= 0 and namespaces[c][s] or local_namespaces[s]
	end
	yue.__unpack = function(rb)
		local p,size = y.read(rb)
		local t = yue.ffi.cast("yue_object_t*", p)
		if t.type == C.YUE_OBJECT_METHOD then
			return _G[y.ldname](yue.ffi.string(t.data,size - 1), m.ctx)
		elseif t.type == C.YUE_OBJECT_ACTOR then
			error("currently actor unpack not supported for security reason")
		else
			error("invalid object type:" .. t.type)
		end
	end	
	
	return m
end)()


--	@module_name: pack
--	@desc: yue object packer/unpacker
yue.pack = (function()
	local m = {}
	yue.ffi.cdef[[
		enum {
			YUE_OBJECT_METHOD = 1,
			YUE_OBJECT_ACTOR = 2,
		};
		typedef struct {
			unsigned char type;
			char data[0];
		}	yue_object_t;
	]]
	m.method = function(self,wb)
		return y.pack.method(self.__m,wb)
	end
	m.actor = function(self,wb)
		return y.pack.actor(self.__c,wb)
	end
	return m
end)()


-- @module_name:	yue.core
-- @desc:	core feature of yue which mainly provided by yue.so
yue.core = (function () 
	-- 	copy core symbols to module table
	local m = {
		poll = y.poll,
		connect = y.connect,
		listen = y.listen,
		timer = y.timer,
		stop_timer = y.stop_timer,
		sleep = y.sleep,
		die = y.stop,
		error = y.error,
		peer = y.peer,
	}
	--	@name: local protect
	--	@desc: helper function to create protected rpc connection 
	--			(using same mechanism as lua_*callk in lua 5.2)
	-- 	@args: p:	conn (which is created by y.open) to be protected
	local protect_mt = { 
		__call = function(f,...)
			local r = {f.__m(...)}
			if not y.error(r[1]) then return unpack(r)
			else error(r[1]) end
		end,
		__index = function(t, k)
			local r = setmetatable(
				{ __m = t.__m[k], __pack = yue.pack.method }, 
				getmetatable(t))
			t[k] = r
			return r
		end,
	}
	local function protect(p)
		local c = { __c = p, __pack = yue.pack.actor }
		return setmetatable(c, {
			__index = function(t, k)
				local r = setmetatable(
					{ __m = t.__c[k], __pack = yue.pack.method }, 
					protect_mt)
				t[k] = r
				return r
			end,
		})
	end

	--	@name: yue.tick
	--	@desc: callback per process (any thread have possibility to call it)
	-- 	@args: 
	-- 	@rval: 
	local tick = (function ()
		return setmetatable({}, {
			__call = function (t, ...)
				for k,v in pairs(t) do
					v()
				end
			end,
			push = function(self, f)
				self:insert(f)
			end,
			pop = function(self, f)
				for k,v in pairs(t) do
					if v == f then
						return self.remove(k)
					end
				end
				return nil
			end
		})
	end)
	yue.tick = tick()


	--	@name: yue.core.try
	--	@desc: provide exception handling
	-- 	@args: main: code block (function) to be executed
	--		   catch: error handler
	--		   finally: code block which is called antime when 
	--					main logic execution finished
	-- 	@rval: 
	m.try = function(main, catch, finally)
		-- lua 5.1 doc $2.3, 
		-- 'When a function is created, 
		-- it inherits the environment from the function that created it.' 
		-- so even if yue.run set some fenv to callee function, 
		-- yue functions declared in here still have its environment _G.
		local env = getfenv(main)
		local oerror = env.error
		-- if nested try is called, its oerror will be _G.error.
		-- so pcall works well... (also error is works as you expect)
		env.error = _G.error
		
		-- call body func
		local ok,r = pcall(main)
		
		-- back environment to original error func
		env.error = oerror
		if not ok and not catch(r) then
			finally()
			oerror(r)
		end
		finally()
	end

	
	--	@name: yue.core.open
	--	@desc: open remote node
	-- 	@args: host: remote node address
	--			opt: connect option
	-- 	@rval: 
	m.open = function(host, opt)
		return protect(y.connect(host, opt))
	end


	--	@name: yue.core.listen
	--	@desc: listen on specified port
	-- 	@args: host: address to listen to
	--			opt: listen option
	--			file: if it specified, load from symbol and add them to namespace.
	-- 	@rval: 
	m.listen = function(host, opt)
		local fd = y.listen(host, opt)
		if type(fd) ~= 'number' or fd < 0 then return nil end
		return { 
			namespace = yue.ld.create_namespace(fd),
			localaddr = function(self,ifname)
				return (yue.util.net.localaddr(fd, ifname) .. host:sub(host:find(':')))
			end
		}
	end
	
	
	return m
end)()



-- @module_name:	yue.dev
-- @desc:	api for creating lua module which can run cooperate with yue
yue.dev = (function()
	local m = {
		read = y.read,
		write = y.write,
		yield = y.yield,
		socket = y.socket,
	}
	local rcb = function(sk, s, func)
		while true do
			local r = sk:read_cb(func(s))
			if r == -1 then
				print('rcb: yield', sk:fd())
				y.yield()
			else
				return r
			end
		end
	end
	__sock_mt.try_read = function(sk, func)
		print('try_read from:', sk:fd())
		local s = sk:try_connect()
		return sk:try_connect() and rcb(sk, s, func) or nil
	end
	__sock_mt.try_write = function(sk, func)
		print('try_write to:', sk:fd())
		local s = sk:try_connect()
		return s and func(s) or nil		
	end
	return m
end)()



-- @module_name:	yue.client
-- @desc:	api for access yue server from luajit console or other lua client program
yue.client = (function ()
	local m = {
		mode = y.mode
	}

	--	tables which stores yue execution result
	local result = {}
	local alive;


	--	@name: terminate
	--	@desc: finish lua rpc yieldable program with return code
	-- 	@args: ok: execution success or not
	--		   code: return code which returned by yue.run or yue.exec
	-- 	@rval: 
	local function terminate(ok, code) 
		alive = false
		result.code = code
		result.ok = ok
		y.exit()
	end


	--	@name: exit
	--	@desc: equivalent to yue.terminate(false, code)
	--	@args: code: return code
	-- 	@rval:
	local function exit(code) 
		terminate(true, code) 
	end


	--	@name: raise
	--	@desc: equivalent to yue.exit(false, code)
	--	@args: code: return code
	-- 	@rval:
	local function raise(code) 
		terminate(false, code) 
	end


	-- 	@name: assert
	-- 	@desc: execute rpc yieldable lua code from file
	-- 	@args: file: lua code to be executed
	-- 	@rval: return value which specified in yue.exit
	local function assert(expr, message)
		if expr then 
			return
		else
			if not message then message = 'assertion fails!' end
			print(message) 
			debug.traceback()
			raise(message)
		end
	end


	-- 	@name: yue.exec
	-- 	@desc: execute rpc yieldable lua code from file
	-- 	@args: file: lua code to be executed
	-- 	@rval: return value which specified in yue.exit
	m.exec = function(file)
		local f,e = loadfile(file)
		if f then 
			return m.run(f) 
		else 
			error(e) 
		end
	end


	-- 	@name: yue.run
	-- 	@desc: execute rpc yieldable lua function 
	-- 	@args: f function to be executed
	-- 	@rval: return value which specified in yue.exit
	local env = setmetatable({
			['assert'] = assert,
			['error'] = raise,
			['sleep'] = yue.core.sleep,
			['try'] = yue.core.try,
			['exit'] = exit,
		}, {__index = _G});
	m.run = function(f)
		result.ok = true
		result.code = nil
		alive = true
		m.mode('normal')
		-- isolation
		setfenv(f, env)
		y.resume(y.newthread(f))
		while alive do 
			y.poll()
		end
		m.mode('sync')
		print(result.ok, result.code)
		if result.ok then 
			return result.code 
		else 
			error(result.code) 
		end
	end
	
	return m
end)()



-- @module_name:	yue.util
-- @desc:	utility module
yue.util = (function()
	local m = {}
	m.time = y.util.time
	m.net = y.util.net
	m.ssh = function (cmdl)
		return y.util.proc.exec(cmdl)
	end
	m.sht = (function ()
		return setmetatable({}, {
			__newindex = function (tbl, key, val)
				local shm = y.util.shm
				local name = '__sht_' .. key
				local pname = name .. '*'
				local vars = (type(val) == 'table' and val[1] or val)
				local ctor = (type(val) == 'table' and val[2] or nil)
				local cdecl = string.format(
					'typedef struct { %s } %s;', vars, name) 
				yue.ffi.cdef(cdecl)
				local buffer,exist = shm.insert(key, yue.ffi.sizeof(name))
				if not exist then
					ctor(yue.ffi.cast(pname, buffer))
				end
				-- print(cdecl, name, yue.ffi.sizeof(name))
				rawset(tbl, key, setmetatable({ 
					__p = buffer,
					__locked = false
				},{
					lock = function(self, func)
						shm.wrlock(key)
						self.__locked = true
						if type(func) == 'function' then
							func(self)
							self.unlock(key)
						end
					end,
					unlock = function(self)
						shm.unlock(key)
						self.__locked = false
					end,
					__index = function (t, k) 
						local lp = t.__locked and shm.fetch(key) or shm.rdlock(key)
						local v = (yue.ffi.cast(pname, lp))[k]
						shm.unlock(key)
						return v
					end,
					__newindex = function (t, k, v) 
						local lp = t.__locked and shm.fetch(key) or shm.wrlock(key)
						local o = yue.ffi.cast(pname, lp)
						o[k] = v
						shm.unlock(key)
						return v
					end
				}))
				return tbl[key]
			end
		})
	end)() 
	return m
end)()



-- @module_name:	yue.paas
-- @desc:	features for running yue servers as cluster
yue.paas = (function(config)
	local m = {}
	local _mcast_addr = 'mcast://' .. config.mcast_addr
	local _ctor = function (localaddr, master_node)
		-- initialize shared data
		yue.util.sht.master = { 
			string.format([[
				unsigned char f_busy, n_nodes, polling, padd;
				char *nodes[%s];
				void *timer;
			]], config.required_master_num),
			function (t)
				t.f_busy = 0
				t.n_nodes = 0
				t.polling = 0
			end
		}
		local t = yue.util.sht.master
		t.busy = function (self, ...)
			if select('#', ...) > 0 then
				self.f_busy = select(1, ...)
			end
			return self.f_busy
		end
		t.enough = function(self)
			return #(self.n_nodes) >= config.required_master_num
		end
		t.add = function (self, addr)
			self:lock()
			if t.n_nodes < config.required_master_num then
				t.nodes[t.n_nodes] = addr
				t.n_nodes = t.n_nodes + 1
			end
			self:unlock()
			return self:enough()
		end
		t.connection = function (self, ...)
			return self:enough() and y[self.nodes[1]] or nil
		end
		-- shared data will be updated by global tick callback
		yue.core.tick.push(function ()
			if t:busy() then return end
			if not t:enough() then
				try(function()
					t:busy(true)
					m.finder.timed_search(5.0, 
						localaddr, master_node):callback(
						function (ok, addr, nodes)
							if ok then
								yue.hspace:add(nodes) 
								if t:add(addr) then
									t:busy(false)
								end
							else
								t:busy(false)
								y.yield()
							end
						end)
				end,
				function () -- catch
					t:busy(false)
				end,
				function () -- finally
				end)
			end
		end)
		return t
	end
	
	
	-- 	@name: yue.paas.finder
	-- 	@desc: mcast session to search for master nodes
	m.finder = yue.core.open(_mcast_addr)


	-- 	@name: yue.paas.as_master
	-- 	@desc: tell yue.paas to act as master node (thus, it responds to multicast from worker)
	-- 	@args: config:	port = master node port num (you should listen this port also)
	-- 	@rval: return yue.paas itself
	m.as_master = function(listener, ifname)
		local localaddr = listener:localaddr(ifname or 'eth0')
		m.mcast_listener = yue.core.listen(_mcast_addr)
		m.mcast_listener.namespace.search = function (addr, master_node)
			if not master_node then
				yue.hspace:add(addr)
			end
			return localaddr, yue.hspace:nodes()
		end
		m.master = _ctor(localaddr, true)
		return m
	end


	-- 	@name: yue.paas.as_worker
	-- 	@desc: tell yue.paas to act as worker node (thus, it responds to multicast from worker)
	-- 	@args: config:	port = master node port num (you should listen this port also)
	-- 	@rval: return yue.paas itself
	m.as_worker = function(listener, ifname)
		m.master = _ctor(listener:localaddr(ifname or 'eth0'), false)
		return m
	end


	-- 	@name: yue.paas.wait
	-- 	@desc: wait yue.paas initialization or configure callback
	--			note that this can only be called in coroutine. otherwise sleep fails. 
	-- 	@args: timeout: how many time wait initialization
	--		   callback: callback function when initialization done (optional)
	-- 	@rval: true if success false otherwise
	m.wait = function (self, timeout, callback)
		local start = yue.util.time.now() 
		local ok = true
		while not self.master:enough() do
			yue.core.sleep(0.5) -- sleep 500 ms
			if (yue.util.time.now() - start) > timeout then
				ok = false
				break
			end				
		end
		if callback then 
			callback(ok, yue.masters)
		end
		return ok
	end

	yue.paas = m
	return m
end)



-- @module_name:	yue.uuid
-- @desc:	generate id which is unique through all menber of yue cluster
yue.uuid = (function()
	-- return y.uuid
end)()



-- @module_name:	yue.hspace
-- @desc:	consistent hash which provides actor group from UUID
yue.hspace = (function()
	-- return y.hspace
end)()

return yue

