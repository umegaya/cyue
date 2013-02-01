local yue = require('_inc')
local try = yue.try

yue.client(function (cl)
	local c = yue.open('tcp://0.0.0.0:8888')
	
	local err = false
	try{function ()
		c.assert(false)
	end,
	catch = function ()
		err = true
	end,
	finally = function ()
		if not err then assert((not 'err should happen')) end
	end}

	err = false
	try{function ()
		c._error_test2(1.0)
	end,
	catch = function ()
		err = true
	end,
	finally = function ()
		if not err then assert((not 'err should happen')) end
	end}

	err = false
	try{function ()
		c.not_exist(1.0)
	end,
	catch = function ()
		err = true
	end,
	finally = function ()
		if not err then assert((not 'err should happen')) end
	end}
	
	return true
end)