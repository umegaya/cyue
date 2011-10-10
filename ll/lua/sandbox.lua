local c = yue.connect('tcp://localhost:8888')
test_global = "testG"

assert(c.keepalive('string?') == 'string?')

local cnt = 0
local done = 0
while (cnt < 10) do
	c.notify_keepalive(11,22,33):callback(function (ok, r) 
		assert(r == 11)
		local r2 = c.keepalive(55,66,77)
		assert(r2 == 55)
		c.notify_keepalive(22,33,44):callback(function(ok, r3)
			assert(r3 == 22)
			done = done + 1
			if (done >= 10) then 
				print("------ all sub coroutine finished");
				yue.exit(100)
			else
				print("------ sub coroutine finished:", done);
			end
		end)
	end)

	cnt = cnt + 1
end

