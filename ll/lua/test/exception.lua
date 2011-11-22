require('yuec')

local c = yue.core.open('tcp://localhost:8888')	

print('-- test exception handling ----------------------------------------')
yue.client.run(function()
	local finally_execute = 0
	try(function () 
		try(function () 
			local r = c.error_test(1,2,4,4)
		end,
		function (e) -- catch
			print("catch error2", e[1], e[2])
			finally_execute = (finally_execute + 1);
			return false
		end,
		function () -- finally
			finally_execute = (finally_execute + 1);
		end)
	end,
	function (e) -- catch
		print("catch error3", e[1], e[2])
		finally_execute = (finally_execute + 1);
		return true
	end,
	function () -- finally
		finally_execute = (finally_execute + 1);
	end)	
	assert(finally_execute == 4)
	exit "can handle error"
end)
