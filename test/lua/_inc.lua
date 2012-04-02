-- package.path = ('../../bin/?.lua;../../bin/?.so;' .. package.path)
package.path = ('../../bin/?.lua;' .. package.path)
package.cpath = ('../../bin/?.so;../../bin/?.dylib;' .. package.cpath)
return require 'yue'
