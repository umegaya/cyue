local yue = require('_inc')

print('-- test exception handling ----------------------------------------')
yue.client(function(cl)
	local c = yue.open('tcp://localhost:8888')
	local finally_execute = 0
	yue.try{function () 
		yue.try{function () 
			local r = c.error_test(1,2,4,4)
		end,
		catch = function (e)
			print('---------------------- catch1')
			yue.pp(e)
			finally_execute = (finally_execute + 1);
			error(e) -- propagate error
		end,
		finally = function ()
			print('---------------------- catch2')
			finally_execute = (finally_execute + 1);
		end
		}
	end,
	catch = function (e)
		print('---------------------- catch3')
		yue.pp(e)
		finally_execute = (finally_execute + 1);
		return true
	end,
	finally = function ()
		print('---------------------- catch4')
		finally_execute = (finally_execute + 1);
	end}	
	assert(finally_execute == 4)
	return true
end)
