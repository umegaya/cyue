local lib = package.loaded.libyue or require('libyue')
local ffi = require('ffi')
local string = require('string')
local bit = require('bit')
local ok, r = pcall(require, 'debugger')
local dbg = ok and r or function () end
local version = (function ()
	return jit and jit.version or _VERSION
end)()
local yue = {} --it will be module table

ffi.cdef[[
	typedef struct lua_State lua_State;
	typedef lua_State *vm_t;
	typedef void *emitter_t;
	typedef unsigned int flag_t;
	typedef const char *method_t;
	typedef struct {
		int wblen, rblen;
		int timeout;
	} option_t;

	emitter_t 	(yue_emitter_new)();
	void		(yue_emitter_refer)(emitter_t);
	void		(yue_emitter_unref)(emitter_t);
	void		(yue_emitter_close)(emitter_t);
	void		(yue_emitter_bind)(vm_t, emitter_t);
	void		(yue_emitter_wait)(vm_t, emitter_t);

	emitter_t	(yue_socket_new)(const char *addr, option_t *opt);
	void		(yue_socket_bind)(vm_t, emitter_t);
	void		(yue_socket_wait)(vm_t, emitter_t);
	void		(yue_socket_call)(vm_t, flag_t, emitter_t, method_t);

	emitter_t	(yue_listener_new)(const char *addr, option_t *opt);
	void		(yue_socket_bind)(vm_t, emitter_t);
	void		(yue_socket_wait)(vm_t, emitter_t);

	emitter_t	(yue_signal_new)(int signo);
	void		(yue_signal_bind)(vm_t, emitter_t);
	void		(yue_signal_wait)(vm_t, emitter_t);

	emitter_t	(yue_timer_new)(double start, double intv);
	void		(yue_timer_bind)(vm_t, emitter_t);
	void		(yue_timer_wait)(vm_t, emitter_t);

	emitter_t	(yue_thread_new)(const char *, const char *);
	void		(yue_thread_bind)(vm_t, emitter_t);
	void		(yue_thread_wait)(vm_t, emitter_t);
	void		(yue_thread_call)(vm_t, flag_t, emitter_t, method_t);
	
]]
local clib = ffi.load('yue')
-- TODO: decide more elegant naming rule
-- TODO: not efficient if # of emittable objects is so many (eg, 1M). should shift it to C++ code?
local namespaces__ = lib.namespaces
local objects__ = lib.objects
local peer__ = {}

local log = {
	debug = (yue.mode == 'debug' and (function (...) print(...) end) or (function (...) end)),
	info = function (...) print(...) end,
	error = function (...) print(...) end,
	fatal = function (...) print(...) end,
}

