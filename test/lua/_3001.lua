function test_control_jquery(selector, method)
	print("test_server_rpc", selector, method)
	return yue.core.peer().control_jquery(selector, method)
end

function __accepted(conn)
	print('accept connection', conn.__addr)
	local name,pass = nil, nil
	yue.core.try(function ()
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

function __closed(conn)
	print('closed connection:', conn.__addr)
end
