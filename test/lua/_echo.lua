
while true do 
	local str = io.stdin:read()
	io.stderr:write('readline:', str .. '\n')
	io.stdout:write(str .. '\n')
end
