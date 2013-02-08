local lib,is_server_process = (function ()
	if package.loaded.libyue then
		return package.loaded.libyue,true
	else
		package.cpath = package.cpath .. ";/usr/local/lib/?.so;"
		return require('libyue'),false
	end
end)()
local ffi = require('ffi')
local string = require('string')
local bit = require('bit')
local coroutine = require('coroutine')
local dbg = (function ()
	if lib.mode ~= 'debug' then 
		return function () end 
	end
	local ok, r = pcall(require, 'debugger')
	return ok and r or function () end
end)()
local pprint = (function ()
	local ok, r = pcall(require, 'serpent')
	return ok and r.dump or _G.print
end)()
local version = (jit and jit.version or _VERSION)
local yue = {} --it will be module table
-- TODO: decide more elegant naming rule
-- TODO: not efficient if # of emittable objects is so many (eg, 1M). should shift it to C++ code?
local namespaces__ = lib.namespaces
local objects__ = lib.objects
local peer__ = {}

local log = (is_server_process or lib.mode == 'debug') and {
	debug = (lib.mode == 'debug' and (function (...) print(...) end) or (function (...) end)),
	info = function (...) print(...) end,
	error = function (...) print(...) end,
	fatal = function (...) print(...) end,
} or {
	debug = function (...) end,
	info = function (...) end,
	error = function (...) end,
	fatal = function (...) end,
}
local errors = (function ()
	local errors = {}
	local codes = lib.const.rpcerrors
	local mt = {
		is_a = function (self, error)
			local tmp = self
			while tmp do 
				if tmp.name == error then
					return true
				end
				tmp = tmp.super
			end
			return false
		end,
		__tostring = function (self)
			local err = "Unknown"
			for k,v in pairs(codes) do
				if v == self.code then
					err = k
					break
				end
			end
			return err .. ":" .. self.msg .. ":" .. self.bt
		end,
	}
	mt.__index = mt
	local create_bt = function (from, bt)
		local from = from and ('on calling ' .. from) or ''
		local curbt = debug.traceback(coroutine.running(), from, 3)
		return (bt and (bt .. '\n' .. curbt) or curbt):sub(0, 4096) -- string upto 4kb
	end
	local new = function (type, msg, code, from, bt, depth)
		local caller = debug.getinfo(depth, 'Sl')
		msg = caller and (caller.source:sub(2) .. ':' .. caller.currentline .. ':' .. tostring(msg)) or msg
		return setmetatable({
			type = type,
			code = code,
			msg = msg,
			bt = create_bt(from, bt)
		}, errors[type])
	end
	local e = function (err, super)
		errors[err] = setmetatable({
			new = function (msg, from, bt, code, depth)
				return new(err, msg, code or codes[err], from, bt, (depth or 3))
			end,
			name = err,
			super = errors[super],
			__tostring = mt.__tostring
		}, mt)
		errors[err].__index = errors[err]
		return errors[err]
	end
	errors.codes = codes
	errors.new = function (e)
		if type(e) == "string" then
			local at = e:find('@')
			local err,str = e:sub(1, at), e:sub(at + 1)
			local eobj = (errors[err] or errors.Error)
			return eobj.new(str)
		elseif type(e) == "table" then
			if e.type then -- fiber returns error from remote
				local eobj = new(e.type, e.msg, e.code, e.from, e.bt, 3)
				eobj.bt = create_bt(e.from, e.bt)
				return eobj
			else -- internal error before dispatch RPC
				local err = "Error"
				for k,v in pairs(codes) do
					if v == e[1] then
						err = k
						break
					end
				end
				return errors[err].new(e[2], e.from, e.bt, nil, 4)
			end
		end
	end
	e("Error")
		e("RPCError", "Error")
			e("TimeoutError", "RPCError")
			e("ClientError", "RPCError")
				e("TransportError", "ClientError")
					e("NetworkUnreachableError", "TransportError")
					e("ConnectionRefusedError", "TransportError")
					e("ConnectionTimeoutError", "TransportError")
				e("MessageRefusedError", "ClientError")
					e("MessageTooLargeError", "MessageRefusedError")
				e("CallError", "ClientError")
					e("NoMethodError", "CallError")
					e("ArgumentError", "CallError")
			e("ServerError", "RPCError")
				e("ServerBusyError", "ServerError")
			e("RuntimeError", "RPCError")		
	return errors
end)()
local yue_mt = (function ()
	local create_callback_list, is_callback_list = (function ()
		local cbpair = function (cb, catch)
			return function (...)
				local v = {pcall(cb,...)}
				if not v[1] then
					if catch then
						catch(v[2])
					else
						log.error('uncatched bind callback error:', yue.pp(v[2]))
					end
				end
				return unpack(v, 2)
			end
		end
		local mt = {
			push = function (t, cb, catch) 
				if not t:search(cb) then 
					table.insert(t, {cb,catch})
					log.debug('cbl:push', t, cb, #t)
				end
				return t
			end,
			append = function (t, cb, catch)
				if not t:search(cb) then 
					table.insert(t, 1, {cb,catch})
					log.debug('cbl:append', t, cb, #t)
				end
				return t
			end,
			__call = function (t, ...) 
				if t.__post then
					if t.__pre then
						return t.__post(t:exec(t.__pre(...)))
					else
						return t.__post(t:exec(...))
					end
				elseif t.__pre then
					return t:exec(t.__pre(...))
				else
					return t:exec(...)
				end
			end,
			exec = function (t, ...)
				local ok, r = true, nil
				log.debug('start cbl:', t, t.__post, t.__pre)
				for k,v in ipairs(t) do
					log.debug('cbl call:', v[1], ...)
					ok,r = pcall(v[1], ...)
					log.debug(r, v[1])
					if not ok then
						if v[2] then 
							v[2](r) -- catch error 
						else
							log.error('uncatched bind callback error:', yue.pp(r))
						end
						break 
					end
				end
				log.debug('end cbl:', t, ...)
				return ok,r,t,...
			end,
			pop = function (t, cb)
				local pos = t:search(cb)
				if pos then table.remove(t, pos) end
				return t
			end,
			search = function (t, cb)
				local pos = nil
				for k,v in ipairs(t) do
					if cb == v[1] then
						pos = k
						break
					end
				end
				log.debug('cbl:search', pos)
				return pos
			end,
			post = function (t, cb, catch)
				t.__post = cbpair(cb,catch)
			end,
			pre = function (t, cb, catch)
				t.__pre = cbpair(cb,catch)
			end,
		}
		mt.__index = mt
		return function ()
			return setmetatable({}, mt)
		end, function (obj)
			return (getmetatable(obj) == mt)
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
				log.debug('attempt to call protected method',local_call,sk,b,k)
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
			if not f then error(errors.Error.new(e)) 
			else 
				-- if f create some global symbol, it stores in ns, and lookup both ns and _G of yue.lua
				setfenv(f, setmetatable({}, {
					__index = function (t, k)
						return ns.__sym[k] or _G[k]
					end,
					__newindex = function (t, k, v)
						ns[k] = v
					end
				}))
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
			error(errors.Error.new('invalid src:' .. type(src)))
		end
		return ns
	end
	
	local create_namespace = (function ()
		local newindex = function (t, k, v)
			if type(v) == 'function' and string.sub(k, 1, 2) == '__' and
				((not t.__sym[k]) or is_callback_list(t.__sym[k])) then
				-- because protected namespace always return nil to key start with _, 
				-- execution path will come here if already value for __**** key is set,
				-- so need to check value is already set and if set, set value to existing value.
				rawset(t.__sym, k, (t.__sym[k] or create_callback_list()):push(v))
			else
				rawset(t.__sym, k, v)
			end
		end
		local mt = {
			protect = {
				__index = function (t, k)
					local r = fetcher(t.__sym, k, false)
					if r then rawset(t, k, r) end
					return r
				end,
				__newindex = newindex,
			},
			raw = {
				__index = function (t, k)
					local r = fetcher(t.__sym, k, true)
					if r then rawset(t, k, r) end
					return r					
				end,
				__newindex = newindex,
			},
		}
		
		return function (kind)
			return setmetatable({ __sym = {} }, mt[kind])
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
					log.debug('call result', unpack(r))
					if r[1] then -- here cannot use [a and b or c] idiom because b sometimes be falsy.
						return unpack(r, 2)
					else
						r[2].from = lib.yue_emitter_address(t.__ptr)
						local err = errors.new(r[2])
						log.debug('call result', err)
						error(err)
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
		log.info('yue', version, lib.mode)
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
			__unref = function (self)
				log.debug('unref', self.__ptr)
				namespaces__[self.__ptr] = nil
				objects__[self.__ptr] = nil
				lib.yue_emitter_unref(self.__ptr)
				assert(self.procs.__emitter == self)
				self.procs.__emitter = nil	-- resolve cyclic reference
				self.__ptr = nil
			end,
			activate = function (self, ptr, namespace)
				log.debug('__activate', ptr)
				namespaces__[ptr] = namespace
				objects__[ptr] = self
				lib.yue_emitter_refer(ptr)
				lib.yue_emitter_open(ptr)
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
					error(errors.Error.new('invalid events type:' .. t))
				end
				for k,v in pairs(events) do
					local key = '__' .. k
					if self.namespace.__sym[key] then
						self.namespace.__sym[key]:pop(v)
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
					error(errors.Error.new('invalid events type:' .. t))
				end
				for k,v in pairs(events) do
					local key = '__' .. k
					if not self.namespace.__sym[key] then
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
					local cbl = self.namespace.__sym[key]
					-- if this thread has catch function, set it to callback list in case of error.
					log.debug('for', cbl, k, method)
					cbl[method](cbl, v, getfenv(0).__catch)
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
					error(errors.Error.new('invalid events type:' .. t))
				end
				return self
			end,
			emit = function (self, ...)
				lib.yue_emitter_emit(self.__ptr, ...)
				return self
			end,
			import = function (self, src)
				import(self.namespace, src)
				return self
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
						__procs = function (emitter) return nil end,
						__create = function (self,name,start,intval,symbols)
							-- normal timer object creation
							if (type(name) == 'string' and start and intval) then 
								return lib.yue_timer_new(name,start,intval), create_namespace('raw'),symbols
							elseif (type(name) == 'number' and start) then
								return lib.yue_timer_new(name,start), create_namespace('raw'),symbols
							-- named timer which already created by another thread
							elseif type(name) == 'userdata' then
								return name,(namespaces__[name] or create_namespace('raw'))
							else
								error(errors.Error.new('invalid timer args:' .. type(name)))
							end
						end,
						find = function (name)
							local p = lib.yue_timer_find(name)
							return p and yue.timer(p) or nil
						end,
					}),
		task = 		extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_TIMER,
						__flags = { tick = 0x00000001 },
						__create = function (self,ptr,symbols)
							return ptr,(namespaces__[ptr] or create_namespace('raw')),symbols
						end,
						__procs = function (emitter) return nil end,
					}),
		signal = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_SIGNAL,
						__flags = { signal = 0x00000001 },
						__create = function (self,sig,symbols)
							local ptr = lib.yue_signal_new(sig)
							return ptr,(namespaces__[ptr] or create_namespace('raw')),symbols
						end,
						__procs = function (emitter) return nil end,
					}),
		socket = 	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_SESSION,
						__flags = { 
							open = 0x00000002,
							establish = 0x00000004,
							data = 0x00000008,
							close = 0x00000010,
						},
						__create = function (self, hostname, options, symbols)
							if type(hostname) == 'string' then
								-- normal creation (given: 1:hostname, 2:option(table), 3:symbols(string/table))
								-- if only 2 args are given, assume second argument as symbols
								if not symbols then symbols, options = options, nil end
								return lib.yue_socket_new(hostname, options), create_namespace('protect'), symbols
							elseif type(hostname) == 'table' then
								-- internal use: server socket creation (given: 1:listener & 2:socket)
								return options,namespaces__[hostname.__ptr],{}
							elseif type(hostname) == 'userdata' then
								-- stream peer creation (given: 1:socket(ptr))
								assert(false)
								return hostname,(namespaces__[lib.yue_socket_listener(hostname)] or create_namespace('protect'))
							else
								error(errors.Error.new('invalid socket args:' .. type(hostname)))
							end
						end,
						__call = function (ptr, flags, ...)
							if not lib.yue_socket_connected(ptr) then
								local args = {...}
								local ok,r
								if bit.band(flags, emitter_mt.__flags.TIMED) ~= 0 then
									if bit.band(flags, emitter_mt.__flags.MCAST) ~= 0 then
										-- ptr, flags, [future, method_name, timeout_sec, arg1, ...] == ...
										ok,r = lib.yue_socket_connect(ptr, args[3])
									else
										-- ptr, flags, [method_name, timeout_sec, arg1, ...] == ...
										ok,r = lib.yue_socket_connect(ptr, args[2])
									end
								else
									-- ptr, flags, method_name, arg1, ...
									ok,r = lib.yue_socket_connect(ptr)
								end
								-- wait fails
								if not ok then return ok,r end
							end
							return lib.yue_socket_call(ptr, flags, ...)
						end,
						__post_open = function (ok, r, t, socket)
							log.info('open', socket:addr(), socket.__ptr)
						end,
						__post_close = function (ok, r, t, socket)
							log.info('close', socket:addr(), socket.__ptr, socket:closed())
							if socket:closed() then -- client connection can reconnect.
								socket:__unref()
							elseif ok and r then -- reconnection
								ok,r = lib.yue_socket_connect(socket.__ptr)
								if not ok then error(errors.new(r)) end
							end
						end,
						__initializing = {},
						__ctor = function (ptr, mt, namespace, hostname, options, symbols)
							local no_cache = (type(hostname) == 'string' and options and options.no_cache)
							if (not no_cache) and mt.__initializing[ptr] then 
								mt.__initializing[ptr]:wait('initialized')
								return objects__[ptr] -- here mt.__initializing[ptr] already set nil (see __activate)
							end
							local r = emitter_mt.__ctor(ptr, mt, namespace, hostname, options, symbols)
							if not no_cache then
								mt.__initializing[ptr] = r
							end
							if not r:listener() then
								namespace.accept__ = r:__make_accept_closure(r) -- bind r as upvalue
							end
							-- if server connection, bind again but never add these functions twice
							-- (because listener namespace already add these)
							-- but watcher may already be removed when previous connection closed, 
							-- so try to call yue_emitter_bind
							r:bind({ -- caution, it yields (means next fiber start execution)
								open = mt.__post_open, 
								close = mt.__post_close
							}, nil, 0, 'post')
							return r
						end,
						activate = function (self, ptr, namespace)
							log.debug('__activate: ', ptr, objects__[ptr])
							if not objects__[ptr] then
								emitter_mt.activate(self, ptr, namespace)
								if self.__initializing[ptr] then 
									self.__initializing[ptr] = nil
									self:emit('initialized')
								end
							end
						end,
						__accept_processor = function (self, socket, r)
							log.debug('accept processor', self, socket, r)
							local aw = self.namespace.__sym.__accept
							socket:grant() -- enable access to remote peer
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
						__create = function (self)
							return nil,create_namespace('protect')
						end,
						__ctor = function (dummy, mt, namespace)
							local ptr,refp,type = lib.yue_peer()
							log.debug('peer', ptr,refp,type)
							return emitter_mt.__ctor(ptr, mt.__mt[type], namespace)
						end,
						activate = function (ptr, namespace)
							-- force do nothing
						end,
						-- TODO: for datagram peer we need to call this even for 5.1 compatible lua. but how?
						__gc = function (self)
							log.debug('peer gc')
							lib.yue_peer_close(self.__ptr)
							peer__[self.__ptr] = nil
						end,
					}),
		server =	extend(emitter_mt, { 
						__event_id = emitter_mt.__events.ID_LISTENER,
						__flags = { accept = 0x00000001 },
						__new = lib.yue_listener_new,
						__procs = function (emitter) return nil end,
						__pre_accept = function (listener, socket_ptr)
							local s = yue.socket(listener, socket_ptr)
							log.debug(s, socket_ptr)
							assert(s == objects__[socket_ptr])
							return s
						end,
						__post_accept = function (ok, r, t, s)
							if ok and (r or #t == 0) then -- #t == 0 => no accept watcher specified (super dirty hack through...)
								log.debug('auth b4:', s:authorized())
								s:grant()
								log.debug('auth:', s:authorized())
								s.procs.accept__(r)
								log.debug('accept__ finish')
							else
								s:close()
							end
						end,
						__create = function (self, hostname, options, symbols)
							if type(hostname) == 'string' then
								-- if only 2 args are given, assume second argument as symbols
								if not symbols then symbols, options = options, nil end
								-- normal creation (given: 1:hostname 2:options(table) 3:symbols(table/string))
								return lib.yue_listener_new(hostname, options),create_namespace('protect'),symbols
							else
								error(errors.Error.new('invalid listener args'))
							end
						end,
						__ctor = function (ptr, mt, namespace, hostname, options, symbols)
							local r = emitter_mt.__ctor(ptr, mt, namespace, hostname, options, symbols)
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
						__create = function (self, name, code, pgname, timeout, symbols)
							if type(name) == 'string' then
								-- because thread already initialize its namespace by using 'code', force activate.
								return lib.yue_thread_new(name, code, pgname, timeout),create_namespace('raw'),symbols
							elseif type(name) == 'userdata' then
								-- internal use: from thread.find, creation of thread.current
								return name,(namespaces__[name] or create_namespace('raw'))
							else
								error(errors.Error.new('invalid thread arg'))
							end
						end,
						__call = lib.yue_thread_call,
						count = function ()
							return lib.yue_thread_count()
						end,
						find = function (name)
							local p = lib.yue_thread_find(name)
							return p and yue.thread(p) or nil
						end,
						exit = function (self, result)
							self.result = result
							self:close()
						end
					}),
	}
	-- additional initialization of metatables
	metatables.peer.__mt = {
		stream = extend(metatables.peer, { __call = lib.yue_socket_call, __gc = function() end }),
		thread = extend(metatables.peer, { __call = lib.yue_thread_call, __gc = function() end }),
		datagram = extend(metatables.peer, { __call = lib.yue_peer_call, __close = metatables.peer.__gc }),
	}
	local taskgrp_use_own_timer = (lib.feature.timer == 'timerfd')
	metatables.taskgrp = extend(metatables.timer, {
		__flags = {},
		__create = function (self, name, max_task, max_interval, resolution_us)
			if type(name) == 'string' then
				-- normal creation: caution it force activate by last {} because taskgrp it self never emits or call symbol
				return lib.yue_taskgrp_new(name, max_task, max_interval, resolution_us), create_namespace('raw'), {}
			elseif type(name) == 'userdata' then
				return name,(namespaces__[name] or create_namespace('raw'))
			else
				error(errors.Error.new('invalid timer args'))
			end
		end,
		find = function (name)
			local p = lib.yue_timer_find(name)
			return p and yue.taskgrp(p) or nil
		end,
		alloc = function (self, start, intval)
			local p = lib.yue_task_new(self.__ptr, start, intval)
			return p and yue.task(p) or nil
		end,
		activate = function (self, ptr, namespace)
			if taskgrp_use_own_timer then
				emitter_mt.activate(self, ptr, namespace)
			end
		end,
	})

	lib.__finalizer = function ()
		for k,v in pairs(namespaces__) do
			lib.yue_emitter_unref(k)
		end
	end

	return {
		__call = function(t, ...) 
			local mt = metatables[t.__type]
			local ptr,namespace,symbols = mt:__create(...)
			if objects__[ptr] then -- cached object may return same pointer as prviously allocated
				return objects__[ptr]
			end
			local r = mt.__ctor(ptr, mt, namespace, ...)
			if symbols then -- have a preloaded symbols. apply to namespace and activate
				import(namespace, symbols)
			end
			mt.activate(r, ptr, namespace)
			return r
		end,
		__index = function(t, k)
			return metatables[t.__type][k]
		end,
	}
