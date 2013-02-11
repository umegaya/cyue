local yue = require('_inc')

local ok, r = yue.client(function (cl)
	local t = yue.thread('test_worker', './_thread.lua')
	local procs = yue.open('thread://test_worker')
	local tmp = 0
	assert('domo' == procs.g())
	for i=1,100 do
		tmp = t.procs.f(i)
		assert(i == tmp)
	end
	return tmp
end)

assert(ok and r == 100)
