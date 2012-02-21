local yue = require('_inc')


print('-- test exec file ----------------------------------------')
yue.client.exec('_executable.lua')
-- sandbox.lua declares test_global in global manner, 
-- so if test_global is null in here, means isolation works
assert(test_global == null) 
