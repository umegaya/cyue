local yue = require '_inc'

if not yue.ffi then
	assert(false)
	return
end

yue.ffi.cdef [[
	typedef int pid_t;
	pid_t getpid();
]]

local ok, r = yue.client(function (cl)

yue.signal(yue.constants.SIGPIPE):bind('signal', function (sig)
	cl:exit(true, "end with sigpipe")
end)

yue.log.debug('kill -' .. yue.constants.SIGPIPE .. ' ' .. yue.ffi.C.getpid())

io.popen('kill -' .. yue.constants.SIGPIPE .. ' ' .. yue.ffi.C.getpid())

end)

assert(ok and r == "end with sigpipe")