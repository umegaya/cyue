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
			print('================= connection closed', closed)
			if closed == -1 then
				print('================= exit')
				cl:exit(true, closed)
			end
			closed = (closed + 1)
			if closed >= 0 and closed <= 37 then
				-- try connecting again
				return true
			else
				assert(pass == 'umegayax') -- wrong password
				pass = 'umegaya' -- fix to correct password
				-- try connecting again
				return true
			end
		end,
		-- it called from server's accept watcher
		get_account_info = function ()
			return acc,pass
		end,
	})
	-- test rpc is lazy enabled with server auth and auto reconnection
	assert('you are welcome' == c.procs.greeting('hello server!'))
	closed = -1
	yue.util.time.suspend(1.0)
	c:close()
end)
print(ok, r)
assert(ok and r == closed)