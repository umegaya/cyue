local yue = require('_inc')

print('-- test aync mode ----------------------------------------')
do 
local c = yue.core.open('tcp://localhost:8888')	
yue.client.run(function()
	c.notify_error_test(4,4,2,1):callback(function(ok, r)
		print(ok, r[1], r[2]);
		exit "can handle error"
	end)
	exit(0)
end)
print('-- test aync mode2 ----------------------------------------')
local catched;
local ok,r = pcall(yue.client.run, function()
	local finally_execute = 0
	try(function () 
		local r = c.error_test(1,2,4,4)
	end,
	function (e) -- catch
		catched = e
		print("catch error", e[1], e[2])
		finally_execute = (finally_execute + 1);
		return false
	end,
	function () -- finally
		finally_execute = (finally_execute + 1);
		assert(finally_execute == 2)
	end)
end)
assert(catched and catched[1] == r[1] and catched[2] == r[2])
assert(not ok)
end
