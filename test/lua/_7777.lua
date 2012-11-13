local yue = require 'yue'

local g_conn = yue.open('tcp://localhost:8888', { no_cache = true })

function server_rpc_test(num)
	local cnt = 0
	while cnt < num do
		assert(100 == g_conn.ping(100))
		cnt = cnt + 1
	end
	return num
end
