local yue = require '_inc'

yue.client(function (cl)
	yue.open('tcp://localhost:8888', {
		__close = function (socket)
			print('term: closed')
			cl:exit(true, nil)
		end,
	}).die()
end)


