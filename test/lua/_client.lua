local yue = require('yue')
local table = require('table')
local math = require('math')
local time = yue.util.time

local fibers = {}
local finished = 0
local n_fiber, n_iter = math.ceil(1000 / yue.thread.count()), 1000

for i=0,n_fiber do
	table.insert(fibers, yue.fiber(function ()
		local c = yue.open('localhost:8888')
		for i=0,n_iter do
			assert(i == c.f(i))
		end
		finished = (finished + 1)
		if finished >= n_fiber then
			yue.thread.current:__close()
		end
	end))
end
local start = time.now()
local total = n_fiber * n_iter

for k,v in ipairs(fibers) do
	v:run()
end

yue.thread.current:__bind('join', function ()
	local elapsed = (time.now() - start)
	print('' .. total .. ' query in ' .. elapsed .. 'usec => ' .. (total * 1000000 / elapsed) .. ' qps')
end)