require('yue')

--	@name: yue.try
--	@desc: provide exception handling
-- 	@args: main: code block (function) to be executed
--		   catch: error handler
--		   finally: code block which is called antime when 
--					main logic execution finished
-- 	@rval: 
yue.try = function(main, catch, finally)
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

yue.protect = function(p)
	local c = { conn = p }
	setmetatable(c, {
		__index = function(t, k)
			return function(...)
				local r = {t.conn[k](...)}
				if not yue.error(r[1]) then return unpack(r)
				else error(r[1]) end
			end
		end
	})
	return c
end
yue.open = function(host, opt)
	return yue.protect(yue.connect(host, opt))
end



--	@name: yue.result
--	@desc: tables which stores yue execution result
yue.result = {}



--	@name: yue.terminate
--	@desc: finish lua rpc yieldable program with return code
-- 	@args: ok: execution success or not
--		   code: return code which returned by yue.run or yue.exec
-- 	@rval: 
yue.terminate = function (ok, code) 
	yue.alive = false
	yue.result.code = code
	yue.result.ok = ok
	yue.yield()
end



--	@name: yue.exit
--	@desc: equivalent to yue.terminate(false, code)
--	@args: code: return code
-- 	@rval:
yue.exit = function (code) 
	print("yue.exit code=", code)
	yue.terminate(true, code) 
end



--	@name: yue.raise
--	@desc: equivalent to yue.exit(false, code)
--	@args: code: return code
-- 	@rval:
yue.raise = function (code) 
	yue.terminate(false, code) 
end



-- 	@name: yue.run
-- 	@desc: execute rpc yieldable lua code from file
-- 	@args: file: lua code to be executed
-- 	@rval: return value which specified in yue.exit
yue.assert = function(expr, message)
	if expr then 
		return
	else
		if not message then message = 'assertion fails!' end
		print(message) 
		debug.traceback()
		yue.raise(message)
	end
end



-- 	@name: yue.run
-- 	@desc: execute rpc yieldable lua code from file
-- 	@args: file: lua code to be executed
-- 	@rval: return value which specified in yue.exit
yue.exec = function(file)
	local f,e = loadfile(file)
	if f then 
		return yue.run(f) 
	else 
		error(e) 
	end
end



-- 	@name: yue.run
-- 	@desc: execute rpc yieldable lua function 
-- 	@args: f function to be executed
-- 	@rval: return value which specified in yue.exit
yue.run = function(f)
	yue.result.ok = true
	yue.result.code = nil
	yue.alive = true
	yue.mode('normal')
	-- isolation
	setfenv(f, setmetatable({
		['assert'] = yue.assert,
		['error'] = yue.raise,
		['try'] = yue.try,
		['exit'] = yue.exit,
	}, {__index = _G}))
	print("yue.resume start:", yue.alive)
	yue.resume(yue.newthread(f))
	print("yue.resume end:", yue.alive)
	while (yue.alive) do 
		yue.poll()
	end
	yue.mode('sync')
	if yue.result.ok then 
		return yue.result.code 
	else 
		error(yue.result.code) 
	end
end
