m.__service = (function()
	local l = _M.core.listen('tcp://0.0.0.0:' .. NODE_SERVICE_PORT)
	l.namespace:import({
		__accepted = function (conn)
			local host = _M.util.addr.host(conn.__addr)
			-- notify cluster that this host added.
			m.
		end,
		__closed = function (conn)
		end,
		broadcast = function (event, hostname, conn)
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
		end
	})
	return l
end)()

m.__nodeset = (function()
	--[[------------------------------------------------------------------------------
		constant
	--]]------------------------------------------------------------------------------
	local NODE_SERVICE_PORT = 18888	
	local EVENT = {
		CREATE_CHILD = 1,
		CREATE_BROTHER = 2,
		ADD_CHILD = 3,
		ADD_BROTHER = 4,
		ADD_PARENT = 5,
		ADD_MEMBER = 6,
	}
	local EVENT_ROUTE = {
		[EVENT.CREATE_CHILD] = {
			parent = EVENT.ADD_MEMBER,
			brother = EVENT.ADD_CHILD,
			child = EVENT.ADD_MEMBER,
		},
		[EVENT.CREATE_BROTHER] = {
			parent = EVENT.ADD_MEMBER,
			brother = EVENT.ADD_BROTHER,
			child = EVENT.ADD_PARENT,
		},		
		[EVENT.ADD_CHILD] = {
			parent = nil,
			brother = nil,
			child = EVENT.ADD_MEMBER,
		},
		[EVENT.ADD_BROTHER] = {
			parent = nil,
			brother = nil,
			child = EVENT.ADD_MEMBER,
		},
		[EVENT.ADD_PARENT] = {
			parent = nil,
			brother = nil,
			child = EVENT.ADD_MEMBER,
		},
		[EVENT.ADD_MEMBER] = {
			parent = EVENT.ADD_MEMBER,
			brother = EVENT.ADD_MEMBER,
			child = EVENT.ADD_MEMBER,
		},
	}
	
	
	
	--[[------------------------------------------------------------------------------
		private function
	--]]------------------------------------------------------------------------------
	local connect = function (hostname)
		return _M.core.open('tcp://' .. hostname .. ':' .. NODE_SERVICE_PORT)
	end
	
	

	return setmetatable({
		__parents = {}, 	-- if this yue node created by node module, connection object which connect to 
							-- creator's address (given by -n option of yue).
		__myhost = nil,		-- this node's hostname decided by node factory given by -n option of yue
		__members = {},		-- all nodes which created by entire cluster
		__children = {},	-- nodes which created by this node.
		__brothers = {},	-- if this node died, one of these node failover it
		__mutex = nil,
	}, {
		initialize = function (self, hostname, parents_hostname)
			self.__myhost = hostname
			for k,v in pairs(parents_hostname) do
				self:on_add(EVENT.ADD_PARENT, v, connect(v))
			end
		end,
		on_add = function (self, event, hostname, conn)
			local h = hostname
			if event == EVENT.ADD_PARENT then
				self.__parents[h] = conn
				return self.__parents[h].register(self.__myhost)
			elseif event == EVENT.ADD_BROTHER then
				self.__brothers[h] = conn
				return self.__brothers[h].register(self.__myhost)
			elseif event == EVENT.ADD_CHILD then
				self.__children[h] = conn
				return h
			elseif event == EVENT.ADD_MEMBER then
				self.__children[h] = conn
				return h
			end
		end,
		on_remove = function (self, hostname)
		end,
		broadcast = function (self)
			
		end,
	})
end)()

	_M.node = (function()
		local m = {
			-- initialized by yue command line args
			__parents = {}, 	-- if this yue node created by node module, connection object which connect to 
								-- creator's address (given by -n option of yue).
			__myhost = nil,		-- this node's hostname decided by node factory given by -n option of yue
			-- initialized by node module itself
			__factory = nil,
			__callback = nil,
			__service = nil,
			__members = {},		-- all nodes which created by entire cluster
			__children = {},	-- nodes which created by this node.
			__brothers = {},	-- if this node died, one of these node failover it
			__mutex = nil,
		}
		local NODE_SERVICE_PORT = 18888
		
		--[[----------------------------------------------------------------------------------
		-- private function
		--]]----------------------------------------------------------------------------------
		-- lock/unlock node object --
		local critcal_section = function (blocks, ...)
			local proc,finally = blocks,nil
			if type(blocks) == 'table' then
				proc,finally = blocks[1],blocks.finally
			end
			m.__mutex:lock()
			local ok,r = pcall(proc, ...)
			if ok and finally then finally() end
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
				return _trim_node_list(m.__brothers), 
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
				string.format('popen://ssh %s killall yue', hostname)
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
