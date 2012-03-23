local yue = require '_inc'

assert(yue == package.loaded['yue'])

yue.core.listen('tcp://0.0.0.0:8888').namespace:import('_8888.lua')
yue.core.listen('tcp://0.0.0.0:4000').namespace:import('_4000.lua')
yue.core.listen('udp://0.0.0.0:7777').namespace:import('_7777.lua').xx = function(s)
	return 'xxxx' .. s
end
yue.core.listen('mcast://239.192.1.2:9999', { ttl = 1 }).namespace.get_hostname = function ()
	return 'tcp://localhost:8888'
end
yue.core.listen('ws://0.0.0.0:3000').namespace:import('_3000.lua')
yue.core.listen('ws://0.0.0.0:3001').namespace:import('_3001.lua')

