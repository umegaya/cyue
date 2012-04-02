if not yue then require('libyue') end
local _Y = yue
yue = nil
return (function ()
	local _M = {}
	--[[----------------------------------------------------------------------------------
	-- 
	--	@module:	yue.ffi
	--	@desc:		the luajit ffi
	--
	--]]----------------------------------------------------------------------------------
	_M.ffi = (function()
		local ok,r = pcall(require,'ffi')
		return ok and r or nil
	end)()
	local C = _M.ffi and _M.ffi.C or nil



	--[[----------------------------------------------------------------------------------
	-- 	@module:	yue.ld
	-- 	@desc:		decide calling function from rpc command and caller context
	--]]----------------------------------------------------------------------------------
	_M.ld = (function()
		local m = {
			_add = function(f)
				local org = _G[_Y.ldname]
				_G[_Y.ldname] = function(s, c)
					return f(s, c) or org(s, c)
				end
				return org
			end,
			ctx = -1	-- current processed namespace id
		}
	
		--[[----------------------------------------------------------------------------------
		--	@name: 	local fetcher
		--	@desc: 	helper function to fetch actual object from rpc method name
		--			it resolve a.b.c => _G[a][b][c], and if symbol contains _ on its top,
		--			it cannot call from remote node (that is, when local_call == false)
		--	@args: 	t:	table to be resolve symbol
		--			k:	rpc method name
		--			local_call:	if true, it can call protected method 
								which name is like _funcname. otherwise cannot.
		--]]----------------------------------------------------------------------------------
		local fetcher = function(t, k, local_call)
			local kl, c, b, r, sk = #k, 0, nil, t, ''
			print(kl,c,b,r,'['..sk..']',k,local_call)
			while c < kl do
				b = string.char(string.byte(k, c + 1))
				c = (c + 1)
				if b == '.' then
					r = rawget(r, sk)
					if not r then 
						return nil -- function(...) error(k .. ' not found') end
					end
					sk = ''
				elseif (not local_call) and #sk == 0 and b == '_' then
					-- attempt to call protected method
					-- print('attempt to call protected method',local_call,sk,b)
					return nil -- function(...) error(k .. ' not found') end
				else
					sk = (sk .. b)
				end
			end
			-- print('fetcher finished', rawget(r, sk))
			return rawget(r, sk);
		end
		
		-- namespace helpers		
		local add_symbol = function (t, k, v)
			if type(v) == 'function' then
				local ok,v = pcall(setfenv, v, t)
				-- print('add_sym', ok, t, k, v)
				if not ok then error(v) end
			end
			rawset(t, k, v)
			return v
		end
		local importer = function (self, file)
			if type(file) == 'string' then
				local f,e = loadfile(file)
				if not f then error(e) 
				else 
					setfenv(f, self.__symbols)
					f()
				end
			elseif type(file) == 'table' then
				for k,v in pairs(file) do
					if type(v) == 'string' then
						self:import(v)
					elseif type(v) == 'function' then
						add_symbol(self.__symbols, k, v)
					end
				end
			else
				error('invalid type:' .. type(file))
			end
			return self
		end
		local remote_namespace_mt = {
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
		
		--[[----------------------------------------------------------------------------------
		-- @name: create_namespace
		-- @desc: create namespace, which provide accessible symbol list for each listener 
		-- 			(thus, if listner fd = 7 defines func_fd7 in its namespace, 
		-- 			it cannot found in rpc call through listner fd = 8 and so on.)
		--			it has __g as environment of calling function of create_namespace			
		-  @args: fd:	listener fd
		--			globals: table which contains global symbols. 
		--]]----------------------------------------------------------------------------------		
		local namespaces = {}
		m.create_namespace = function(fd, globals)
			namespaces[fd] = setmetatable({
						__fd = fd, 
						__symbols = setmetatable({}, 
							{ 
								__index = setmetatable( { yue = _M }, { __index = globals } ),  
								__newindex = add_symbol,
							}),
						import = importer,
					}, remote_namespace_mt)
			return namespaces[fd]
		end
		m.resolve_symbol_from_fd = function(fd, name)
			return m.resolve_symbol(namespaces[fd], name)
		end
		m.resolve_symbol = function(ns, name)
			return fetcher(ns.__symbols, name, true)
		end
				
		-- method loader set up main
		local local_namespaces = setmetatable({
				__symbols = setmetatable({
					invoke_from_namespace = function (addr, f, ...)
						return namespaces[_Y.listeners(addr)][f](...)
					end,
					remove_conn = function (addr)
						_M.core.remove_conn_cache(addr)
					end,
				}, {__index = _G})
			}, local_namespace_mt)
		_G[_Y.ldname] = function(s, c)
			m.ctx = c
			if c >= 0 then 
				return namespaces[c][s] 
			else 
				return local_namespaces[s]
			end
		end
		--	unpacker of yue module obj (correspond to _M.packer)
		_M.__unpack = function(rb)
			local p,size = _Y.read(rb)
			local t = _M.ffi.cast("yue_object_t*", p)
			if t.type == C.YUE_OBJECT_METHOD then
				return _G[_Y.ldname](_M.ffi.string(t.data,size - 1), m.ctx)
			elseif t.type == C.YUE_OBJECT_ACTOR then
				error("currently actor unpack not supported for security reason")
			else
				error("invalid object type:" .. t.type)
			end
		end	
		
		return m
	end)()


	--[[----------------------------------------------------------------------------------
	--
	-- @module: yue.pack
	-- @desc: 	yue object packer
	--
	--]]----------------------------------------------------------------------------------
	_M.pack = (function()
		local m = {}
		_M.ffi.cdef[[
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
			return _Y.pack.method(self.__m,wb)
		end
		m.actor = function(self,wb)
			return _Y.pack.actor(self.__c,wb)
		end
		return m
	end)()


	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.core
	-- @desc:	core feature of yue which mainly provided by yue.so
	--
	--]]----------------------------------------------------------------------------------
	_M.core = (function () 
		-- 	copy core symbols to module table
		local m = {
			poll = _Y.poll,
			connect = _Y.connect,
			listen = _Y.listen,
			timer = _Y.timer,
			stop_timer = _Y.stop_timer,
			sleep = _Y.sleep,
			die = _Y.stop,
			error = _Y.error,
			threads = {},
			connections = {},
			command_line = _Y.command_line,
		}
		
		--[[----------------------------------------------------------------------------------
		-- @name: local protect
		-- @desc: helper function to create protected rpc connection 
		-- 		(using same mechanism as lua_*callk in lua 5.2)
		-- @args: p:	conn (which is created by _Y.open) to be protected
		--			host: hostname to open this connection.
		--]]----------------------------------------------------------------------------------
		local protect_mt = { 
			__call = function(f,...)
				local r = {f.__m(...)}
				print(r, _Y.error(r[1]))
				if not _Y.error(r[1]) then return unpack(r)
				else 
					-- print(_M.dev.inspect(r, 0))
					error(r[1]) 
				end
			end,
			__index = function(t, k)
				local r = setmetatable(
					{ __m = t.__m[k], __pack = _M.pack.method }, 
					getmetatable(t))
				t[k] = r
				return r
			end,
		}
		local function protect(p)
			local c = m.connections[p.__addr]
			if c then return c end
			c = setmetatable(
				{ __c = p, __pack = _M.pack.actor, __addr = p.__addr },
				{
					__index = function(t, k)
						local r = setmetatable(
							{ __m = t.__c[k], __pack = _M.pack.method }, 
							protect_mt)
						t[k] = r
						return r
					end,
				})
			m.connections[p.__addr] = c
			return c
		end

		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.remove_conn_cache
		--	@desc: receive host address and remove all related caches
		-- 	@args: addr: connection to remove cache
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		m.remove_conn_cache = function(addr)
			local c = _Y[addr]
			if not c then
				if m.connections[addr] then
					c = m.connections[addr].__c
				end
			end
			-- print(addr, " will cleanup", c)
			-- call __fin method (it remove entry to registry and unregister itself from session watcher)
			if c then c:__fin() end
			-- remove connection cache on this VM			
			_Y[addr] = nil
			m.connections[addr] = nil
			-- force VM to collect these memory
			collectgarbage("collect")
		end

		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.iterate_thread
		--	@desc: do something for all VM thread
		-- 	@args: 
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		-- init threads table
		for idx = 1,_Y.thread_count,1 do
			m.threads[idx] = protect(_Y:accepted(idx - 1))
			assert(m.threads[idx].__addr == (idx - 1)) 
		end	
		local each_thread = function (fn)
			for idx = 1,_Y.thread_count,1 do
				fn(m.threads[idx])
			end
		end
		m.each_thread = each_thread
	
		--[[----------------------------------------------------------------------------------
		--	@name: yue.tick
		--	@desc: callback per process (any thread have possibility to call it)
		-- 	@args: 
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		_Y.registry.tick = (function ()
			return setmetatable({
					__fn = {},
					push = function(self, f)
						table.insert(self.__fn, f)
					end,
					pop = function(self, f)
						for k,v in pairs(self.__fn) do
							if v == f then
								return table.remove(self.__fn, k)
							end
						end
						return nil
					end
				}, {
					__call = function (t, ...)
						for k,v in pairs(t.__fn) do
							v()
						end
					end,
				})
		end)()
		_M.tick = _Y.registry.tick
		local gc_count = 0
		_M.tick:push(function()
			gc_count = (gc_count + 1)
			if gc_count >= 10 then
				-- print("GC:", collectgarbage("count"), "Kbyte used")
				gc_count = 0
			end
		end)
		
		--[[----------------------------------------------------------------------------------
		--	@name: registry.yue
		--	@desc: yue object which can be accesible from C code
		-- 	@args: 
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		_Y.registry.yue = _Y		
	
		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.try
		--	@desc: provide exception handling
		-- 	@args: main: code block (function) to be executed
		--		   catch: error handler
		--		   finally: code block which is called anytime when 
		--					main logic execution finished
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
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
			rawset(env, "error", _G.error)
			
			-- call body func
			local ok,r = pcall(main)
			
			-- back environment to original error func
			rawset(env, "error", oerror)
			if not ok and not catch(r) then
				finally()
				oerror(r)
			else
				finally()
			end
		end
	
		
		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.open
		--	@desc: open remote node
		-- 	@args: host: remote node address
		--			opt: connect option {
						symbols: initial namespace symbols
							these are special kind of symbol.
							__closed: close watcher (when this connection closed, it calls)
							__accepted: accept_watcher (when this connection accepted to server, it calls)
					}
		-- 	@rval: yue connection
		--			which namespaces globals inherit of caller of yue.core.open
		--]]----------------------------------------------------------------------------------
		m.open = function(host, opt)
			-- search from cache
			local c = m.connections[addr]
			if c then return c end
			-- seperate symbols from option (it is not used for network module)
			local symbols = opt and opt.symbols or nil
			if opt then opt.symbols = nil end
			c = protect(_Y.connect(host, opt))
			print('yue.core.open host:', c.__addr)
			-- when server side accept success, server call this
			local mt = getmetatable(c.__c)
			-- __accepted, __closed need to put metatable so that it can be referred from C code.
			mt.__accepted = function (conn)
				-- normal close watcher (called from local)
				mt.__closed = opt and 
					(symbols and symbols.__closed or nil) 
					or nil
				-- use c instead of c (because conn will be GC'ed)
				local p = c.__c
				-- give environment of this function as globals
				c.namespace = _M.ld.create_namespace(p:__fd(), getfenv(1))
				if symbols then
					c.namespace:import(symbols)
				end
				c.namespace.accepted__ = function (r)
					-- print('client accepted__ will call', getfenv(), c.namespace)
					local aw = _M.ld.resolve_symbol(c.namespace, "__accepted")
					if aw then
						-- print('client __accepted will call')
						if not aw(c, r) then
							p:__close()
							return nil
						end
					else
						-- print('client __accepted not exist')
					end
					p:__permit_access()
					return nil
				end
				mt.__accepted = nil -- remove this function (please GC)
			end
			-- __accepted inherit global environment which calls yue.core.open
			setfenv(mt.__accepted, getfenv(2))
			return c
		end
	
		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.close
		--	@desc: close connection which is created by yue.core.open, peer, accepted
		-- 	@args: 
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		m.close = function(conn)
			conn.__c:__close()
		end

	
		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.peer
		--	@desc: retrieve peer connection which execute current rpc
		-- 	@args:
		-- 	@rval: yue connection
		--]]----------------------------------------------------------------------------------
		m.peer = function()
			return protect(_Y:peer())
		end


		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.accepted
		--	@desc: retrieve accepted connection from address
		-- 	@args: addr 
		-- 	@rval: yue connection
		--]]----------------------------------------------------------------------------------
		m.accepted = function(addr)
			local c = m.connections[addr]
			if c then return c end
			return protect(_Y:accepted(addr))
		end


		--[[----------------------------------------------------------------------------------
		--	@name: _G[_Y.watcher]
		--	@desc: global accept watcher
		-- 	@args: afd: listner socket (key for namespace)
		--			conn: connection which related to accepted fd
		--			accept: true if accepted otherwise this conn have closed (thus no writable)
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		-- TODO: share values with C++ code (by using FFI feature)
		local ACCEPTED, CLOSED, FINALIZED = 3, 6, 8 -- see session.h 31-41
		_G[_Y.watcher] = function (conn, afd, event)
			if event == ACCEPTED then
				local w = _M.ld.resolve_symbol_from_fd(afd, "__accepted")
				local pconn = protect(conn)
				if w then 
					local ok,r = pcall(w, pconn)
					print("w result=", ok, r)
					if ok and r then
						conn:__permit_access()
						pconn.accepted__(r)
					else
						conn:__close()
					end
				else
					conn:__permit_access()
					pconn.accepted__(true)
				end
			elseif event == CLOSED then
				local w = _M.ld.resolve_symbol_from_fd(afd, "__closed")
				if w then w(protect(conn)) end
			elseif event == FINALIZED then
				local a = conn.__addr
				each_thread(function (t)
					t.timed_notify_remove_conn(30, a):callback(function() end)
				end)
			end
		end
		--[[----------------------------------------------------------------------------------
		--	@name: yue.core.listen
		--	@desc: listen on specified port
		-- 	@args: host: address to listen to
		--			opt: listen option
		--			file: if it specified, load from symbol and add them to namespace.
		-- 	@rval: listner table
		--			which namespaces globals inherit of caller of yue.core.listen
		--]]----------------------------------------------------------------------------------
		m.listen = function(host, opt)
			local fd = _Y.listen(host, opt)
			if type(fd) ~= 'number' or fd < 0 then return nil end
			return { 
				-- give environment of caller of listen as globals
				namespace = _M.ld.create_namespace(fd, getfenv(2)),
				bindaddr = host, 
				localaddr = function(self,ifname)
					return (_M.util.net.localaddr(fd, ifname) .. ':' .. _M.util.addr.port(self.bindaddr))
				end
			}
		end
		
		
		return m
	end)()



	--[[----------------------------------------------------------------------------------
	--
	-- @module_name:	yue.dev
	-- @desc:	api for creating lua module which can run cooperate with yue
	--
	--]]----------------------------------------------------------------------------------
	_M.dev = (function()
		local m = {
			read = _Y.read,
			write = _Y.write,
			yield = _Y.yield,
			socket = _Y.socket,
		}
		local rcb = function(sk, s, func)
			while true do
				local r = sk:read_cb(func(s))
				if r == -1 then
					print('rcb: yield', sk:fd())
					_Y.yield()
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
		m.inspect = function (obj, depth)
			local buff = ''
			if type(obj) == 'table' then
				local out = ''
				for i = 1,(1 + depth),1 do
					out = (out .. '    ')
				end
				buff = (buff .. '{\n')
				for k,v in pairs(obj) do
					buff = (buff .. out)
					buff = (buff ..'[')
					buff = (buff .. m.inspect(k, depth + 1))
					buff = (buff ..']')
					buff = (buff .. ' => ')
					buff = (buff .. m.inspect(v, depth + 1))
					buff = (buff .. ',\n')
				end
				local out = ''
				for i = 1,depth,1 do
					out = (out .. '    ')
				end
				buff = (buff .. (out .. '}'))
			else
				if type(obj) == 'nil' then
					buff = (buff .. '(nil)')
				elseif type(obj) == 'function' then
					buff = (buff .. '(function)')
				elseif type(obj) == 'boolean' then
					buff = (buff .. (obj and 'true' or 'false'))
				else
					buff = (buff .. obj)
				end
			end
			return buff
		end
		return m
	end)()



	--[[----------------------------------------------------------------------------------
	--
	-- @module_name:	yue.client
	-- @desc:	api for access yue server from luajit console or other lua client program
	--
	--]]----------------------------------------------------------------------------------
	_M.client = (function ()
		local m = {
			mode = _Y.mode
		}
	
		--	tables which stores yue execution result
		local result = {}
		local alive;
	
	
		--[[----------------------------------------------------------------------------------
		--	@name: terminate
		--	@desc: finish lua rpc yieldable program with return code
		-- 	@args: ok: execution success or not
		--		   code: return code which returned by yue.run or yue.exec
		-- 	@rval: 
		--]]----------------------------------------------------------------------------------
		local function terminate(ok, code) 
			alive = false
			result.code = code
			result.ok = ok
			_Y.exit()
		end
	
	
		--[[----------------------------------------------------------------------------------
		--	@name: exit
		--	@desc: equivalent to yue.terminate(false, code)
		--	@args: code: return code
		-- 	@rval:
		--]]----------------------------------------------------------------------------------
		local function exit(code) 
			terminate(true, code) 
		end
	
	
		--[[----------------------------------------------------------------------------------
		--	@name: raise
		--	@desc: equivalent to yue.exit(false, code)
		--	@args: code: return code
		-- 	@rval:
		--]]----------------------------------------------------------------------------------
		local function raise(code) 
			terminate(false, code) 
		end
	
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: assert
		-- 	@desc: execute rpc yieldable lua code from file
		-- 	@args: file: lua code to be executed
		-- 	@rval: return value which specified in yue.exit
		--]]----------------------------------------------------------------------------------
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
	
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.exec
		-- 	@desc: execute rpc yieldable lua code from file
		-- 	@args: file: lua code to be executed
		-- 	@rval: return value which specified in yue.exit
		--]]----------------------------------------------------------------------------------
		m.exec = function(file)
			local f,e = loadfile(file)
			if f then 
				return m.run(f) 
			else 
				error(e) 
			end
		end
	
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.run
		-- 	@desc: execute rpc yieldable lua function 
		-- 	@args: f function to be executed
		-- 	@rval: return value which specified in yue.exit
		--]]----------------------------------------------------------------------------------
		m.run = function(f)
			result.ok = true
			result.code = nil
			alive = true
			m.mode('normal')
			-- isolation
			local env = setmetatable({
				['assert'] = assert,
				['error'] = raise,
				['sleep'] = _M.core.sleep,
				['try'] = _M.core.try,
				['exit'] = exit,
			}, {__index = _G});
			setfenv(f, env)
			_Y.resume(_Y.newthread(f))
			print('mainloop', env)
			while alive do 
				_Y.poll()
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



	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.util
	-- @desc:	utility module
	--
	--]]----------------------------------------------------------------------------------
	_M.util = (function()
		local m = {}
		m.time = _Y.util.time
		m.net = _Y.util.net
		m.addr = {
			proto = function(addr) 
				return addr:sub(addr:find('://'))
			end,
			host = function(addr)
				local s,e = addr:find('://')
				if not s then return nil end
				local a = addr:sub(e)
				return a:sub(0, a:find(':'))
			end,
			port = function(addr)
				local s,e = addr:find('://')
				if not s then return nil end
				local a = addr:sub(e)
				return a:sub(a:find(':'))
			end,
		}
		
		--[[----------------------------------------------------------------------------------
		-- 	@name: util.sht
		-- 	@desc: provide shared memory between all VM thread 
		--]]----------------------------------------------------------------------------------
		m.sht = (function ()
			return setmetatable({}, {
				__newindex = function (tbl, key, val)
					local shm = _Y.util.shm
					local name = '__sht_' .. key
					local pname = name .. '*'
					local vars = (type(val) == 'table' and val[1] or val)
					local ctor = (type(val) == 'table' and val[2] or nil)
					local cdecl = string.format(
						'typedef struct { %s } %s;', vars, name) 
					_M.ffi.cdef(cdecl)
					local buffer,exist = shm.insert(key, _M.ffi.sizeof(name))
					if (not exist) and ctor then
						ctor(_M.ffi.cast(pname, buffer))
					end
					-- print(cdecl, name, _M.ffi.sizeof(name))
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
							local v = (_M.ffi.cast(pname, lp))[k]
							shm.unlock(key)
							return v
						end,
						__newindex = function (t, k, v) 
							local lp = t.__locked and shm.fetch(key) or shm.wrlock(key)
							local o = _M.ffi.cast(pname, lp)
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



	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.node
	-- @desc:	create new compute node in IaaS env and use it as programming resource
	--
	--]]----------------------------------------------------------------------------------
	_M.node = (function()
		local m = {
			-- initialized by yue command line args
			__parent = nil, 	-- if this yue node created by node module, connection object which connect to 
								-- creator's address (given by -n option of yue).
			__myhost = nil,		-- this node's hostname decided by node factory given by -n option of yue
			-- initialized by node module itself
			__factory = nil,
			__callback = nil,
			__service = nil,
			__clan = {},		-- all nodes which created by entire cluster
			__children = {},	-- nodes which created by this node.
			__uncles = {},		-- if parent died, next parent choosed from
			__brothers = {},		-- if this node died, one of these node failover it
			__mutex = nil,
		}
		local NODE_SERVICE_PORT = 18888
		
		-- private function for node.initialize -- 
		-- lock/unlock node object --
		local critcal_section = function (proc, finally, ...)
			m.__mutex:lock()
			local ok,r = pcall(proc, ...)
			if ok and type(finally) == 'function' then finally() end
			m.__mutex:unlock()
		end
		-- create node service --
		local node_service_procs = {
			_trim_node_list = function (list)
				local r = {}
				for k,v in pairs(list) do
					r[k] = v.spec
				end
				return r
			end,
			register = function (hostname)
				local spec = m.__children[hostname].spec
				local raddr = _M.core.peer().__addr
				broadcast(m.__myhost, 'add', hostname, spec, raddr)
				return _trim_node_list(m.__uncles), 
					_trim_node_list(m.__clan), 
					_trim_node_list(m.__children)
			end,
			broadcast = function (from, event, hostname, ...)
				critical_section(function (...)
					-- notice node creation/destroy to
					--  *parent node
					if from and m.__parent then 
						-- add_uncle not propagete to parent
						if event == 'add_uncle' then event = 'add' end
						m.__parent.notify_broadcast(m.__myhost, event, hostname, ...)
					end
					-- *child node
					for k,v in pairs(m.__children) do
						if not k == from then
							v.conn.notify_broadcast(nil, event, hostname, ...)
						end
					end
					-- *worker thread on same node
					for k,v in ipairs(_M.threads) do
						v.invoke_from_namespace(
							self.__service.bindaddr, 'node_callback', event, hostname, ...)
					end
				end, ...)
			end,
			_choose_stepfather = function ()
				for k,v in pairs(m.__uncles) do
					m.__uncles[k]= nil
					return k,v
				end
				return nil,nil
			end,
			node_callback = function (event, hostname, ...)
				if event == 'add' then
					if not m.__clan[hostname] then
						m.__clan[hostname] = {}
					end
					m.__clan[hostname].spec = select(..., 0)
					if select(..., '#') > 1 then
						m.__clan[hostname].conn = _M.core.accepted(select(..., 1))
						if m.__clan[hostname].conn then
							m.__children[hostname] = m.__clan[hostname]
							m.__clan[hostname] = nil
						end
					end
				elseif event == 'add_uncle' then
					if not m.__uncles[hostname] then
						m.__uncles[hostname] = {}
					end
					m.__uncles[hostname].spec = select(..., 0)
				elseif event == 'rm' then
					m.__clan[hostname] = nil
					m.__children[hostname] = nil
					m.__uncles[hostname] = nil
					if _M.util.host(m.__parent.__addr) == hostname then
						local next,hostname = _choose_stepfather()
						if next then
							next.conn = yue.core.open(
								'tcp://' .. hostname .. ':' .. M.util.addr.port(self.__parent.__addr))
							m.__parent = next
							m.register()
						else	-- if no parent, should die. something wrong.
							print('no parent, node(' .. m.__myhost .. ') suicide')
							_M.core.die()
						end
					end
				else
					assert(not ('invalid event:' .. event))
				end
				-- call callback function (in this function, actually connect and use them)
				if  m.__callback then
					m.__callback(event, hostname, ...)
				end
			end,
		}
		local create_node_service = function (port)
			local bind_addr = 'tcp://0.0.0.0:' .. port
			return _M.core.listen(bind_addr).namespace:import(node_service_procs)
		end
		m.init_with_command_line = function (p)
			m.__myaddr = p:sub(0, p:find('|'))
			m.__parent = _M.core.open(p:sub(p:find('|')), nil, node_service_procs)
			m.register_to_parent()
		end
		m.register_to_parent = function ()
			critcal_section(
				function ()
					if not m.__mutex.initialized == 0 then return end
					local u, c, ch = m.__parent.register(m.__myaddr)
					for idx,thrd in ipairs(_M.threads) do
						for k,v in pairs(u) do
							thrd.invoke_from_namespace(
								self.__service.bindaddr, 'node_callback', 'add_uncle', k, v)
						end
						for k,v in pairs(c) do
							thrd.invoke_from_namespace(
								self.__service.bindaddr, 'node_callback', 'add', k, v)
						end
						for k,v in pairs(ch) do
							thrd.invoke_from_namespace(
								self.__service.bindaddr, 'node_callback', 'add', k, v)
						end
					end
				end,
				function () 
					m.__mutex.initialized = 1
				end
			)
		end
 		--[[----------------------------------------------------------------------------------
		-- 	@name: node.initialize
		-- 	@desc: intialize node with selected node factory 
		-- 	@args: 	factory: factory module to use
						(currently node.factory.node_list, node.factory.rightscale)
		--			callback: function called when node is added to/deleted from cluster	
		--					- when added: func(node_address, spec)
		--					- when deleted: func(node_address)
		--			port: node_service listner port (default 18888)
		-- 	@rval: return true if success false otherwise
		--]]----------------------------------------------------------------------------------
		m.initialize = function (self, factory, callback, port)
			self.__factory = factory
			self.__callback = callback
			local p = (self.__parent and 
					M.util.addr.port(self.__parent.__addr) or 
					(port or NODE_SERVICE_PORT))
			print('node service: port = ', p)
			self.__service = create_node_service(p)
			_M.util.sht.__node = {
				"int initialized;",
				function (t) t.initialized = 0 end
			}
			self.__mutex = _M.util.sht.__node
			return true
		end

		-- private function for node.create
		local verify = function (option)
			return option.bootimg and option.role
		end
		--[[----------------------------------------------------------------------------------
		-- 	@name: node.create
		-- 	@desc: create node
		-- 	@args: option: {
					port = port to connect
					proto = proto to connect (default: tcp)
					bootimg = file to loaded on start up
					role = node's role (eg. database, frontend, etc...)
					uncle = if true, created node is failover node of myself
					yue = /path/to/yue
				}
		-- 	@rval: return true if success false otherwise
		--]]----------------------------------------------------------------------------------
		m.create = function (self, option)
			if not self.__factory then
				error('please initialize node module')
			end
			if not verify(option) then 
				error('option is not enough')
				return nil 
			end
			local hostname, spec = self.__factory:create(option)
			if not hostname then 
				error('node allocation fails')
				return nil 
			end
			-- boot yue server
			local port = (self.__port or 
				-- use setting if this node is root node
				(option.port or NODE_SERVICE_PORT)
			)
			critical_section(function ()
				for k,v in ipairs(_M.threads) do
					v.invoke_from_namespace(bind_addr, 'node_callback', 'add', hostname, spec)
				end
			end)
			_M.socket(
				string.format('popen://ssh %s %s %s %d -n=%s|%s',
					addr, 
					(option.yue or "yue"),
					option.bootimg, 
					(option.thn or -1), 
					hostname, 
					('tcp://' .. self.__service.localaddr))
			):try_read(function (sk) end)
			return hostname,spec
		end
		
		--[[----------------------------------------------------------------------------------
		-- 	@name: node.destroy
		-- 	@desc: destroy node
		-- 	@args:
		-- 	@rval: return true if success false otherwise
		--]]----------------------------------------------------------------------------------
		m.destroy = function (self, hostname)
			_M.socket(
				string.format('popen://ssh %s killall yue',
					hostname)
			):try_read(function (sk) end)
			self.__factory:destroy(hostname)
			node_service_procs.broadcast(m.__myaddr, 'rm', hostname)
		end
		
		-- node factory collections --
		m.factory = {
			node_list = {
				mt = {
					init = function (self, resource)
						if type(resource) == 'string' then
							self.__free:insert({addr = resource})
						elseif type(resource) == 'table' then
							for k,v in pairs(resource) do
								self:init(v)
							end
						elseif type(resource) == 'function' then
							local r = resource()
							while r do
								self:init(r)
								r = resource()	
							end
						end
						return self
					end,
					__alloc = function (self)
						for k,v in pairs(self.__free) do
							print(k, v)
							if v then 
								self.__free:remove(k)
								self.__used:insert(v)
								return v
							end 
						end
						return nil
					end,
					__free = function (self,n)
						for k,v in pairs(self.__used) do
							if v == n then
								self.__free:insert(v)
								self.__used:remove(k)
								return
							end
						end
						assert(false)
						return
					end,
					create = function (self, option)
						return self:__alloc(), option
					end,
					destroy = function (self, node)
						self:__free(node)
					end
				},
				callback = function (self, event, hostname)
					if event == 'add' or event == 'add_uncle' then
						for k,v in pairs(self.__free) do
							if v == hostname then
								self.__used:insert(v)
								self.__free:remove(k)
							end
						end
					elseif event == 'rm' then
						for k,v in pairs(self.__used) do
							if v == hostname then
								self.__free:insert(v)
								self.__used:remove(k)
							end
						end
					end
				end,
				new = function (self, resource)
					print('node_list new: size = ', #resource)
					local v = setmetatable({
									__free = setmetatable({}, { __index = table }),
									__used = setmetatable({}, { __index = table }),
								}, {__index = self.mt})
					return v:init(resource)
				end
			},
			rightscale = {
				mt = {
					init = function (self, resouce)
						assert(false)
						return self
					end,
					create = function (self, option)
						assert(false)
					end,
					destroy = function (self, node)
						assert(false)
					end
				},
				new = function (self, resource)
					local v = setmetatable({
									__free = {},
									__used = {}
								}, {__index = self.mt})
					return v:init(resource)
				end
			},	
		}
		return m
	end)()
	


	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.membership
	-- @desc:	features for running yue servers as cluster
	--
	--]]----------------------------------------------------------------------------------
	_M.membership = (function(config)
		local m = {}
		local _mcast_addr = 'mcast://' .. config.mcast_addr
		local _ctor = function (localaddr, master_node)
			-- initialize shared data
			_M.util.sht.master = { 
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
			local t = _M.util.sht.master
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
			_M.tick:push(function ()
				if t:busy() then return end
				if not t:enough() then
					try(function()
						t:busy(true)
						m.finder.timed_search(5.0, 
							localaddr, master_node):callback(
							function (ok, addr, nodes)
								if ok then
									_M.hspace:add(nodes) 
									if t:add(addr) then
										t:busy(false)
									end
								else
									t:busy(false)
									_Y.yield()
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
		
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.paas.finder
		-- 	@desc: mcast session to search for master nodes
		--]]----------------------------------------------------------------------------------
		m.finder = _M.core.open(_mcast_addr)
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.paas.as_master
		-- 	@desc: tell yue.paas to act as master node (thus, it responds to multicast from worker)
		-- 	@args: config:	port = master node port num (you should listen this port also)
		-- 	@rval: return yue.paas itself
		--]]----------------------------------------------------------------------------------
		m.as_master = function(listener, ifname)
			local localaddr = listener:localaddr(ifname)
			m.mcast_listener = _M.core.listen(_mcast_addr)
			m.mcast_listener.namespace.search = function (addr, master_node)
				if not master_node then
					_M.hspace:add(addr)
				end
				return localaddr, _M.hspace:nodes()
			end
			m.master = _ctor(localaddr, true)
			return m
		end
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.paas.as_worker
		-- 	@desc: tell yue.paas to act as worker node (thus, it responds to multicast from worker)
		-- 	@args: config:	port = master node port num (you should listen this port also)
		-- 	@rval: return yue.paas itself
		--]]----------------------------------------------------------------------------------
		m.as_worker = function(listener, ifname)
			m.master = _ctor(listener:localaddr(ifname), false)
			return m
		end
	
	
		--[[----------------------------------------------------------------------------------
		-- 	@name: yue.paas.wait
		-- 	@desc: wait yue.paas initialization or configure callback
		--			note that this can only be called in coroutine. otherwise sleep fails. 
		-- 	@args: timeout: how many time wait initialization
		--		   callback: callback function when initialization done (optional)
		-- 	@rval: true if success false otherwise
		--]]----------------------------------------------------------------------------------
		m.wait = function (self, timeout, callback)
			local start = _M.util.time.now() 
			local ok = true
			while not self.master:enough() do
				_M.core.sleep(0.5) -- sleep 500 ms
				if (_M.util.time.now() - start) > timeout then
					ok = false
					break
				end				
			end
			if callback then 
				callback(ok, _M.masters)
			end
			return ok
		end
	
		_M.paas = m
		return m
	end)


	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.uuid
	-- @desc:	generate id which is unique through all menber of yue cluster
	--
	--]]----------------------------------------------------------------------------------
	_M.uuid = (function()
		-- return _Y.uuid
	end)()



	--[[----------------------------------------------------------------------------------
	--
	-- @module:	yue.hspace
	-- @desc:	consistent hash which provides actor group from UUID
	--
	--]]----------------------------------------------------------------------------------
	_M.hspace = (function()
		-- return _Y.hspace
	end)()
	
	-- initialize yue from command line args
	for k,v in ipairs(_Y.command_line) do
		if v:sub(0, v:find('=')) == '-n' then
			local p = v:sub(v:find('='))
			_M.node.init_with_command_line(p)
		end
	end
	return _M
end)()

