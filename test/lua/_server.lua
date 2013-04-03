local yue = require 'yue'

yue.server('tcp://0.0.0.0:8888', { wblen = 3 * 1024 * 1024, rblen =  3 * 1024 * 1024 }, '../test/lua/_8888.lua')
yue.server('udp://0.0.0.0:7777', '../test/lua/_7777.lua'):import({
	xx = function(s)
		return 'xxxx' .. s
	end
})
yue.server('ws://0.0.0.0:3000', '../test/lua/_3000.lua')
yue.server('ws://0.0.0.0:3001', '../test/lua/_3001.lua')

yue.server('tcp://0.0.0.0:4000', '../test/lua/_4000.lua')
--yue.server('mcast://239.192.1.2:9999', { ttl = 1 }).namespace.get_hostname = function ()
--	return 'tcp://localhost:8888'
--end
