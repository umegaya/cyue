require('yuec')

yue.mcons = yue.run(function()
	local c = yue.open('udp://0.0.0.0:9999', { group = '239.192.1.1', ttl = 1 })
	-- local c = yue[{'udp://0.0.0.0:9999', { group = '239.192.1.1', ttl = 1 }}]
	local mcons -- connection array to yue master
	
	try(function() 
			c.timed_ping(5.0):callback(function(ok,r)
				if ok then
					mcons.insert(yue.open(r))
					if #mcons >= 3 then
						exit(mcons)
					end
				end
			end)
		end,
		function(e) -- catch
			print('cannot find enough number of master node', e)
			return false
		end,
		function() -- finally
		end
	)
end)

assert(#(yue.mcons) == 3)