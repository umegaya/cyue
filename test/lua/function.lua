require('_inc')

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

r = yue.client.run(function()
	local c = yue.core.open('tcp://localhost:8888')
	local v = c.test_func(nil, true, 1000, "string with 18byte", tester, 
		{ 100, 200, ['keys'] = 300 })
	assert(v == 11618)
	exit(100)
end)
assert(r == 100)
