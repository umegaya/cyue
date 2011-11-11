require('yuec')

print('-- test sync mode ----------------------------------------')
function sync_test()
	local conn = yue.open('tcp://localhost:8888')

	local function test(a, b, c)
		a = a * 2;
		b = b * 2;
		c = c * 2;
		return conn.keepalive(a, b, c)
	end
	
	assert(44 == test(22, 33, 55))
end
sync_test()



print('-- test aync mode ----------------------------------------')
do 
local c = yue.open('tcp://localhost:8888')	
yue.run(function()
	c.notify_error_test(4,4,2,1):callback(function(ok, r)
		print(ok, r[1], r[2]);
		exit "can handle error"
	end)
	exit(0)
end)
print('-- test aync mode2 ----------------------------------------')
local catched;
local ok,r = pcall(yue.run, function()
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
assert(catched[1] == r[1] and catched[2] == r[2])
assert(not ok)
print('-- test aync mode3 ----------------------------------------')
yue.run(function()
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
end

print('-- test exec file ----------------------------------------')
yue.exec('sandbox.lua')
-- sandbox.lua declares test_global in global manner, 
-- so if test_global is null in here, means isolation works
assert(test_global == null) 



print('-- test yue exit -----------------------------------------')
r = yue.run(function()
	local c = yue.open('tcp://localhost:8888')
	c.notify_keepalive('hogehoge'):callback(function(ok,r)
		assert(ok and (r == 'hogehoge'))
		exit "test exit"
		assert(false) -- should not reach here
	end)
	
	c.server_rpc_test(10)
end)
assert(r == "test exit")


print('-- test function rpc -------------------------------------')
function tester(_nil, _boolean, _integer, _string, _table)
	assert(type(_nil) == "nil")
	sum = 0
	for k,v in pairs(_table) do
		sum = (sum + v)
	end
	if _boolean then sum = (sum + 10000) end
	return _integer + #_string + sum
end

r = yue.run(function()
	local c = yue.open('tcp://localhost:8888')
	local v = c.test_func(nil, true, 1000, "string with 18byte", tester, 
		{ 100, 200, ['keys'] = 300 })
	assert(v == 11618)
	exit(100)
end)
assert(r == 100)

print('-- test exception system ---------------------------------')
yue.run(function()
	require 'bignum'
	
	local bn = bignum.new("123456789123456789123456789")
	assert(not (bignum.__pack == nil or bignum.__unpack == nil))
	try(function ()
			local bn2 = yue.open('tcp://localhost:8888').keepalive(bn);
			print('returned bignum', bn2)
			assert(bn == bn2)
		end,
		function(e)
			print('error catch!', e[1], e[2])
		end,
		function ()
		end
	)
	exit(nil)
end)

print('-- test udp multicast ---------------------------------')
yue.mcons = yue.run(function()
	local c = yue.open('mcast://239.192.1.2:9999', { ttl = 1 })
	-- local c = yue[{'udp://0.0.0.0:9999', { ttl = 1 }}]
	local mcons = {} -- connection array to yue master
	
	c.timed_get_hostname(5.0):callback(function(ok,r)
		print('callback', r)
		assert(r == 'tcp://localhost:8888')
		if ok then
			table.insert(mcons, yue.open(r))
			if #mcons >= 1 then
				exit(mcons)
			end
		else
			print('error happen', r)
			error(r)
		end
	end)
end)

assert(#(yue.mcons) == 1)

