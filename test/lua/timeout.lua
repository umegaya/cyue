local yue = require('_inc')

yue.client(function(cl)
print(yue.open('tcp://localhost:9999').hello())
end)

local ok, r = yue.client(function(cl)
	local c = yue.open('tcp://localhost:8888')
	local cause_error = false
	yue.try { 
		function ()
			c.sleeper(10)
		end,
	catch = function (e)
			cause_error = true
			print(e)
			assert(e:is_a("TimeoutError"))
			yue.try {
				function ()
					c.sleeper(1)
				end,
			catch = function (e)
					assert(false)
				end,
			finally = function ()
				end
			}
			
		end,
	finally = function ()
		assert(cause_error)
		end,
	}
	return true
end)

assert(ok and r)
