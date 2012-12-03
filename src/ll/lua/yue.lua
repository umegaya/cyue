local lib,is_server_process = (function ()
	if package.loaded.libyue then
		return package.loaded.libyue,true
	else
		return require('libyue'),false
	end
end)()
local ffi = require('ffi')
local string = require('string')
local bit = require('bit')
local coroutine = require('coroutine')
local dbg = (function ()
	local ok, r = pcall(require, 'debugger')
	return ok and r or function () end
end)()
local pprint = (function ()
	local ok, r = pcall(require, 'serpent')
	return ok and r or _G.print
end)()
local version = (jit and jit.version or _VERSION)
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
local ok,clib = pcall(ffi.load, 'yue')
-- TODO: decide more elegant naming rule
-- TODO: not efficient if # of emittable objects is so many (eg, 1M). should shift it to C++ code?
local namespaces__ = lib.namespaces
local objects__ = lib.objects
local peer__ = {}

local log = {
	debug = (lib.mode == 'debug' and (function (...) print(...) end) or (function (...) end)),
	info = function (...) print(...) end,
	error = function (...) print(...) end,
	fatal = function (...) print(...) end,
}

local yue_mt = (function ()
	local create_callback_list = (function ()
		local mt = {
			push = function (t, cb) 
				if not t:search(cb) then 
					table.insert(t, cb)
					log.debug('cbl:push', t, cb, #t)
				end
				return t
			end,
			append = function (t, cb)
				if not t:search(cb) then 
					table.insert(t, 1, cb)
					log.debug('cbl:append', t, cb, #t)
				end
				return t
			end,
			__call = function (t, ...) 
				if t.__post then
					return t.__post(t:exec(t.__pre and t.__pre(...) or ...))
				else
					return t:exec(t.__pre and t.__pre(...) or ...)
				end
			end,
			exec = function (t, ...)
				local ok, r = true, true
				log.debug('start cbl:', t)
				for k,v in ipairs(t) do
					log.debug('cbl call:', v)
					ok,r = pcall(v, ...)
					log.debug(r, v)
					if not ok then break end
				end
				log.debug('end cbl:', t)
				return ...,ok,r
			end,
			pop = function (t, cb)
				local pos = t:search(cb)
				if pos then table.remove(t, pos) end
				return t
			end,
			search = function (t, cb)
				local pos = nil
				for k,v in ipairs(t) do
					if cb == v then
						pos = k
						break
					end
				end
				log.debug('cbl:search', pos)
				return pos
			end,
			post = function (t, cb)
				t.__post = cb
			end,
			pre = function (t, cb)
				t.__pre = cb
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
		log.debug(k, 'fetcher finished', rawget(r, sk))
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
					ns[k] = v
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
			local prefixes = {
				async_ = constant.flags.ASYNC,
				timed_ = constant.flags.TIMED,
				mcast_ = constant.flags.MCAST,
			}
			local parse = function (name)
				local flag, index, match = 0, 0
				repeat
					match = false
					for k,v in pairs(prefixes) do
						if bit.band(flag, prefixes[k]) == 0 then
							local pattern = "(.-)"..k.."(.*)"
							local tmp = string.gsub(name, pattern, "%1%2", 1)
							if #tmp ~= #name then
								name = tmp
								flag = bit.bor(flag, v)
								match = true
							end
						end
					end
				until not match 
				if string.find(name, '_') == 1 then
					flag = bit.bor(flag, constant.flags.PROTECTED)
				end
				return name,flag
			end
			local async_launch = function (t, ...)
				return t.call(t.__ptr, t.__flag, t.__name, ...)
			end
			local mcast_launch = function (t, ...)
				return t.call(t.__ptr, t.__flag, t.__name, ...)
			end
			local method_mt = {
				__call = function (t, ...)
					if bit.band(t.__flag, constant.flags.ASYNC) ~= 0 then
						return yue.fiber(async_launch):run_unprotect(t, ...)
					elseif bit.band(t.__flag, constant.flags.MCAST) ~= 0 then
						return yue.fiber(mcast_launch):run_unprotect(t, ...)
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
				log.debug('method_index', k, pk, f, mt, method_mt)
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
		log.info('yue:', 'lj='..version, lib.mode)
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
				return self
			end,
			bind = function (self, events, fn, timeout, method)
				local t,f,ef = type(events),0,{}
				if not method then method = 'push' end
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
					local ns = self.namespace[key]
					ns[method](ns, v)
				end
				log.debug('bind', t, f, #ef)
				if f ~= 0 then
					self.__bounds[1] = bit.bor(self.__bounds[1], f)
					lib.yue_emitter_bind(self.__ptr, self.__event_id, f, timeout or 0)
				end
				if #ef > 0 then
					for k,v in ipairs(ef) do
						lib.yue_emitter_bind(self.__ptr, constant.events.ID_EMIT, v, timeout or 0)
					end
				end
				return self
			end,
			wait = function (self, events, timeout)
				local t = type(events)
				if t == 'string' then
					if self.__flags[events] then
						lib.yue_emitter_wait(self.__ptr, self.__event_id, self.__flags[events], timeout or 0)
					else
						lib.yue_emitter_wait(self.__ptr, constant.events.ID_EMIT, events, timeout or 0)
					end
				else
					error('invalid events type:', t)
				end
				return self
			end,
			emit = function (self, ...)
				lib.yue_emitter_emit(self.__ptr, ...)
			end,
			import = function (self, src)
				import(self.namespace, src)
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
								local ok,r
								if bit.band(flags, emitter_mt.__flags.TIMED) ~= 0 then
									if bit.band(flags, emitter_mt.__flags.MCAST) ~= 0 then
										-- ptr, flags, future, method_name, timeout_sec, arg1, ...
										ok,r = lib.yue_socket_connect(ptr, args[5])
									else
										-- ptr, flags, method_name, timeout_sec, arg1, ...
										ok,r = lib.yue_socket_connect(ptr, args[4])
									end
								else
									-- ptr, flags, method_name, arg1, ...
									ok,r = lib.yue_socket_connect(ptr)
								end
								-- wait fails
								if not ok then error(r) end
							end
							return lib.yue_socket_call(ptr, flags, ...)
						end,
						__post_open = function (socket)
							log.info('open', socket:addr(), socket.__ptr)
						end,
						__post_close = function (socket, ok, r)
							log.info('close', socket:addr(), socket.__ptr, socket:closed())
							if socket:closed() then -- client connection can reconnect.
								socket:__unref()
							elseif ok and r then -- reconnection
								lib.yue_socket_connect(socket.__ptr)
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
							-- if server connection, bind again but never add these functions
							-- (because listener namespace already add these)
							-- but watcher may already be removed when previous connection closed, 
							-- so try to call yue_emitter_bind
							r:bind({ -- caution, it yields (means next fiber start execution)
								open = mt.__post_open, 
								close = mt.__post_close
							}, nil, 0, 'post')
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
						__flags = { accept = 0x00000001 },
						__new = lib.yue_listener_new,
						__procs = function (emitter) return nil end,
						__pre_accept = function (listener, socket_ptr)
							local s = yue.open(listener, socket_ptr)
							log.debug(s, socket_ptr)
							assert(s == objects__[socket_ptr])
							log.debug('============= pre acc', s, listener, socket_ptr)
							return s
						end,
						__post_accept = function (s, ok, r)
							if ok and r then
								log.debug('auth b4:', s:authorized())
								s:grant()
								log.debug('auth:', s:authorized())
								s.procs.accept__(r)
								log.debug('accept__ finish')
							else
								s:close()
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
							r:bind({ accept = r.__pre_accept}, nil, 0, 'pre')
							r:bind({ accept = r.__post_accept}, nil, 0, 'post')
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
		-- pritty printer (if available)
		yue.pp = pprint
		-- non-emittabble objects
		yue.fiber = (function () 
			local fiber_mt = (function ()
				local protect = (lib.mode ~= 'debug' and function (ft, fn, ...)
					ft(pcall(fn, ...))
				end or function (ft, fn, ...)
					local res = {pcall(fn, ...)}
					log.debug(ft, unpack(res))
					ft(unpack(res))
				end)
				local unprotect = function (ft, fn, ...)
					ft(fn(...))
				end
				local r = {
					run_unprotect = function (self, ...)
						local ft = yue.future()
						lib.yue_fiber_run(self.__ptr, unprotect, ft, self.__f, ...)
						return ft
					end,
					run = function (self, ...)
						local ft = yue.future()
						lib.yue_fiber_run(self.__ptr, protect, ft, self.__f, ...)
						return ft						
					end,
					co = function (self, ...)
						return lib.yue_fiber_coro(self.__ptr)
					end,
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
				local r = {
					on = function (self, cb)
						table.insert(self.__receiver, cb)
						if self.__result then
							cb(unpack(self.__result))
						end
					end,
					sync = function (self)
						local co = coroutine.running()
						self:on(function (ok, ...)
							print("result: resume suspended coro", co, ok, unpack{...})
							coroutine.resume(co, ok, ...)
						end)
						return coroutine.yield()
					end,
					__call = function (self, ...)
						for k,v in ipairs(self.__receiver) do
							v(...)
						end
						self.__result = {...}
					end,
				}
				r.__index = r
				return r
			end)()
			return function ()
				return setmetatable({ __receiver = {} }, future_mt)
			end
		end)()
		yue.client = (function ()
			local poll = lib.yue_poll
			local alive = lib.yue_alive
			local client_mt = {
				__call = function (self, fn)
					self.fb = yue.fiber(fn)
					self.fb:run(self):on(function (ok,r)
						print("client result: ", ok, r)
						-- on failure or success and has return value, terminate immediately
						if (not ok) or (ok and r) then 
							self:exit(ok, r)
						end
					end)
					while self.running and alive() do 
						poll()
					end
					if not self.running then
						return self.success,self.result
					else
						return false,"terminated"
					end
				end,
				exit = function (self, ok, r)
					self.success,self.result = ok,r
					self.running = false
					coroutine.yield(self.fb:co())
				end
			}
			client_mt.__index = client_mt
			local function run(src, mt)
				if type(src) == "string" then
					log.debug('try load as file:', src)
					src = loadfile(src)
					if not src then
						log.debug('try load as string:', src)
						src = loadstring(src)
					end
				end
				if type(src) ~= "function" then
					error('invalid source:'..type(src))
				end
				-- sandboxing
				setfenv(src, setmetatable({}, {__index = _G}))
				return setmetatable({ running = true }, mt)(src)
			end
			return function (src)
				return run(src, client_mt)
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
		-- yue finalizer (client auto finalize)
		yue.fzr = lib.fzr
		
		
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
