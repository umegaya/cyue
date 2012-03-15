local g_conn = yue.core.open('tcp://localhost:8888')

function server_rpc_test(num)
	local cnt = 0
	while cnt < num do
		assert(100 == g_conn.ping(100))
		cnt = cnt + 1
	end
	return num
end
