local yue = require '_inc'

local acc = 'umegaya'
local pass = 'umegaya'

local r = yue.client.run(function()
	local c = yue.core.open('tcp://localhost:4000', {
		symbols = {
			__accept = function (conn, r)
				assert(r == 'umegaya')
				return true
			end,
			__close = function (conn)
				exit('closed')
			end,
			get_account_info = function ()
				return acc,pass
			end,
		}
	})
	c.greeting('hello server!')
	c.close()
end)
assert(r == 'closed')