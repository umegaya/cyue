require('_inc')

local ctx = yue.core.listen('0.0.0.0:8888')

-- run as worker (search another master node by multicast on 239.192.1.2:9999)
-- it search for at least 3 master node and notice its remote addr (eg. tcp://100.0.0.1:8888)
yue.paas({
	mcast_addr = '239.192.1.2:9999', 
	required_master_num = 3,
}):as_worker(ctx):wait(function(ok, r)
	if ok then
		print('initialize finish', r)
	else
		print('initialize failure', r)
		assert(false)
	end
end)
