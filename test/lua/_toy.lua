local yue = require('_inc')
c = yue.core.open('tcp://0.0.0.0:8888')
print(c.gc_test(), 'Kbyte used')
