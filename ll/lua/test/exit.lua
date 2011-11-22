require('yuec')


print('-- test yue exit -----------------------------------------')
r = yue.client.run(function()
	local c = yue.core.open('tcp://localhost:8888')
	c.notify_keepalive('hogehoge'):callback(function(ok,r)
		assert(ok and (r == 'hogehoge'))
		exit "test exit"
		assert(false) -- should not reach here
	end)
	
	c.server_rpc_test(10)
end)
assert(r == "test exit")
