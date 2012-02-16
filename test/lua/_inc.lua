-- package.path = ('../../bin/?.lua;../../bin/?.so;' .. package.path)
package.path = ('../../bin/?.lua;' .. package.path)
package.cpath = ('../../bin/?.so;' .. package.cpath)
return require 'yue'
