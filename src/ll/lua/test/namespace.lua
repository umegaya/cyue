require('yuec')

print('-- test namespace ----------------------------------------')
yue.client.run(function()
	local c8 = yue.core.open('tcp://localhost:8888')	
	local c7 = yue.core.open('udp://localhost:7777')
	assert('dr56yhu89k2200' == c8.x.y.z:h(200))
	assert('abcf100' == c8.x.y.z.f(100))
	assert('abcg100' == c8.x.y.z.g(100))
	assert('abg100' == c8.x.y.w(100))
	assert('xxxxa' == c7.xx('a'))
	local err = true
	try(function()
		c8.x._y.z.f(100)
	end,
	function()
		err = false
		return true
	end,
	function()
		if err then error('error not occur') end
	end);
	try(function()
		c7.x.y.z.f(100)
	end,
	function()
		err = false
		return true
	end,
	function()
		if err then error('error not occur') end
	end);
	exit(0)
end)