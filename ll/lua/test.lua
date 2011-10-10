require('yuec')

-- test sync mode
function sync_test()
	local conn = yue.connect('tcp://localhost:8888')

	local function test(a, b, c)
		a = a * 2;
		b = b * 2;
		c = c * 2;
		return conn.keepalive(a, b, c)
	end
	
	assert(44 == test(22, 33, 55))
end
sync_test()



-- test aync mode
yue.exec('sandbox.lua')
-- sandbox.lua declares test_global in global manner, 
-- so if test_global is null in here, means isolation works
assert(test_global == null) 

r = yue.run(function()
	local c = yue.connect('tcp://localhost:8888')
	c.notify_keepalive('hogehoge'):callback(function(ok,r)
		assert(ok and (r == 'hogehoge'))
		yue.exit(true, "test exit")
		assert(false) -- should not come here
	end)
	
	c.server_rpc_test(10)
end)
assert(r == "test exit")

function tester(_nil, _boolean, _integer, _string, _table)
	assert(type(_nil) == "nil")
	sum = 0
	for k,v in pairs(_table) do
		sum = (sum + v)
	end
	if _boolean then sum = (sum + 10000) end
	return _integer + #_string + sum
end

r = yue.run(function()
	local c = yue.connect('tcp://localhost:8888')
	local v = c.test_func(nil, true, 1000, "string with 18byte", tester, 
		{ 100, 200, ['keys'] = 300 })
	assert(v == 11618)
	yue.exit(true)
end)
