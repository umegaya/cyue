require 'yuec'

yue.client.run(function()
	s = yue.dev.socket('popen://ls -al')
	r = ''
	p = yue.ffi.new("char[256]")
	while true do
		sz = 256
		sz = s:read(p, sz)
		if not sz then break end
		r = (r .. yue.ffi.string(p,sz))	
	end
	print('ls -al result', '[' .. r .. ']')
	exit(nil)
end)


