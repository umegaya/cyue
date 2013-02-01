local yue = require('_inc')
local try = yue.try

print('-- test namespace ----------------------------------------')
yue.client(function(cl)
	local c8 = yue.open('tcp://localhost:8888')
	local c7 = yue.open('udp://localhost:7777')
	assert('abcf100' == c8.x.y.z.f(100))
	assert('abcg100' == c8.x.y.z.g(100))
	assert('abg100' == c8.x.y.w(100))
	assert('xxxxa' == c7.xx('a'))
	local err = false
	
	
	-- test if such a method have a _**** like name, it cannot call
	try {function()
		c8.x._y.z.f(100)
	end,
	catch = function()
		err = true
		return true
	end,
	finally = function()
		if not err then error('error not occur') end
	end}
	
	
	err = false
	-- test if such a method not found, error occur
	try {function()
		c7.x.y.z.f(100)
	end,
	catch = function()
		err = true
		return true
	end,
	finally = function()
		if not err then error('error not occur') end
	end}
	
	
	err = false
	try {function()
		c7.assert(false)
	end,
	catch = function()
		err = true
		return true
	end,
	finally = function()
		if not err then error('error not occur') end
	end}
	yue.util.time.suspend(0.1)
	cl:exit(true, 0)
end)
