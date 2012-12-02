local yue = require('yue')
local table = require('table')
local io = require('io')
local time = yue.util.time

-- local n_fiber, n_iter = 125, 1000
local n_fiber, n_iter = 1000, 500
local result = {}
local finished = 0
local n_fiber_1_percent = (n_fiber / 100)

local print_result = function (result)
	local elapsed = (time.now() - result.start)
	print('' .. result.total .. ' query in ' .. elapsed .. 'usec => ' .. (result.total * 1000000 / elapsed) .. ' qps')
end

local routines = {
	[1] = function ()
		local nc = yue.open('tcp://localhost:8888')
		local c = nc.procs
		for i=0,n_iter do
			local ok,r = pcall(c.f, i)
			if ok then
				assert(i == r)
			else
				print('error occurs:', r)
			end
		end
		nc:close()
		finished = (finished + 1)
		if finished >= n_fiber then
			print_result(result)
			yue.thread.current:emit('done')
		end
	end,
	[2] = function ()
		local nc = yue.open('tcp://localhost:8888')
		local c = nc.procs
		for i=0,n_iter do
			local ok,r = pcall(c.f, i)
			if ok then
				assert(i == r)
			else
				print('error occurs:', r)
			end
		end
		finished = (finished + 1)
		if (finished % (n_fiber_1_percent)) == 0 then
			io.write('.')
		end
		if finished >= n_fiber then
			print_result(result)
			nc:close()
			yue.thread.current:emit('done')
		end
	end,
	[3] = function ()
		local nc = yue.open('tcp://localhost:8888', {}, { no_cache = true } )
		local c = nc.procs
		for i=0,n_iter do
			local ok,r = pcall(c.f, i)
			if ok then
				assert(i == r)
			else
				print('error occurs:', r)
			end
		end
		nc:close()
		finished = (finished + 1)
		if finished >= n_fiber then
			print_result(result)
			yue.thread.current:emit('done')
		end
	end,
}

local function test(routine) 
	local fibers = {}
	finished = 0
	for i=1,n_fiber do
		table.insert(fibers, yue.fiber(routine))
	end
	
	result.start = time.now()
	result.total = n_fiber * n_iter
	
	for k,v in ipairs(fibers) do
		v:run():on(function(ok, r)
			-- print('result:', k, ok, r)
		end)
	end

	yue.thread.current:wait('done', 60)
end

if yue.mode == 'release' then
	test(routines[2])
else
	for k,v in ipairs(routines) do
		print('=================================================')
		print(k, 'start')
		test(v)
		yue.util.time.suspend(1.0)
	end
end

yue.thread.current:exit()
