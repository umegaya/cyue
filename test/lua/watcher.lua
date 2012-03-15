local yue = require '_inc'

local acc = 'umegaya'
local pass = 'umegayax'  -- wrong password
local closed = 0
local accept = 0
print('-- test accept/close watcher  ----------------------------------------')
local r = yue.client.run(function()
	local c = yue.core.open('tcp://localhost:4000', {
		symbols = {
			__accepted = function (conn, r)
				print('connection accepted', r)
				assert(r == 'umegaya')
				return true
			end,
			__closed = function (conn)
				print('connection closed')
				closed = (closed + 1)
				if closed == 1 then
					assert(pass == 'umegayax') -- wrong password
					pass = 'umegaya' -- fix to correct password
					-- try greeting again
					assert('you are welcome' == conn.greeting('hello server!'))
				end
				-- conn.close_me()
				if closed >= 2 then
					exit(closed)
				end
			end,
			-- it called from server's accept watcher
			get_account_info = function ()
				return acc,pass
			end,
		}
	})
	-- test rpc is lazy enabled with server auth and auto reconnection
	assert('you are welcome' == c.greeting('hello server!'))
	c.close_me()
end)
assert(r == 2)