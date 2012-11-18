local yue = require '_inc'

local acc = 'umegaya'
local pass = 'umegayax'  -- wrong password
local closed = 0
print('-- test accept/close watcher  ----------------------------------------')
local ok, r = yue.client(function(cl)
	local c = yue.open('ws://localhost:3001', {
		__accept = function (conn, r)
			print('connection accepted', r)
			assert(r == 'umegaya')
			return true
		end,
		__close = function (conn)
			print('connection closed')
			closed = (closed + 1)
			if closed == 1 then
				assert(pass == 'umegayax') -- wrong password
				pass = 'umegaya' -- fix to correct password
				-- try greeting again
				assert('you are welcome' == conn.procs.greeting('hello server!'))
			end
			-- conn.close_me()
			if closed >= 2 then
				cl:exit(true, closed)
			end
		end,
		-- it called from server's accept watcher
		get_account_info = function ()
			print('get a i')
			return acc,pass
		end,
	})
	-- test rpc is lazy enabled with server auth and auto reconnection
	assert('you are welcome' == c.procs.greeting('hello server!'))
	c:close()
end)
assert(ok and r == 2)