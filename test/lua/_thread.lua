local yue = require('_inc')

yue.thread.current:import({
	f = function (i)
		return i
	end
})

