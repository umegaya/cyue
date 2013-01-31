local yue = require 'yue'

function test_control_jquery(selector, method)
	print("test_server_rpc", selector, method)
	return yue.peer().procs.control_jquery(selector, method)
end

local cnt = 0
function __accept(conn)
	print('accept connection', conn:addr())
	cnt = cnt + 1
	if cnt < 10 then
		print('raise error', cnt)
		error('emulate some error:' .. cnt)
	else
		conn.procs.error_recover()
	end
	print('yue.try')
	local name,pass = nil, nil
	yue.try { 
		function ()
			-- ask client to input account info within 60sec
			print('auth challenge')
			-- name,pass = conn.procs.procs.get_account_info('server required authentification')
			name,pass = conn.procs.get_account_info('server required authentification')
			print('credential=',name,pass)
			if name ~= pass then
				name = nil
			end
		end,
		catch = function (e)
			print(e) 
		end,
		finally = function ()
		end
	}
	print('return name = ', name)
	return name
end

function greeting(msg)
	assert(msg == 'hello server!')
	return 'you are welcome'
end

function __close(conn)
	print('user close')
	print('close connection:', conn:addr())
end
