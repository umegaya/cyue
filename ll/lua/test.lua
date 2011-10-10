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