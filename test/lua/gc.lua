local yue = require('_inc')

function a(n, flag)
	local string = ("tcp://0.0.0.0:"  .. (1000 * n))
	if flag then
		return setmetatable({
			conn = yue.core.open(string)
		},{__gc = function (t)
				print(t.conn.__addr, "gc")
			end})
	else
		yue.core.remove_conn_cache(string)
	end
end

b = {}
for i=1,5,1 do
	b[i] = a(i, true)
	print(b[i].conn, b[i].conn.__c)	
end

-- print(yue.dev.inspect(b, 0))

print("gc1")
collectgarbage("collect")

for i=1,5,1 do
	a(i, false)
	b[i] = nil
end

print("gc2")
collectgarbage("collect")


print("collected!")
