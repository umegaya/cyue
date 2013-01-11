import subprocess
from pprint import pprint

Import('env')
Import('build')
Import('config')
Import('linkflags')

if env['PLATFORM'] == 'darwin':
	config["linkflags"]["bin"] += ["-pagezero_size", "10000", "-image_base", "100000000"]

# make seems to execute on project root dir
path = Dir('.').srcnode().abspath 
pprint(path + "/exlib/luajit/")
code = subprocess.call(["make", "-C", path + "/exlib/luajit/"])
if code != 0:
	raise BaseException("fail to build luaJIT:" + str(code))
	
try:
	code = subprocess.call(["luajit", "-v"])
except OSError: # luaJIT not installed
	print("luaJIT not installed: install our one")
	# make seems to execute on project root dir
	code = subprocess.call(["sudo", "make", "-C", path + "/exlib/luajit", "install"])
	if code != 0:
		raise BaseException("fail to install luaJIT:" + str(code))

try:
	code = subprocess.call(["luarocks", "path"])
except OSError: # luarocks not installed
	print("luarocks not installed: install our one")
	luarocks_make = ["sudo", "make", "-C", path + "/exlib/luarocks"]
	commands = [["sudo", path + "/exlib/luarocks/configure", "--with-lua=lua", "--prefix=/usr/local/", "--lua-suffix=jit", "--with-lua-include=/usr/local/include/luajit-2.0", "--force-config"], luarocks_make, luarocks_make + ["install"]]
	for command in commands:
		code = submodule.call(command)
		if code != 0:
			raise BaseException("cmd fails:" + str(command) + ":" + str(code))
	
env.Append(LIBS = ['dl', 'libluajit'])
env.Append(LIBPATH = [path + '/exlib/luajit/src/'])
lobjs = env.SharedObject(Glob("*.cpp"))

config["name"]["bin"] = "yue"
libs = [
	["yue.lua", "share/lua/5.1"],
	["exlib/debugger/debugger.lua", "share/lua/5.1"],
	["exlib/serpent/src/serpent.lua", "share/lua/5.1"]
]
Return("lobjs", "libs")
