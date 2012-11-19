local yue = require('_inc')

local b4 = 0
yue.client(function(cl)
	local cnt = 0
	local t = yue.timer(0.3, 0.5):bind('tick', function(t)
		if b4 == 0 then
			b4 = yue.util.time.now()
			print('b4', b4)
		end
		cnt = cnt + 1
		print('now:', yue.util.time.now())
		if cnt >= 5 then
			t:close()
			cl:exit(true, 0)
		end
	end)
end)
local aft = yue.util.time.now()
print('aft', aft)

local diff = ((aft - b4) - (2.0 * 1000.0 * 1000.0))
print('df', diff)
assert(math.abs(diff) <= (0.2 * 1000.0 * 1000.0))
