local yue = require '_inc'
yue.client(function (cl)
	local c = yue.open('udp://localhost:7777')
	c.server_rpc_test(10)
	yue.try { 
		function ()
			c.server_rpc_error_test()
		end,
	catch = function (e)
			print(e)
		end,
	finally = function ()
	end
	}
	return true
end)
