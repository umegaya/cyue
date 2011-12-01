require('yue')
local y = yue
yue = {}

-- @module_name:	yue.ffi
-- @desc:	the luajit ffi
local ok,r = pcall(require,'ffi')
local C = nil
yue.ffi = nil
if ok then
	yue.ffi = r
	C = yue.ffi.C
end
	

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
		die = y.stop,
		error = y.error,
	}
	local function protect(p)
		local c = { conn = p }
		setmetatable(c, {
			__index = function(t, k)
				return function(...)
					local r = {t.conn[k](...)}
					if not y.error(r[1]) then return unpack(r)
					else error(r[1]) end
				end
			end
		})
		return c
	end


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
	--		   catch: error handler
	--		   finally: code block which is called antime when 
	--					main logic execution finished
	-- 	@rval: 
	m.open = function(host, opt)
		return protect(y.connect(host, opt))
	end
	

	return m
end)()



-- @module_name:	yue.dev
-- @desc:	api for creating lua module which can run cooperate with yue
yue.dev = (function()
	local m = {
		read = y.read,
		write = y.write,
		yield = y.yield
	}
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
		yue.dev.yield()
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
	m.run = function(f)
		result.ok = true
		result.code = nil
		alive = true
		m.mode('normal')
		-- isolation
		setfenv(f, setmetatable({
			['assert'] = assert,
			['error'] = raise,
			['try'] = yue.core.try,
			['exit'] = exit,
		}, {__index = _G}))
		y.resume(y.newthread(f))
		while (alive) do 
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
	m.sht = (function ()
		return setmetatable({}, {
			__newindex = function (tbl, key, val)
				local shm = y.util.shm
				local name = '__sht_' .. key
				local pname = name .. '*'
				local cdecl = string.format(
					'typedef struct { %s } %s;', val, name) 
				yue.ffi.cdef(cdecl)
				print(cdecl, name, yue.ffi.sizeof(name))
				rawset(tbl, key, setmetatable({ 
					__p = shm.insert(key, yue.ffi.sizeof(name)),
					__locked = false
				},{
					lock = function(self)
						shm.wrlock(key)
						self.__locked = true
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
		yue.util.sht.master = string.format([[
			unsigned char f_busy, n_nodes, padd[2];
			char *nodes[%s];
			void *timer;
		]], config.required_master_num)
		
		local t = yue.util.sht.master
		t.f_busy = 0
		t.n_nodes = 0
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
			return self:enough() and y[self.nodes[0]] or nil
		end
		t:lock()
		if not t.timer then 
			t.timer = yue.core.timer(function ()
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
		end
		t:unlock()
		return t
	end
	
	
	-- 	@name: yue.paas.finder
	-- 	@desc: mcast session to search for master nodes
	m.finder = yue.core.open(_mcast_addr)


	-- 	@name: yue.paas.as_master
	-- 	@desc: tell yue.paas to act as master node (thus, it responds to multicast from worker)
	-- 	@args: config:	port = master node port num (you should listen this port also)
	-- 	@rval: return yue.paas itself
	m.as_master = function(port)
		local localaddr = yue.util.net.localaddr .. ':' .. port
		m.mcast_listener = yue.core.listen(_mcast_addr)
		m.mcast_listener:namespace().search = function (addr, master_node)
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
	m.as_worker = function(port)
		local localaddr = yue.util.net.localaddr .. ':' .. port
		m.master = _ctor(localaddr, false)
		return m
	end


	-- 	@name: yue.paas.wait
	-- 	@desc: wait yue.paas initialization or configure callback
	-- 	@args: timeout: how many time wait initialization
	--		   callback: callback function when initialization done (optional)
	-- 	@rval: true if success false otherwise
	m.wait = function (self, timeout, callback)
		local start = yue.util.time.now() 
		local ok = true
		while not self.master:enough() do
			yue.core.poll()
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
