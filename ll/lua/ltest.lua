require('yuec')

yue.mcons = yue.run(function()
	local c = yue.open('mcast://239.192.1.2:9999', { ttl = 1 })
	-- local c = yue[{'udp://0.0.0.0:9999', { ttl = 1 }}]
	local mcons = {} -- connection array to yue master
	
	c.timed_get_hostname(5.0):callback(function(ok,r)
		print('callback', r)
		assert(r == 'tcp://localhost:8888')
		if ok then
			table.insert(mcons, yue.open(r))
			if #mcons >= 1 then
				exit(mcons)
			end
		else
			print('error happen', r)
			error(r)
		end
	end)
end)

assert(#(yue.mcons) == 1)