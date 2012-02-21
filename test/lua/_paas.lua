require 'yue'
local counter = 0
yue.paas:initialize(
	yue.paas.factory.node_list:new(
		["192.168.56.101", "192.168.56.102", "192.168.56.103"]
	),
	function (event, hostname, spec, raddr)
		assert(event == 'add')
		if counter == 0 then
			assert(spec.role == 'role1')
			assert('role1' == yue.core.open('tcp://' .. hostname .. ':1111').get_name())
			counter++
			return
		else if counter == 1 then 
			assert(spec.role == 'role2')
			assert('role2' == yue.core.open('tcp://' .. hostname .. ':1111').get_name())
			counter++
			return
		else if counter == 2 then
			assert(spec.role == 'role3')
			assert(hostname == '192.168.56.103')
			assert('role3' == yue.core.open('tcp://' .. hostname .. ':1111').get_name())
			counter++
			return
		end
	end
)

local host,spec = yue.paas:create({
	role = 'role1',
	bootimg = './_role1.lua'
})

assert(host == "192.168.56.101")

local host,spec = yue.paas:create({
	role = 'role2',
	bootimg = './_role2.lua'
})

assert(host == "192.168.56.102")






