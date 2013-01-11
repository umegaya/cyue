local yue = require('_inc')

local b4,aft,diff

local tg = yue.taskgrp('test', 100, 1.0, 0.1)

yue.client(function(cl)
	local cnt = 0
	b4 = yue.util.time.now()
	print('b4', b4)
	local t = tg:alloc(3.0, 1.0)
	t:bind('tick', function(t)
		cnt = cnt + 1
		print('now:', yue.util.time.now())
		if cnt >= 3 then
			aft = yue.util.time.now()
			t:close()
			cl:exit(true, 0)
		end
	end)
end)
print('aft,b4', aft,b4,aft - b4)

diff = ((aft - b4) - (5.0 * 1000.0 * 1000.0))
print('df', diff)
assert(math.abs(diff) <= (2000.0))

b4 = 0
yue.client(function(cl)
	local cnt = 0
	local t = tg:alloc(0.3, 0.5):bind('tick', function(t)
		if b4 == 0 then
			b4 = yue.util.time.now()
			print('b4', b4)
		end
		cnt = cnt + 1
		print('now:', yue.util.time.now())
		if cnt >= 5 then
			aft = yue.util.time.now()
			t:close()
			cl:exit(true, 0)
		end
	end)
end)
print('aft,b4', aft,b4,aft - b4)

diff = ((aft - b4) - (2.0 * 1000.0 * 1000.0))
print('df', diff)
assert(math.abs(diff) <= (2000.0))


