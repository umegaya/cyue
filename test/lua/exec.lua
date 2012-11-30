local yue = require('_inc')


print('-- test exec file ----------------------------------------')
local ok, r = yue.client('_executable.lua')
-- _executable.lua declares test_global in global manner, 
-- so if test_global is null in here, means isolation works
assert(test_global == null) 
print(ok, r)
assert(ok and r == 1000)
