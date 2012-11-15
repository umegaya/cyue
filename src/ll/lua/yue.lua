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
-- TODO: decide more elegant naming rule (after C++ code compilation passes)
-- TODO: not efficient if # of emittable objects is so many (eg, 1M). should shift it to C++ code?
local namespaces__ = lib.namespaces
local objects__ = lib.objects

local yue_mt = (function ()
--		local error_mt = _G.error_mt
	local create_namespace = (function ()
		local fetcher = function(t, k, local_call)
			if type(k) ~= 'string' then
				print('non-string method:', k)
				return rawget(r, k)
			end
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
					-- print('attempt to call protected method',local_call,sk,b)
					return nil -- function(...) error(k .. ' not found') end
				else
					sk = (sk .. b)
				end
			end
			-- print('fetcher finished', rawget(r, sk))
			return rawget(r, sk)
		end
		
		local add_symbol = function (ns, k, v)
			if type(v) == 'function' then
				local ok,v = pcall(setfenv, v, ns)
				if not ok then error(v) end
			end
			rawset(ns, k, v)
			return v
		end
		
		local import = function (ns, src)
			if type(src) == 'string' then
				local f,e = loadfile(src)
				if not f then error(e) 
				else 
					setfenv(f, ns.__symbols)
					f()
				end
			elseif type(src) == 'table' then
				for k,v in pairs(src) do
					if type(v) == 'string' then
						ns:import(v)
					elseif type(v) == 'function' then
						add_symbol(ns.__symbols, k, v)
					end
				end
			else
				error('invalid src:' .. type(src))
			end
			return ns
		end
	
		local mt = {
			protect = {
				__index = function(t, k)
					local r = fetcher(t.__symbols, k, false)
					if r then t[k] = r end
					return r
				end,
			},
			raw = {
				__index = function(t, k)
					local r = fetcher(t.__symbols, k, true)
					if r then t[k] = r end
					return r
				end,
			},
		}
		
		return function (kind)
			return setmetatable({__symbols = setmetatable({}, {__index = _G}), import = import}, mt[kind])
		end
	end)()
	
	local emitter_mt = (function ()
		local flags = {
			ASYNC 		= 0x00000001,
			TIMED 		= 0x00000002,
			MCAST 		= 0x00000004,
			PROTECTED 	= 0x00000008,
		}
		local events = {
			ID_TIMER	= 1,
			ID_SIGNAL	= 2,
			ID_SESSION	= 3,
			ID_LISTENER	= 4,
			ID_PROC		= 5,
			ID_EMIT		= 6,
			ID_FILESYSTEM	= 7,
			ID_THREAD	= 8,
		}
		local parse = function (name)
			local prefixes = {
				async_ = flags.ASYNC,
				timed_ = flags.TIMED,
				mcast_ = flags.MCAST,
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
				flag = bit.bor(flag, flags.PROTECTED)
			end
			return name,flag
		end
		local async_launch = function (t, ft, ...)
			ft(t.__mt.__call(t.__ptr, t.__flag, t.__name, ...))
		end
		local mcast_launch = function (t, ft, ...)
			t.__mt.__call(t.__ptr, t.__flag, ft, t.__name, ...)
		end
		local method_mt = {
			__call = function (t, ...)
				if bit.band(t.__flag, flags.ASYNC) ~= 0 then
					local ft = yue.future()
					yue.fiber(async_launch):run(t, ft, ...)
					return ft
				elseif bit.band(t.__flag, flags.MCAST) ~= 0 then
					local ft = yue.future()
					yue.fiber(mcast_launch):run(t, ft, ...)
					return ft
				end
				local r = {t.__mt.__call(t.__ptr, t.__flag, t.__name, ...)} --> yue_emitter_call
				print('result:', r[1], r[2], unpack(r, 2))
				if r[1] then -- here cannot use [a and b or c] idiom because b sometimes be falsy. (false or nil)
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
			-- print('method_index', k, mt, method_mt)
			if mt == method_mt then	-- method object (element of emitter object or method object)
				t[k] = setmetatable({ __ptr = t.__ptr, __flag = f, __name = (t.__name .. "." .. pk), __mt = t.__mt}, method_mt)
			elseif mt[k] then -- pre-defined symbol of emitter object (return it as it is)
				return mt[k]
			else	-- emitter object
				t[k] = setmetatable({ __ptr = t.__ptr, __flag = f, __name = pk, __mt = mt}, method_mt)
			end
			return t[k]
		end
		method_mt.__index = method_index

		-- version not support table __gc
		print('underlying Lua version:', version)
		return {
			__index = method_index,
			__new = lib.yue_emitter_new,
			__flags = flags,
			__events = events,
			__create = function (mt,...)
				return mt.__new(...), create_namespace('protect')
			end,
			__ctor = function (ptr, mt, namespace, ...)
				return setmetatable({ __ptr = ptr, namespace = namespace }, mt)
			end,
			__close = function (self)
				self:__unref()
				lib.yue_emitter_close(self.__ptr)
			end,
			__unref = function (self)
				namespaces__[self.__ptr] = nil
				objects__[self.__ptr] = nil
				lib.yue_emitter_unref(self.__ptr)
			end,
			__bind = function (self, events, fn)
				print('bind call')
				local t,f,ef = type(events),0,{}
				if t == 'string' then
					if not self.namespace['__' .. events] then
						if self.__flags[events] then
							f = bit.bor(f, self.__flags[events])
						else
							table.insert(ef, events)
						end
					end
					self.namespace['__' .. events] = fn
				elseif t == 'table' then
					for k,v in pairs(events) do
						if not self.namespace['__' .. events] then
							if self.__flags[events] then
								f = bit.band(f, self.__flags[v])
							else
								table.insert(ef, v)
							end
						end
						self.namespace['__' .. v] = fn
					end
				else
					error('invalid events type:', t)
				end
				print('call_bind', t, f, ef)
				if f ~= 0 then
					lib.yue_emitter_bind(self.__ptr, self.__event_id, f)
				end
				if #ef > 0 then
					for k,v in ipairs(ef) do
						lib.yue_emitter_bind(self.__ptr, events.ID_EMIT, v)
					end
				end
			end,
			__wait = function (self, events, timeout)
				local t = type(events)
				if t == 'string' then
					if self.__flags[events] then
						lib.yue_emitter_wait(self.__ptr, self.__event_id, self.__flags[events], timeout)
					else
						lib.yue_emitter_wait(self.__ptr, events.ID_EMIT, events, timeout)
					end
				else
					error('invalid events type:', t)
				end
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
		return result
	end
	local metatables = {
		emitter = 	emitter_mt,
		timer = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_TIMER,
						__flags = { tick = 0x00000001 },
						__new = lib.yue_timer_new,
					}),
		signal = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_SIGNAL,
						__flags = { signal = 0x00000001 },
						__new = lib.yue_signal_new,
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
								-- normal creation (given: 1:hostname & 2:option)
								return lib.yue_socket_new(...),create_namespace('protect')
							elseif type(args[1]) == 'table' then
								-- server socket creation (given: 1:listener & 2:socket)
								return args[2],namespaces__[args[1].__ptr]
							elseif type(args[1]) == 'userdata' then
								-- stream peer creation (given: 1:socket(ptr))
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
						__open = function (socket)
							print('open', socket:__addr())
						end,
						__close = function (socket)
							print('close', socket:__addr())
							if socket:__listener() then
								print('server: auto unref')
								socket:__unref()
							end
						end,
						__ctor = function (ptr, mt, namespace, ...)
							local r = emitter_mt.__ctor(ptr, mt, namespace, ...)
							if not r:__listener() then
								namespace.accept__ = r:__make_accept_closure(r) -- bind r as upvalue
							end
							r:__bind('open', mt.__open)
							r:__bind('close', mt.__close)
							return r
						end,
						__grant = function (self)
							if not self:__authorized() then 
								if not lib.yue_socket_valid(self.__ptr) then
									self:__wait('open')
								end
								lib.yue_socket_grant(self.__ptr)
							end
						end,
						__authorized = function (self)
							return lib.yue_socket_authorized(self.__ptr)
						end,
						__addr = function (self)
							return lib.yue_socket_address(self.__ptr)
						end,
						__listener = function (self)
							return lib.yue_socket_listener(self.__ptr)
						end,
						__accept_processor = function (self, socket, r)
							local aw = self.namespace.__accept
							socket:__grant()
							if aw then
								if not aw(socket, r) then
									socket:__close()
								end
							end
						end,
						__make_accept_closure = function (self, socket)
							return function (r)
								return self:__accept_processor(socket, r)
							end
						end,
					}),
		open_peer = extend(emitter_mt, {
						__create = function (self,...)
							local args = {...}
							return args[1],create_namespace('protect')
						end,
						__call = lib.yue_peer_call,
						__gc = function (self)
							namespaces__[self.__ptr] = nil
							objects__[self.__ptr] = nil
							lib.yue_peer_close(self.__ptr)
						end,
					}),
		listen =	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_LISTENER,
						__flags = { acpt = 0x00000001 },
						__new = lib.yue_listener_new,
						__acpt = function (listener, socket_ptr)
							print('__acpt called')
							local s = yue.open(listener, socket_ptr)
							local aw = listener.namespace.__accept
							if aw then
								local ok, r = pcall(aw, s) 
								if ok and r then
									s:__grant()
									s.accept__(r)
								else
									s:__close()
								end
							else
								print('auth b4:', s:__authorized())
								s:__grant()
								print('auth:', s:__authorized())
								s.accept__(true)
								print('accept__ finish')
							end
						end,
						__ctor = function (ptr, mt, namespace, ...)
							local r = emitter_mt.__ctor(ptr, mt, namespace, ...)
							r:__bind('acpt', r.__acpt)
							return r
						end,
					}),
		fs =		extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_FILESYSTEM,
						__flags = lib.yue_fs_event_flags,
						__new = lib.yue_fs_new,
					}),
		thread = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_THREAD,
						__flags = { join = 0x00000001 },
						__create = function (self,...)
							local args = {...}
							if type(args[1]) == 'string' then
								return lib.yue_thread_new(...),create_namespace('raw')
							elseif type(args[1]) == 'userdata' then
								return args[1].__ptr,(namespaces__[args[1].__ptr] or create_namespace('raw'))
							else
								error('invalid thread arg')
							end
						end,
						__call = lib.yue_thread_call,
					}),
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
			local result = mt.__ctor(ptr, mt, namespace, ...)
			lib.yue_emitter_refer(ptr)
			namespaces__[ptr] = namespace
			objects__[ptr] = result
			lib.yue_emitter_open(ptr)
			return result
		end
	}
end)()



return setmetatable((function () 
		-- debugger (if available)
		yue.dbg = dbg
		-- non-emittabble objects
		yue.fiber = (function () 
			local fiber_mt = (function ()
				return {
					run = lib.yue_fiber_run,
				}
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
			return setmetatable({ __receiver = {} }, future_mt)
		end)()
		yue.peer = function ()
			local type,ptr = lib.yue_peer()
			print('peer', type, ptr, objects__[ptr])
			return objects__[ptr] or yue[type](ptr)
		end
		
		
		-- parse and initialize argument
		yue.args = {
			boot = nil,
			launch = nil,
			launch_timeout = 1,
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
		for k,v in pairs(yue.args) do
			-- print(k, v)
		end
		return yue
	end)(), {
	__index = function(t, k)
		t[k] = setmetatable({ __type = k, }, yue_mt)
		return t[k]
	end
})
-- end of yue.lua
