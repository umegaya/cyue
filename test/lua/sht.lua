require '_inc'

yue.util.sht.test = {
	[[
		int x, y, z;
		char buffer[256];
		double f;
	]],
	function (t)
		t.x = 10
		t.y = 20
		t.z = 30
		t.buffer = "testtest"
		t.f = 6.6
	end
}

assert(10 == yue.util.sht.test.x)
assert(20 == yue.util.sht.test.y)
assert(30 == yue.util.sht.test.z)
assert("testtest" == yue.ffi.string(yue.util.sht.test.buffer))
assert(6.6 == yue.util.sht.test.f)