local yue_mt = (function ()
	local create_callback_list = (function ()
		local mt = {
			push = function (t, cb) 
				if not t.head then
					t.head = { cb }
					t.tail = t.head
				else
					t.head = { cb, next = t.head }
				end
				return t
			end,
			append = function (t, cb)
				if not t.tail then
					t.head = { cb }
					t.tail = t.head
				else
					t.tail.next = { cb }
					t.tail = t.tail.next
				end
				return t
			end,
			__call = function (t, ...) 
				local c,r = t.head,nil
				while c do 
					r,c = c[1](...),c.next
					if not r then return r end
				end
				return r
			end,
			pop = function (t, cb)
				local c = t.head
				while true do
					local nc = c.next
					if not nc then
						return nil
					end
					if cb == nc[1] then
						c.next = nc.next
						if nc == t.tail then
							assert(not nc.next)
							t.tail = c
						end	
						return nc
					end
					c = nc
				end
			end,
		}
		mt.__index = mt
		return function ()
			return setmetatable({}, mt)
		end
	end)()
	
	local fetcher = function(t, k, local_call)
		if type(k) ~= 'string' then
			log.debug('non-string method:', k)
			return rawget(r, k)
		end
		-- TODO: use string.find make below faster?
		local kl, c, b, r, sk = #k, 0, nil, t, ''
		while c < kl do
			b = string.char(string.byte(k, c + 1))
			c = (c + 1)
			if b == '.' then
				r = rawget(r, sk)
				if not r then return nil end
				sk = ''
			elseif (not local_call) and #sk == 0 and b == '_' then
				-- attempt to call protected method
				-- log.debug('attempt to call protected method',local_call,sk,b)
				return nil -- function(...) error(k .. ' not found') end
			else
				sk = (sk .. b)
			end
		end
		-- log.debug('fetcher finished', rawget(r, sk))
		return rawget(r, sk)
	end
	
	local import = function (ns, src)
		if type(src) == 'string' then
			local f,e = loadfile(src)
			if not f then error(e) 
			else 
				setfenv(f, ns)
				f() 
			end
		elseif type(src) == 'table' then
			for k,v in pairs(src) do
				if type(v) == 'string' then
					ns:import(v)
				elseif type(v) == 'function' then
					rawset(ns, k, v)
				end
			end
		else
			error('invalid src:' .. type(src))
		end
		return ns
	end
	
	local create_namespace = (function ()
		local fallback_fetch = function (t, k, local_call)
			local r = fetcher(t, k, local_call)
			if r then 
				t[k] = r 
				return r
			else
				return _G[k]
			end
		end
		local newindex = function (t, k, v)
			if type(v) == 'function' and string.sub(k, 1, 2) == '__' then
				rawset(t, k, create_callback_list():push(v))
			else
				rawset(t, k, v)
			end
		end
		local mt = {
			protect = {
				__index = function (t, k)
					return fallback_fetch(t, k, false)
				end,
				__newindex = newindex,
			},
			raw = {
				__index = function (t, k)
					return fallback_fetch(t, k, true)
				end,
				__newindex = newindex,
			},
		}
		
		return function (kind)
			return setmetatable({}, mt[kind])
		end
	end)()
	
	local emitter_mt = (function ()
		local constant = {
			events = {
				ID_TIMER	= 1,
				ID_SIGNAL	= 2,
				ID_SESSION	= 3,
				ID_LISTENER	= 4,
				ID_PROC		= 5,
				ID_EMIT		= 6,
				ID_FILESYSTEM	= 7,
				ID_THREAD	= 8,
			},
			flags = {
				ASYNC 		= 0x00000001,
				TIMED 		= 0x00000002,
				MCAST 		= 0x00000004,
				PROTECTED 	= 0x00000008,
			},
		}
		local create_procs = (function ()
			local parse = function (name)
				local prefixes = {
					async_ = constant.flags.ASYNC,
					timed_ = constant.flags.TIMED,
					mcast_ = constant.flags.MCAST,
				}
				local flag, index, match = 0, 0, false
				repeat
					for k,v in pairs(prefixes) do
						index = string.find(name, k) 
						if index then
							name = string.sub(name, index)
							flag = bit.bor(flag, v)
							match = true
						end
					end
				until not match 
				if string.find(name, '_') == 1 then
					flag = bit.bor(flag, constant.flags.PROTECTED)
				end
				return name,flag
			end
			local async_launch = function (t, ft, ...)
				ft(t.call(t.__ptr, t.__flag, t.__name, ...))
			end
			local mcast_launch = function (t, ft, ...)
				t.call(t.__ptr, t.__flag, ft, t.__name, ...)
			end
			local method_mt = {
				__call = function (t, ...)
					if bit.band(t.__flag, constant.flags.ASYNC) ~= 0 then
						local ft = yue.future()
						yue.fiber(async_launch):run(t, ft, ...)
						return ft
					elseif bit.band(t.__flag, constant.flags.MCAST) ~= 0 then
						local ft = yue.future()
						yue.fiber(mcast_launch):run(t, ft, ...)
						return ft
					end
					log.debug('call', t.__name, t.__ptr)
					local r = {t.call(t.__ptr, t.__flag, t.__name, ...)} --> yue_emitter_call
					if r[1] then -- here cannot use [a and b or c] idiom because b sometimes be falsy.
						return unpack(r, 2)
					else
						error(r[2])
					end
				end,
				__pack = function (t, pbuf)
					return lib.yue_pbuf_write(pbuf, t.__name)
				end,
			}
			local method_index = function (t, k)
				local pk,f = parse(k)
				local mt = getmetatable(t)
				log.debug('method_index', k, mt, method_mt)
				if mt == method_mt then	-- method object (element of emitter object or method object)
					t[k] = setmetatable(
						{ __ptr = t.__ptr, __flag = f, __name = (t.__name .. "." .. pk), call = t.call}, method_mt)
				else	-- emitter object
					t[k] = setmetatable({ 
						__ptr = t.__emitter.__ptr, __flag = f, __name = pk, call = t.__emitter.__call}, method_mt)
				end
				return t[k]
			end
			method_mt.__index = method_index
			local proc_mt = { __index = method_index }
			return function (e)
				assert(e.__ptr)
				return setmetatable({ __emitter = e }, proc_mt)
			end
		end)()
		
		-- version not support table __gc
		print('underlying Lua version:', version)
		return {
			__new = lib.yue_emitter_new,
			__flags = constant.flags,
			__events = constant.events,
			__create = function (mt,...)
				return mt.__new(...), create_namespace('protect')
			end,
			__procs = function (emitter)
				return create_procs(emitter)
			end,
			__ctor = function (ptr, mt, namespace, ...)
				local r = setmetatable({ __ptr = ptr, 
						namespace = namespace, __bounds = {0}, 
					}, mt)
				r.procs = mt.__procs(r)
				return r
			end,
			__activate = function (self, ptr, namespace)
				log.debug('__activate', ptr)
				namespaces__[ptr] = namespace
				objects__[ptr] = self
				lib.yue_emitter_refer(ptr)
				lib.yue_emitter_open(ptr)
			end,
			__unref = function (self)
				log.debug('unref', self.__ptr)
				namespaces__[self.__ptr] = nil
				objects__[self.__ptr] = nil
				lib.yue_emitter_unref(self.__ptr)
				assert(self.procs.__emitter == self)
				self.procs.__emitter = nil	-- resolve cyclic reference
				self.__ptr = nil
			end,
			close = function (self)
				if self.__ptr then -- if falsy, already closed
					lib.yue_emitter_close(self.__ptr)
				end
			end,
			unbind = function (self, events, fn)
				log.debug('unbind call')
				local t = type(events)
				if t == 'string' then
					events = { [events] = fn }
				elseif t == 'table' then
					if fn then
						local tmp = {}
						for k,v in ipairs(events) do
							tmp[v] = fn
						end
						events = tmp
					end
				else
					error('invalid events type:', t)
				end
				for k,v in pairs(events) do
					local key = '__' .. k
					if self.namespace[key] then
						self.namespace[key]:pop(v)
					end
				end
			end,
			bind = function (self, events, fn, timeout)
				local t,f,ef = type(events),0,{}
				if t == 'string' then
					events = { [events] = fn }
				elseif t == 'table' then
					if fn then
						local tmp = {}
						for k,v in ipairs(events) do
							tmp[v] = fn
						end
						events = tmp
					end
				else
					error('invalid events type:', t)
				end
				for k,v in pairs(events) do
					local key = '__' .. k
					if not self.namespace[key] then
						self.namespace[key] = create_callback_list()
					end
					if self.__flags[k] then
						if bit.band(self.__bounds[1], self.__flags[k]) == 0 then 
							f = bit.bor(f, self.__flags[k])
						end
					elseif not self.__bounds[k] then
						table.insert(ef, k)
						self.__bounds[k] = true
					end
					self.namespace[key]:push(v)
				end
				log.debug('bind', t, f, #ef)
				if not timeout then
					timeout = 0
				end
				if f ~= 0 then
					self.__bounds[1] = bit.bor(self.__bounds[1], f)
					lib.yue_emitter_bind(self.__ptr, self.__event_id, f, timeout)
				end
				if #ef > 0 then
					for k,v in ipairs(ef) do
						lib.yue_emitter_bind(self.__ptr, constant.events.ID_EMIT, v, timeout)
					end
				end
			end,
			wait = function (self, events, timeout)
				local t = type(events)
				if t == 'string' then
					if self.__flags[events] then
						lib.yue_emitter_wait(self.__ptr, self.__event_id, self.__flags[events], timeout)
					else
						lib.yue_emitter_wait(self.__ptr, constant.events.ID_EMIT, events, timeout)
					end
				else
					error('invalid events type:', t)
				end
			end,
			emit = function (self, ...)
				lib.yue_emitter_emit(self.__ptr, ...)
			end,
		}
	end)() 
	local extend = function (base, ext)
		local result = {}
		for k,v in pairs(base) do
			result[k] = v
		end
		for k,v in pairs(ext) do
			result[k] = v
		end
		result.__index = result -- refer themselves for missing symbol
		return result
	end
	local metatables = {
		emitter = 	emitter_mt,
		timer = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_TIMER,
						__flags = { tick = 0x00000001 },
						__new = lib.yue_timer_new,
						__procs = function (emitter) return nil end,
					}),
		signal = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_SIGNAL,
						__flags = { signal = 0x00000001 },
						__new = lib.yue_signal_new,
						__procs = function (emitter) return nil end,
					}),
		open = 		extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_SESSION,
						__flags = { 
							open = 0x00000002,
							establish = 0x00000004,
							data = 0x00000008,
							close = 0x00000010,
						},
						__create = function (self,...)
							local args = {...}
							if type(args[1]) == 'string' then
								-- normal creation (given: 1:hostname, 2:symbols(string/table), 3:option(table))
								local ns = create_namespace('protect')
								if args[2] then import(ns, args[2]) end
								return lib.yue_socket_new(...), ns
							elseif type(args[1]) == 'table' then
								-- server socket creation (given: 1:listener & 2:socket)
								return args[2],namespaces__[args[1].__ptr]
							elseif type(args[1]) == 'userdata' then
								-- stream peer creation (given: 1:socket(ptr))
								assert(false)
								return args[1],(namespaces__[lib.yue_socket_listener(args[1])] or create_namespace('protect'))
							else
								error('invalid socket args')
							end
						end,
						__call = function (ptr, flags, ...)
							if not lib.yue_socket_connected(ptr) then
								local args = {...}
								if bit.band(flags, emitter_mt.__flags.TIMED) ~= 0 then
									if bit.band(flags, emitter_mt.__flags.MCAST) ~= 0 then
										-- ptr, flags, future, method_name, timeout_sec, arg1, ...
										lib.yue_socket_connect(ptr, args[5])
									else
										-- ptr, flags, method_name, timeout_sec, arg1, ...
										lib.yue_socket_connect(ptr, args[4])
									end
								else
									-- ptr, flags, method_name, arg1, ...
									lib.yue_socket_connect(ptr)
								end
							end
							return lib.yue_socket_call(ptr, flags, ...)
						end,
						__sys_open = function (socket)
							print('open', socket:addr(), socket.__ptr)
						end,
						__sys_close = function (socket)
							print('close', socket:addr(), socket.__ptr)
							if socket:closed() then -- client connection can reconnect.
								socket:__unref()
							end
						end,
						__initializing = {},
						__ctor = function (ptr, mt, namespace, ...)
							local args = {...}
							local no_cache = (#args >= 3 and (type(args[1]) == 'string') and args[3].no_cache)
							if (not no_cache) and mt.__initializing[ptr] then 
								mt.__initializing[ptr]:wait('initialized')
								return objects__[ptr] -- here mt.__initializing[ptr] already set nil (see __activate)
							end
							local r = emitter_mt.__ctor(ptr, mt, namespace, ...)
							if not no_cache then
								mt.__initializing[ptr] = r
							end
							if not r:listener() then
								namespace.accept__ = r:__make_accept_closure(r) -- bind r as upvalue
							end
							r:bind({ -- caution, it yields (means next fiber start execution)
								open = mt.__sys_open, 
								close = mt.__sys_close
							})
							return r
						end,
						__activate = function (self, ptr, namespace)
							log.debug('__activate: ', ptr, objects__[ptr])
							if not objects__[ptr] then
								emitter_mt.__activate(self, ptr, namespace)
								if self.__initializing[ptr] then
									self.__initializing[ptr] = nil
									self:emit('initialized')
								end
							end
						end,
						__accept_processor = function (self, socket, r)
							log.debug('accept processor')
							local aw = self.namespace.__accept
							socket:grant()
							if aw then
								if not aw(socket, r) then
									socket:close()
								end
							end
						end,
						__make_accept_closure = function (self, socket)
							return function (r)
								return self:__accept_processor(socket, r)
							end
						end,
						grant = function (self)
							if not self:authorized() then 
								if not lib.yue_socket_valid(self.__ptr) then
									self:wait('open')
								end
								lib.yue_socket_grant(self.__ptr)
							end
						end,
						authorized = function (self)
							return lib.yue_socket_authorized(self.__ptr)
						end,
						addr = function (self)
							return lib.yue_socket_address(self.__ptr)
						end,
						listener = function (self)
							return lib.yue_socket_listener(self.__ptr)
						end,
						closed = function (self)
							return lib.yue_socket_closed(self.__ptr)
						end,
					}),
		peer = 		extend(emitter_mt, {
						__create = function (self,...)
							return nil,create_namespace('protect')
						end,
						__ctor = function (dummy, mt, namespace, ...)
							local ptr,refp,type = lib.yue_peer()
							log.debug('peer', ptr,refp,type)
							return emitter_mt.__ctor(ptr, mt.__mt[type], namespace)
						end,
						__activate = function (ptr, namespace)
							-- force do nothing
						end,
						-- TODO: for datagram peer we need to call this even for 5.1 compatible lua. but how?
						__gc = function (self)
							log.debug('peer gc')
							lib.yue_peer_close(self.__ptr)
							peer__[self.__ptr] = nil
						end,
					}),
		listen =	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_LISTENER,
						__flags = { acpt = 0x00000001 },
						__new = lib.yue_listener_new,
						__procs = function (emitter) return nil end,
						__acpt = function (listener, socket_ptr)
							local s = yue.open(listener, socket_ptr)
							log.debug(s, socket_ptr)
							assert(s == objects__[socket_ptr])
							local aw = listener.namespace.__accept
							log.debug('__acpt: ', aw)
							if aw then
								local ok, r = pcall(aw, s) 
								if ok and r then
									s:grant()
									s.procs.accept__(r)
								else
									s:close()
								end
							else
								log.debug('auth b4:', s:authorized())
								s:grant()
								log.debug('auth:', s:authorized())
								s.procs.accept__(true)
								log.debug('accept__ finish')
							end
						end,
						__create = function (self,...)
							local args = {...}
							if type(args[1]) == 'string' then
								-- normal creation (given: 1:hostname 2:symbols(table/string) 3:option(table))
								local ns = create_namespace('protect')
								log.debug('symbols', args[2])
								if args[2] then import(ns, args[2]) end
								return lib.yue_listener_new(...),ns
							else
								error('invalid listener args')
							end
						end,
						__ctor = function (ptr, mt, namespace, ...)
							local r = emitter_mt.__ctor(ptr, mt, namespace, ...)
							r:bind('acpt', r.__acpt)
							return r
						end,
					}),
		fs =		extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_FILESYSTEM,
						__flags = lib.yue_fs_event_flags,
						__new = lib.yue_fs_new,
						__procs = function (emitter) return nil end,
					}),
		thread = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_THREAD,
						__flags = { join = 0x00000001 },
						__create = function (self,...)
							local args = {...}
							if type(args[1]) == 'string' then
								return lib.yue_thread_new(...),create_namespace('raw')
							elseif type(args[1]) == 'userdata' then
								-- from thread.find
								return args[1],(namespaces__[args[1]] or create_namespace('raw'))
							else
								error('invalid thread arg')
							end
						end,
						__call = lib.yue_thread_call,
						count = function ()
							return lib.yue_thread_count()
						end,
						find = function (name)
							return yue.thread(lib.yue_thread_find(name))
						end,
						exit = function (self, result)
							self.result = result
							self:close()
						end
					}),
	}
	metatables.peer.__mt = {
		stream = extend(metatables.peer, { __call = lib.yue_socket_call, __gc = function() end }),
		thread = extend(metatables.peer, { __call = lib.yue_thread_call, __gc = function() end }),
		datagram = extend(metatables.peer, { __call = lib.yue_peer_call, __close = metatables.peer.__gc }),
	}
	
	
	lib.__finalizer = function ()
		for k,v in pairs(namespaces__) do
			lib.yue_emitter_unref(k)
		end
	end

	return {
		__call = function(t, ...) 
			local mt = metatables[t.__type]
			local ptr,namespace = mt:__create(...)
			if objects__[ptr] then -- cached object may return same pointer as prviously allocated
				return objects__[ptr]
			end
			local r = mt.__ctor(ptr, mt, namespace, ...)
			mt.__activate(r, ptr, namespace)
			return r
		end
	}
