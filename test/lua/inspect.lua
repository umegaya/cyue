local yue = require '_inc'

local r = yue.dev.inspect({
	[1] = 	-7,
	[2] =     '_4000.lua:48: attempt to index field \'peer\' (a function value)',
	test = {
		x = false,
		y = nil,
		z = {
			w = function (t)
				print(t)
			end,
			[123] = 'abc',
			[456] = 789,
			a = {
				"xxxx",
				"yyyy",
			},
		},
	}
}, 0)

print(r)

local result = [[{
    [1] => -7,
    [2] => _4000.lua:48: attempt to index field 'peer' (a function value),
    [test] => {
        [z] => {
            [a] => {
                [1] => xxxx,
                [2] => yyyy,
            },
            [w] => (function),
            [123] => abc,
            [456] => 789,
        },
        [x] => false,
    },
}]]

assert(r == result)
