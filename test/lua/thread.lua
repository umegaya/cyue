local yue = require('_inc')

local ok, r = yue.client(function (cl)
	local t = yue.thread('test_worker', './_thread.lua')
	local tmp = 0
	print('call trpc:', t.__ptr)
	for i=1,100 do
		tmp = t.procs.f(i)
		assert(i == tmp)
	end
	return tmp
end)

assert(ok and r == 100)
