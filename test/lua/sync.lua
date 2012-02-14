require('_inc')

print('-- test sync mode ----------------------------------------')
function sync_test()
	local conn = yue.core.open('tcp://localhost:8888')

	local function test(a, b, c)
		a = a * 2;
		b = b * 2;
		c = c * 2;
		return conn.keepalive(a, b, c)
	end
	
	assert(44 == test(22, 33, 55))
end
sync_test()
