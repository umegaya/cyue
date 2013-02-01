local yue = require('_inc')
local cl = ...
local c = yue.open('tcp://localhost:8888')
test_global = "testG"

assert(c.keepalive('string?') == 'string?')

local cnt = 0
local done = 0
local iter = 1000
while cnt < iter do
	c.async_keepalive(11,22,33):on(function (ok, r) 
		print(' -------------------------- run callback =---------------------')
		assert(r == 11)
		local r2 = c.keepalive(55,66,77)
		assert(r2 == 55)
		c.async_keepalive(22,33,44):on(function(ok, r3)
			assert(r3 == 22)
			done = done + 1
			if (done >= iter) then 
				print("------ all sub coroutine finished");
				cl:exit(true, iter)
			else
				print("------ sub coroutine finished:", done);
			end
		end)
	end)

	cnt = cnt + 1
end

