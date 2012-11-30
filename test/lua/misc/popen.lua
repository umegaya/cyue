local yue = require '_inc'

print('-- test popen  ---------------------------------')
yue.client.run(function()
	s = yue.dev.socket('popen:///bin/ls -al')
	r = ''
	p = yue.ffi.new('char[256]')
	while true do
		sz = 256
		sz = s:read(p, sz)
		if not sz then break end
		r = (r .. yue.ffi.string(p,sz))	
	end
	print('ls -al result', '[' .. r .. ']')
	exit(nil)
end)

yue.client.run(function()
	s = yue.dev.socket('popen:///bin/sh')
	p = yue.ffi.new('char[256]')
	local cmds = [[
		touch test.file
		echo 'testtesttest' >> test.file
		cat test.file 
		rm test.file
	]]
	yue.ffi.copy(p, cmds)
	s:write(p, $cmds)
	r = ''
	while true do
		sz = 256
		sz = s:read(p, sz)
		if not sz then break end
		r = (r .. yue.ffi.string(p,sz))	
		break
	end
	assert(r == 'testtesttest\n')
	exit(nil)
end)

