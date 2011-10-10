require('yue')

--	@name: yue.result
--	@desc: tables which stores yue execution result
yue.result = {}



--	@name: yue.exit
--	@desc: finish lua rpc yieldable program with return code
-- 	@args: code: return code which returned by yue.run or yue.exec
-- 	@rval: 
yue.exit = function (ok, code) 
	yue.alive = false
	yue.result.code = code
	yue.result.ok = ok
	yue.yield()
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
		yue.exit(false, message)
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
	yue.sync_mode(false)
	-- isolation
	setfenv(f, setmetatable(
		{['assert'] = yue.assert}, {__index = _G}))
	yue.resume(yue.newthread(f))
	print("exit yue.resume")
	while (yue.alive) do 
		yue.poll()
	end
	yue.sync_mode(true)
	if yue.result.ok then 
		return yue.result.code 
	else 
		error(yue.result.code) 
	end
end
