local yue = require('_inc')


print('-- test yue exit -----------------------------------------')
local ok, r = yue.client(function(cl)
	local c = yue.open('tcp://localhost:8888').procs
	c.async_keepalive('hogehoge'):on(function(ok,r)
		assert(ok and (r == 'hogehoge'))
		cl:exit(true, "test exit")
		assert(false) -- should not reach here
	end)
end)
assert(ok and r == "test exit")

local ok, r = yue.client(function(cl)
	local c = yue.open('tcp://localhost:8888').procs
	c.timed_async_keepalive2(1.0, 'hogehoge', 2.0):on(function(ok,r)
		assert(ok and (r == 'hogehoge'))
		cl:exit(true, "test exit")
	end)
end)
assert((not ok) and (r == "exit.lua:18: assertion failed!"))

local external_callback = function (ok, r)
	assert(ok and (r == 'hogehoge'))
	cl:exit(true, "test exit")
end

local ok, r = yue.client(function(cl)
	local c = yue.open('tcp://localhost:8888').procs
	c.timed_async_keepalive2(3.0, 'hogehoge', 2.0):on(function(ok,r)
		print(ok, r)
		assert(ok and (r == 'hogehoge'))
		c.timed_async_keepalive2(1.0, 'hogehoge', 2.0):on(external_callback)
	end)
end)
print(ok, r)
assert((not ok) and (r == "exit.lua:25: assertion failed!"))

local ok, r = yue.client(function(cl)
	yue.timer(0.0, 1.0):bind('tick', function (t)
		error('test error in bind callback')
	end)
end)
assert((not ok) and (r == "exit.lua:42: test error in bind callback"))
print("--- success ---------------------------------------------")
