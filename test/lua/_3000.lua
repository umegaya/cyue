local yue = require 'yue'

function hello_websocket(a, b, c, d)
	print("hello websocket called", a, b, c, d)
	return { msg = "hello! yue web socket", result = (a + b + c + d) }
end

function test_control_jquery(selector, method)
	print("test_server_rpc", selector, method)
	return yue.peer().procs.control_jquery(selector, method)
end
