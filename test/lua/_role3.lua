local yue = require('_inc')

yue.core.listen('tcp://0.0.0.0:1111').namespace:import({
	get_name = function () 
		return 'role3'
	end
})