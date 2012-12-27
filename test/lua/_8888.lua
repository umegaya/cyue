local yue = require('yue')

assert(print and assert and error)

x = {
	y = {
		z = {
			f = function (num)
				assert(num == 100)
				return 'abcf' .. num
			end,
			g = function (num)
				assert(num == 100)
				return 'abcg' .. num
			end,
			secret_data = 'dr56yhu89k2',
			h = function(self, num)
				assert(num == 200)
				return self.secret_data .. num
			end
		},
		w = function(num)
			assert(num == 100)
			return 'abg' .. num
		end
	},
	_y = {
		z = {
			f = function (num)
				assert(num == 100)
				return 'abcf' .. num
			end
		}
	},
}

function f(i)
	return i
end

function die()
	print('die:') 
	yue.die()
end

function gc_test()
	local byte = collectgarbage("count")
	print(byte, 'kbyte used')
	return byte
end

function error_test(a,b,c,d)
	print(a,b,c,d);
	error("test error!!");
end

function _error_test2()
	return "cannot call it!!!!!"
end

function greeting()
	return 'domo'
end

function keepalive(tm)
	a = 1;
	b = 2;
	c = 3;
	return tm
end

function keepalive2(tm, sleep_sec)
	if sleep_sec then
		yue.util.time.suspend(sleep_sec)
	end
	return tm
end

function test_func(_nil, _boolean, _integer, _string, _function, _table)
	if not type(_nil) == "nil" then error("invalid nil") end
	if not type(_boolean) == "boolean" then error("invalid boolean") end
	if not type(_integer) == "number" then error("invalid integer") end
	if not type(_string) == "string" then error("invalid string") end
	if not type(_function) == "function" then error("invalid function") end
	if not type(_table) == "table" then error("invalid table") end
	return _function(_nil, _boolean, _integer, _string, _table)
end

function ping(time)
	return time
end

function sleeper()
	print('=============================== sleeper called')
	local b4 = yue.util.time.now()
	yue.util.time.suspend(2.0)
	local aft = yue.util.time.now()
	local diff = ((aft - b4) - (2.0 * 1000.0 * 1000.0))
	print('df', diff, b4, aft, (aft - b4))
end
