local yue = require('_inc')

print('-- test namespace ----------------------------------------')
yue.client.run(function()
	local c8 = yue.core.open('tcp://localhost:8888')	
	local c7 = yue.core.open('udp://localhost:7777')
	assert('dr56yhu89k2200' == c8.x.y.z:h(200))
	assert('abcf100' == c8.x.y.z.f(100))
	assert('abcg100' == c8.x.y.z.g(100))
	assert('abg100' == c8.x.y.w(100))
	assert('xxxxa' == c7.xx('a'))
	local err = false
	-- test if such a method have a _**** like name, it cannot call
	try(function()
		c8.x._y.z.f(100)
	end,
	function()
		err = true
		return true
	end,
	function()
		if not err then error('error not occur') end
	end);
	err = false
	-- test if such a method not found, error occur
	try(function()
		c7.x.y.z.f(100)
	end,
	function()
		err = true
		return true
	end,
	function()
		if not err then error('error not occur') end
	end);
	err = false
	try(function()
		c7.assert(false)
	end,
	function()
		err = true
		return true
	end,
	function()
		if not err then error('error not occur') end
	end);
	exit(0)
end)
