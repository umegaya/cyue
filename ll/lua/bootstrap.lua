function keepalive(tm)
	a = 1;
	b = 2;
	c = 3;
	print("keepalive time = ", tm);
	return tm
end
function error_test(a,b,c,d)
	print(a,b,c,d);
	error("test error!!");
end

print("listen@tcp://0.0.0.0:8888")
yue.listen('tcp://0.0.0.0:8888')
print("listen@tcp://0.0.0.0:7777")
yue.listen('tcp://0.0.0.0:7777')
g_conn = yue.connect('tcp://0.0.0.0:7777')

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