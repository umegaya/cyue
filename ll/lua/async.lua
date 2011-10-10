require('yue')
yue.orig_connect = yue.connect
yue.connect = function(addr) 
	local r = { conn = yue.orig_connect(addr) }
	setmetatable(r, {
		__index = function(t, k) 
			print("__index:", k)
			local cr = { 
				co = yue.newthread(function (...)
					print("new thread resume: ", k)
					return t.conn[k](...)
				end) 
			}
			setmetatable(cr, {
				__call = function(f,...)
					local ret = yue.resume(f.co, ...)
					return ret
				end
			})
			return cr
		end
	})
	return r
end

yue.sync_mode(false)
c = yue.connect('tcp://localhost:8888')
nfunc = c.notify_keepalive

cnt = 0
while (cnt < 10) do
	alive = true
	nfunc(11, 22, 33):callback(function (ok, r) 
		assert(r == 11)
		local r2 = c.conn.keepalive(55,66,77)
		assert(r2 == 55)
		print("notify_ka inside future callback")
		c.conn.notify_keepalive(22,33,44):callback(function(ok, r3)
			assert(r3 == 22)
			alive = false	
		end)
	end)

	while (alive) do
		yue.poll();
	end
	cnt = cnt + 1
	print("future has come!!", cnt)
end

