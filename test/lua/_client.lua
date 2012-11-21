local yue = require('yue')
local table = require('table')
local time = yue.util.time

local fibers = {}
local finished = 0
-- local n_fiber, n_iter = 125, 1000
local n_fiber, n_iter = 100, 1000

for i=1,n_fiber do
	table.insert(fibers, yue.fiber(function ()
		local nc = yue.open('tcp://localhost:8888')
		local c = nc.procs
		for i=0,n_iter do
			assert(i == c.f(i))
		end
		nc:close() -- c.__emitter:close()
		finished = (finished + 1)
		if finished >= n_fiber then
			yue.thread.current:close()
		end
	end))
end
local start = time.now()
local total = n_fiber * n_iter

for k,v in ipairs(fibers) do
	print('run fiber:', k)
	v:run()
	print('end fiber:', k)
end

yue.thread.current:bind('join', function ()
	local elapsed = (time.now() - start)
	print('' .. total .. ' query in ' .. elapsed .. 'usec => ' .. (total * 1000000 / elapsed) .. ' qps')
end)
