local yue = require 'yue'

--yue.listen('tcp://0.0.0.0:8888').namespace:import('../test/lua/_8888.lua')
--yue.listen('tcp://0.0.0.0:4000').namespace:import('../test/lua/_4000.lua')
--yue.listen('udp://0.0.0.0:7777').namespace:import('../test/lua/_7777.lua').xx = function(s)
--	return 'xxxx' .. s
--end
--yue.listen('mcast://239.192.1.2:9999', { ttl = 1 }).namespace.get_hostname = function ()
--	return 'tcp://localhost:8888'
--end
yue.listen('ws://0.0.0.0:3000').namespace:import('../test/lua/_3000.lua')
yue.listen('ws://0.0.0.0:3001').namespace:import('../test/lua/_3001.lua')
