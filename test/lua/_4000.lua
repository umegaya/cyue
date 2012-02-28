local print = __g.print
local assert = __g.assert
local error = __g.error
local type = __g.type


local authed = false

function __accepted(conn)
	print('accept connection')
	return try(function ()
			-- ask client to input account info with in 60sec
			local name,pass = conn.timed_get_account_info(60, 'server required authentification')
			if name == pass then
				authed = true
				return name
			else
				return nil
			end
		end,
		function () 
		end,
		function ()
		end)
end

function __closed(conn)
	print('connection closed')
end

function greeting(msg)
	assert(authed)
	assert(msg == 'hello server!')
end
