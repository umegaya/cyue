local yue = require('_inc')

yue.client(function ()
print('-- test aync mode ----------------------------------------')
yue.fiber(function()
	print('start fiber')
	local c = yue.open('tcp://localhost:8888')	
	print('call method', c.procs)
	c.procs.async_error_test(4,4,2,1):on(function(ok, r)
		print('notify callback:', ok, r)
		assert(not ok)
		assert(r == "../test/lua/_8888.lua:47: test error!!")
	end)
	print('end call method')
	return "can handle error"
end):run():on(function (ok, r) -- receive result asynchronously
	print(ok, r)
	assert(ok and r == "can handle error")
end)

print('-- test aync mode2 ----------------------------------------')
local ok, catched = yue.fiber(function()
	local c = yue.open('tcp://localhost:8888')	
	local finally_execute = 0
	return yue.try{function () 
			local r = c.procs.error_test(1,2,4,4)
		end,
		catch = function (e)
			print("catch error", e)
			finally_execute = (finally_execute + 1)
			return e
		end,
		finally = function ()
			finally_execute = (finally_execute + 1)
			assert(finally_execute == 2)
		end
	}
end):result() -- wait until execution finished

assert(type(catched) == 'string')
assert(not ok)
end)
