require('yuec')

print('-- test user data rpc ---------------------------------')
yue.client.run(function()
	require 'bignum'
	
	local bn = bignum.new("123456789123456789123456789")
	assert(not (bignum.__pack == nil or bignum.__unpack == nil))
	try(function ()
			local bn2 = yue.core.open('tcp://localhost:8888').keepalive(bn);
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
