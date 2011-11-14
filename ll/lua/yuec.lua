require('yue')
local y = yue
yue = {}

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
	return m
end)()



-- @module_name:	yue.paas
-- @desc:	features for running yue servers as cluster
yue.paas = (function()
	local m = {}
	return m
end)()



-- @module_name:	yue.uuid
-- @desc:	generate id which is unique through all menber of yue cluster
yue.uuid = (function()
	local m = {}
	return m
end)()



-- @module_name:	yue.hspace
-- @desc:	consistent hash which provides actor group from UUID
yue.hspace = (function()
	local m = {
		__index = function(t, k) 
		end
	}
	return m
end)()
