require 'yuec'

yue.util.sht.test = [[
	int x, y, z;
	char buffer[256];
	double f;
]]

yue.util.sht.test.x = 10
yue.util.sht.test.y = 20
yue.util.sht.test.z = 30
yue.util.sht.test.buffer = "testtest"
yue.util.sht.test.f = 6.6

assert(10 == yue.util.sht.test.x)
assert(20 == yue.util.sht.test.y)
assert(30 == yue.util.sht.test.z)
assert("testtest" == yue.ffi.string(yue.util.sht.test.buffer))
assert(6.6 == yue.util.sht.test.f)