end)()

setmetatable((function () 
		-- try catch clause
		yue.try = function (block) 
			local prev = getfenv(0).__catch
			getfenv(0).__catch = block.catch
			local ok, r = pcall(block[1])
			getfenv(0).__catch = prev
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
		-- can open thread and socket with same semantics (but socket can specify some options and symbols)
		yue.open = function(addr, options, symbols)
			local em
			if addr:sub(1, 6) == 'thread' then
				em = yue.thread.find(addr:sub(10))
			else
				em = yue.socket(addr, options, symbols)
			end
			return em and em.procs or nil
		end
		yue.fiber = (function () 
			local fiber_mt = (function ()
				local protect = (lib.mode ~= 'debug' and function (catch, ft, fn, ...)
					getfenv(0).__catch = catch
					ft(pcall(fn, ...))
				end or function (catch, ft, fn, ...)
					getfenv(0).__catch = catch
					local res = {pcall(fn, ...)}
					log.debug(ft, unpack(res))
					ft(unpack(res))
				end)
				local unprotect = function (catch, ft, fn, ...)
					getfenv(0).__catch = catch
					ft(fn(...))
				end
				local r = {
					run_unprotect = function (self, ...)
						local ft = yue.future()
						lib.yue_fiber_run(self.__ptr, unprotect, getfenv(0).__catch, ft, self.__f, ...)
						return ft
					end,
					run = function (self, ...)
						local ft = yue.future()
						lib.yue_fiber_run(self.__ptr, protect, getfenv(0).__catch, ft, self.__f, ...)
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
							local ok, r = pcall(cb, unpack(self.__result))
							if not ok then self.__catch(cb, r) end
						end
						return self
					end,
					sync = function (self)
						local co = coroutine.running()
						self:on(function (ok, ...)
							log.debug("result: resume suspended coro", co, ok, unpack{...})
							coroutine.resume(co, ok, ...)
						end)
						return coroutine.yield()
					end,
					__call = function (self, ...)
						for k,v in ipairs(self.__receiver) do
							local ok, r = pcall(v, ...)
							if not ok then self.__catch(v, r) end
						end
						self.__result = {...}
					end,
					__catch = function (fn, r) 
						local handler = getfenv(0).__catch
						if handler then
							handler(r)
						else
							log.info('future rescue:', r) 
						end
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
					yue.try {function ()
							assert(getfenv(0).__catch)
							self.fb = yue.fiber(fn)
							self.fb:run(self):on(function (ok,r)
								log.debug("client result: ", ok, r)
								-- on failure or success and has return value, terminate immediately
								if (not ok) or (ok and r) then 
									self:exit(ok, r)
								end
							end)
							while self.running and alive() do 
								poll()
							end
						end,
						catch = function (e)
							self:exit(false, e)
						end,
						finally = function () 
						end
					}
					if not self.running then
						return self.success,self.result
					else
						return false,"terminated"
					end
				end,
				exit = function (self, ok, r)
					self.success,self.result = ok,r
					self.running = false
					if self.fb then
						coroutine.yield(self.fb:co())
					end
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
					error(errors.Error.new('invalid source:'..type(src)))
				end
				-- sandboxing
				setfenv(src, setmetatable({}, {__index = _G}))
				return setmetatable({ running = true }, mt)(src)
			end
			return function (src)
				return run(src, client_mt)
			end
		end)()
		yue.die = function (options)
			options = (options or {})
			log.info(
				(options.msg and options.msg or 'yue will die:') .. 
				(options.shutdown_after and ('after ' .. options.shutdown_after) or 'now')
			)
			yue.timer(options.shutdown_after or 0.0, 1.0):bind('tick', function (self)
				lib.yue_die()
				self:close()
			end)
		end
		-- debugger (if available)
		yue.dbg = dbg
		-- pritty printer (if available)
		yue.pp = pprint
		-- FFI (luajit)
		yue.ffi = ffi
		-- initialize util module from lib object
		yue.util = lib.util
		-- initialize yue running mode (debug/release)
		yue.mode = lib.mode
		-- initialize constants
		yue.const = lib.const
		yue.feature = lib.feature
		-- yue finalizer (client auto finalize)
		yue.fzr = lib.fzr
		-- system logger
		yue.log = log
		-- error facility
		yue.errors = errors
		
		-- replace original lua API
		yue.original = {} -- original API stored in here
		yue.original.require = _G.require
		yue.original.error = _G.error
		yue.require = function (name, version, pkgname) -- yue require will install package if not installed.
			local ANOTHER_ROCKSPEC_SERVER = 'http://rocks.moonscript.org/'
			local ok, r = pcall(yue.original.require, name)
			if not ok then
				log.info('require fails with: ' .. r)
				local p, cmd = nil, ('sudo luarocks --server=' .. ANOTHER_ROCKSPEC_SERVER .. 
					' install ' .. (pkgname or name) .. (version and (' ' .. version) or ''))
				log.debug('COMMAND:', cmd)
				ok, p = pcall(io.popen, cmd)
				if not ok then error(errors.Error.new(r)) end
				while true do
					local l = p:read()
					if not l then break end
					log.info('luarocks: ' .. l)
				end
				ok, r = pcall(yue.original.require, name)
				if not ok then error(errors.Error.new(r)) end
			end
			return r
		end
		yue.error = function (e, kind)
			if type(e) ~= 'table' then
				e = (kind and yue.errors[kind] or yue.errors.RuntimeError).new(e)
			end
			yue.original.error(e)
		end
		yue.uninstall = function (pkgname, version)
			local ok, p = pcall(io.popen, 
				'sudo luarocks remove ' .. pkgname .. (version and (' ' .. version) or '')
			)
			if not ok then error(errors.Error.new(p)) end
			while true do
				local l = p:read()
				if not l then break end
				log.info('luarocks: ' .. l)
			end
		end
		_G.require = yue.require
		_G.error = yue.error
		
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
