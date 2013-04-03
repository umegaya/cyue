local yue = require ('_inc')

local ok, r = yue.client(function (cl)
	local c = yue.open('tcp://0.0.0.0:4000', { no_cache = true }, {
        	kill = function (msg)
               		c.msg = msg
        	end
	})
	c.msg = false
	while not c.msg do
		b = yue.util.time.now()
		diff = -(c.ping(b) - yue.util.time.now())
		yue.util.time.suspend(1.0)
		print('ping:' .. diff .. 'us')
	end
	print('keepaliver killed:' .. tostring(c.msg))
end)
print(ok, r)
