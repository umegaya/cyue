function keepalive(tm)
	a = 1;
	b = 2;
	c = 3;
	return tm
end

function test_func(_nil, _boolean, _integer, _string, _function, _table)
	if not type(_nil) == "nil" then error("invalid nil") end
	if not type(_boolean) == "boolean" then error("invalid boolean") end
	if not type(_integer) == "number" then error("invalid integer") end
	if not type(_string) == "string" then error("invalid string") end
	if not type(_function) == "function" then error("invalid function") end
	if not type(_table) == "table" then error("invalid table") end
	return _function(_nil, _boolean, _integer, _string, _table)
end

function error_test(a,b,c,d)
	print(a,b,c,d);
	error("test error!!");
end

yue.listen('tcp://0.0.0.0:8888')
print("listen@tcp://0.0.0.0:8888")

yue.listen('udp://0.0.0.0:7777')
print("listen@udp://0.0.0.0:7777")

yue.listen('mcast://239.192.1.2:9999', { ttl = 1 })
-- or yue.listen('udp://0.0.0.0:9999', { group = '239.192.1.2', ttl = 1 }) also work
print("listen@mcast://239.192.1.2:9999")


g_conn = yue.connect('tcp://localhost:8888')

function server_rpc_test(num)
	local cnt = 0
	while (cnt < num) do
		assert(100 == g_conn.ping(100))
		cnt = cnt + 1
	end
	return num
end

function ping(time)
	return time
end

function get_hostname()
	return 'tcp://localhost:8888'
end