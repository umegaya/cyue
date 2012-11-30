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
assert(r == "test exit")
