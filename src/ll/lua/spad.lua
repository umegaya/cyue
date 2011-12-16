
local ok = false
if ok then
-- run as master (search another master node by multicast on 239.192.1.2:9999)
-- port = 8888 so master node address should be like tcp://10.0.0.1:8888.
yue.paas({
	mcast_addr = '239.192.1.2:9999', 
	required_master_num = 3
}):as_master(8888):wait()


-- run as worker (search another master node by multicast on 239.192.1.2:9999)
-- it search for at least 3 master node and notice its remote addr (eg. tcp://100.0.0.1:8888)
yue.paas({
	mcast_addr = '239.192.1.2:9999', 
	required_master_num = 3
}):wait(function(ok, r)
	if ok then
		print('initialize finish')
	else
		print('initialize failure', r)
		assert(false)
	end
end)

end 
