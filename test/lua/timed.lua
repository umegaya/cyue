local yue = require('_inc')

yue.client.run(function ()
	local c = yue.core.open('tcp://0.0.0.0:8888')
	local err = false
	try(function ()
		c.timed_sleeper(1.0)
	end,
	function ()
		err = true
	end,
	function ()
		if not err then assert('err should happen') end
	end)
	err = false
	try(function ()
		c.timed_sleeper(3.0)
	end,
	function ()
		err = true
	end,
	function ()
		if err then assert('err should not happen') end
	end)
	exit(nil)
end)

