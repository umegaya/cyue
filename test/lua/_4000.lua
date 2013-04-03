local yue = require 'yue'

local conns = {}

function __accept(conn)
	print('connection accepted', conn:addr())
	conns[conn:addr()] = conn
	return true
end

function __close(conn)
	print('connection closed', conn:addr())
	conns[conn:addr()] = nil
end

local function cnum()
	local cnt = 0
	for a,c in pairs(conns) do
		cnt = (cnt + 1)
	end
	return cnt
end

function ping(t)
	return t
end

function twoconn_test()
	print('2c test create handle')
	while cnum() < 3 do
		yue.util.time.suspend(1.0)
		print('waiting keepaliver accept')
	end
	print('2c test conn established')
	for addr,c in ipairs(conns) do
		local ok, r = pcall(c.kill, 'kill by server:' .. addr)
		print(ok, r)
	end 	
	print('2c test kill connection')
	while cnum() > 1 do
		yue.util.time.suspend(1.0)
		print('waiting keepaliver disconnect')
	end
	print('finish')
	return true
end

