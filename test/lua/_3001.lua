local yue = require 'yue'

function test_control_jquery(selector, method)
	print("test_server_rpc", selector, method)
	return yue.peer().control_jquery(selector, method)
end

function __accept(conn)
	print('accept connection', conn:__addr())
	local name,pass = nil, nil
	yue.try(function ()
			-- ask client to input account info with in 60sec
			print('auth challenge')
			name,pass = conn.get_account_info('server required authentification')
			print('credential=',name,pass)
			if name ~= pass then
				name = nil
			end
		end,
		function (e)
			print(e) 
		end,
		function ()
		end)
	return name
end

function __close(conn)
	print('close connection:', conn:__addr())
end
