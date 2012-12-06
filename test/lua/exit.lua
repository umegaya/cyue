local yue = require('_inc')


print('-- test yue exit -----------------------------------------')
local ok, r = yue.client(function(cl)
	print('start client')
	local c = yue.open('tcp://localhost:8888').procs
	print('call rpc')
	c.async_keepalive('hogehoge'):on(function(ok,r)
		print('receive response:', ok, r)
		assert(ok and (r == 'hogehoge'))
		cl:exit(true, "test exit")
		assert(false) -- should not reach here
	end)
end)
print('end yue.client:', ok, r)
assert(r == "test exit")
print('pass assert')