local print = __g.print
local assert = __g.assert

function hello_websocket(a, b, c, d)
	print("hello websocket called", a, b, c, d)
	return { msg = "hello! yue web socket", result = (a + b + c + d) }
end

function test_control_jquery(selector, method)
	print("test_server_rpc", selector, method)
	return yue.core.peer().control_jquery(selector, method)
end
