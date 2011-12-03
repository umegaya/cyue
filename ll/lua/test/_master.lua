require('yuec')

local ctx = yue.listen('0.0.0.0:8888')

-- run as master (search another master node by multicast on 239.192.1.2:9999)
-- port = 8888 so master node address should be like tcp://10.0.0.1:8888.
yue.paas({
	mcast_addr = '239.192.1.2:9999', 
	required_master_num = 3,
}):as_master(ctx):wait(5.0f)

