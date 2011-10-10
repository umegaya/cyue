require('yue')

--	@name: yue.bless
--	@desc: create class object which is rpc-callable, auto-save-to-KVS
-- 	@args: class: class name
-- 	@rval: table object which represent class of given name
yue.bless = function (class)
	if _G[class] then 
		return _G[class]
	end
	local c = {}
	local mt = {
		__index = function (t, k) 
			return c[k]
		end
	}
	local rpc_mt = {
		__call = function (self, ...)
			return self.actor.rpc(self.uuid, self.k, ...)
		end
	}
	local method_mt = {
		__index = function (obj, key)
			local v = obj[key]
			if v then return v end
			v = {
				uuid = obj.__uuid,
				k = key,
				actor = yue.hspace.from(obj.__uuid)
			}
			setmetatable(v, rpc_mt)
			obj[key] = v
			return v;
		end
	}	
	c.new = function (...)
		local v = {
			__uuid = obj.__uuid
		}
		if yue.hspace.from(v.uuid) == yue.current then
			setmetatable(v, mt)
		else
			setmetatable(v, method_mt)
		end
		v:initialize(...)
		return v
	end
	_G[class] = c
	return c
end
