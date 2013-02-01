local yue = require('_inc')

yue.client(function(cl)
print(yue.open('tcp://localhost:9999').hello())
end)
