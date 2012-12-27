local yue = require('_inc')
local c = yue.open('tcp://localhost:8888').procs

yue.thread.current:import({
	f = function (i)
		return i
	end,
	g = function ()
		return c.greeting()
	end,
})
