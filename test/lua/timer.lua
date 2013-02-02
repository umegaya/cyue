local yue = require('_inc')

local b4,aft,diff

local resolution = ((yue.feature.timer == 'timerfd') and 2000.0 or 100000.0)

yue.client(function(cl)
	local cnt = 0
	b4 = yue.util.time.now()
	print('b4', b4)
	local t = yue.timer(3.0, 1.0):bind('tick', function(t)
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
assert(math.abs(diff) <= (resolution))

b4 = 0
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
			aft = yue.util.time.now()
			t:close()
			cl:exit(true, 0)
		end
	end)
end)
print('aft,b4', aft,b4,aft - b4)

diff = ((aft - b4) - (2.0 * 1000.0 * 1000.0))
print('df', diff)
assert(math.abs(diff) <= (resolution))