end)()



setmetatable((function () 
		-- debugger (if available)
		yue.dbg = dbg
		-- non-emittabble objects
		yue.fiber = (function () 
			local fiber_mt = (function ()
				local runner = function (ft, fn, ...)
					ft(pcall(fn, ...))
				end
				local r = {
					run = function (self, ...)
						local ft = yue.future()
						lib.yue_fiber_run(self.__ptr, runner, ft, self.__f, ...)
						return ft
					end
				}
				r.__index = r
				return r
			end)()
			return setmetatable({},{
				__call = function (t, f)
					return setmetatable({__f = f, __ptr = lib.yue_fiber_new()}, fiber_mt)
				end
			})
		end)()
		yue.future = (function ()
			local future_mt = (function ()
				return {
					on = function (self, cb)
						table.insert(self.__receiver, cb)
					end,
					__call = function (self, ok, ...)
						for k,v in ipairs(self.__receiver) do
							v(ok, ...)
						end
					end,
				}
			end)()
			future_mt.__index = future_mt
			return function ()
				return setmetatable({ __receiver = {} }, future_mt)
			end
		end)()
		yue.try = function (block) 
			local ok, r = pcall(block[1])
			if ok then
				block.finally()
				return r
			else
				ok, r = pcall(block.catch, r)
				block.finally()
				if ok then
					return r
				else
					error(r) -- throw again
				end
			end
		end
		-- initialize util module from lib object
		yue.util = lib.util
		-- initialize yue running mode (debug/release)
		yue.mode = lib.mode
		
		
		-- parse and initialize argument
		yue.args = {
			boot = nil,
			launch = nil,
			launch_timeout = 3,
			wc = (function ()
				local c = 0
				if jit.os == 'Windows' then return 1 end
				if jit.os == 'OSX' then
					local ok, r = pcall(io.popen, 'sysctl -a machdep.cpu | grep thread_count')
					if not ok then return 1 end
					c = 1
					for l in r:lines() do 
						c = l:gsub("machdep.cpu.thread_count:%s?(%d+)", "%1")
					end
				else
					local ok, r = pcall(io.popen, 'cat /proc/cpuinfo | grep processor')
					if not ok then return 1 end
					for l in r:lines() do c = c + 1 end
					r:close()
				end
				return c
			end)()
		}
		for i,a in ipairs(lib.args) do
			-- generally options are {option name}={value}
			local pos = string.find(a, '=')
			if not pos then
				-- if no =, it suppose to be launch option's filename.
				yue.args.launch = a
			else -- otherwise it forms like key=value --> yue.args[key] = value
				yue.args[string.sub(a, 1, pos - 1)] = string.sub(a, pos + 1)
			end
		end
		
		
		return yue
	end)(), {
	__index = function(t, k)
		t[k] = setmetatable({ __type = k, }, yue_mt)
		return t[k]
	end
})

-- initialize thread variables
if lib.thread then
	yue.thread.current = yue.thread(lib.thread)
end

return yue
-- end of yue.lua
