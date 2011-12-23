require 'yuec'

yue.ffi.cdef [[
	int socket_read(void *, char *, size_t);
	int socket_sys_write(void *, char *, size_t);
]]
local ylib = yue.ffi.load('./yue.so')

yue.client.run(function()
	s = yue.dev.socket('popen:///bin/ls -al')
	r = ''
	p = yue.ffi.new('char[256]')
	while true do
		sz = 256
		sz = s:try_read(function (sk)
			print('read from fd', s:fd(), ylib.socket_read)
			return ylib.socket_read(sk, p, sz)
		end)
		if not sz then break end
		r = (r .. yue.ffi.string(p,sz))	
	end
	print('ls -al result', '[' .. r .. ']')
	exit(nil)
end)

if true then
yue.client.run(function()
	s = yue.dev.socket('popen:///bin/sh')
	p = yue.ffi.new('char[256]')
	s:try_write(function (sk)
		print('try_write:', s:fd())
		local cmds = [[
			touch test.file
			echo 'testtesttest' >> test.file
			cat test.file 
			rm test.file
		]]
		yue.ffi.copy(p, cmds)
		return ylib.socket_sys_write(sk, p, #cmds)
	end)
	r = ''
	while true do
		sz = 256
		sz = s:try_read(function (sk)
			return ylib.socket_read(sk, p, sz)
		end)
		if not sz then break end
		r = (r .. yue.ffi.string(p,sz))	
		break
	end
	print(r)
	assert(r == 'testtesttest\n')
	exit(nil)
end)
end

