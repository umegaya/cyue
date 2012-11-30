local yue = require('_inc')
local try = yue.try

yue.client(function (cl)
	local c = yue.open('tcp://0.0.0.0:8888').procs

	local err = false
	try{function ()
		c.timed_sleeper(1.0)
	end,
	catch = function ()
		err = true
	end,
	finally = function ()
		if not err then assert((not 'err should happen')) end
	end}
	
	err = false
	try{function ()
		c.timed_sleeper(3.0)
	end,
	catch = function ()
		err = true
	end,
	finally = function ()
		if err then assert((not 'err should not happen')) end
	end}
	return true
end)

