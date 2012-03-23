yue.util.sht.auth = {
	'bool authed;',
	function (t)
		t.authed = false
	end
}

function __accepted(conn)
	print('accept connection')
	local name,pass = nil, nil
	yue.core.try(function ()
			-- ask client to input account info with in 60sec
			print('auth challenge')
			name,pass = conn.get_account_info('server required authentification')
			print('credential=',name,pass)
			if name == pass then
				yue.util.sht.auth.authed = true
			else
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
	print('connection closed')
end

function greeting(msg)
	print('greeting', yue.util.sht.auth.authed)
	assert(yue.util.sht.auth.authed)
	assert(msg == 'hello server!')
	return 'you are welcome'
end

function close_me()
	yue.core.close(yue.core.peer())
end
