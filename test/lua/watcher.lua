local yue = require '_inc'

local acc = 'umegaya'
local pass = 'umegayax'  -- wrong password
local closed = 0
local error_recovered = false
print('-- test accept/close watcher  ----------------------------------------')
local ok, r = yue.client(function(cl)
	local c = yue.socket('ws://localhost:3001', {
		__accept = function (conn, r)
			print('connection accepted', r)
			assert(r == 'umegaya')
			return true
		end,
		error_recover = function ()
			print('~~~~~~~~~~~~~~~~~ errror recovered!!', r)
			error_recovered = true
		end,
		__close = function (conn)
			print('================= connection closed', closed)
			if closed == -1 then
				print('================= exit')
				cl:exit(true, closed)
			end
			closed = (closed + 1)
			if not error_recovered then
				-- try connecting again
				return true
			else
				assert(pass == 'umegayax', pass .. ' is not same as umegayax') -- wrong password
				pass = 'umegaya' -- fix to correct password
				print('~~~~~~~~~~~~~~~~~~~~~~~ fix password now', closed)
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
	assert('you are welcome' == c.procs.timed_greeting(60, 'hello server!'))
	closed = -1
	print('set closed = -1')
	yue.util.time.suspend(1.0)
	print('suspend finished')
	c:close()
	print('close called')
end)
print(ok, r)
assert(ok and r == closed)
