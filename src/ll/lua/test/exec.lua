require('yuec')


print('-- test exec file ----------------------------------------')
yue.client.exec('test/_executable.lua')
-- sandbox.lua declares test_global in global manner, 
-- so if test_global is null in here, means isolation works
assert(test_global == null) 
