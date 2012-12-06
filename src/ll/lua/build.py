import subprocess
from pprint import pprint

Import('env')
Import('build')
Import('config')
Import('linkflags')

if env['PLATFORM'] == 'darwin':
	config["linkflags"]["bin"] += ["-pagezero_size", "10000", "-image_base", "100000000"]

# make seems to execute on project root dir
path = subprocess.Popen(["pwd"], stdout=subprocess.PIPE).communicate()[0].rstrip()
pprint(path + "/exlib/luajit/")
code = subprocess.call(["make", "-C", path + "/exlib/luajit/"])
if code != 0:
	raise BaseException("fail to build luaJIT:" + str(code))
	
try:
	code = subprocess.call(["luajit", "-v"])
except OSError: # luaJIT not installed
	print("luaJIT not installed: install our one")
	# make seems to execute on project root dir
	code = subprocess.call(["make", "-C", path + "exlib/luajit", "install"])
	if code != 0:
		raise BaseException("fail to install luaJIT:" + str(code))

env.Append(LIBS = ['dl', 'libluajit'])
env.Append(LIBPATH = [path + '/exlib/luajit/src/'])
lobjs = env.SharedObject(Glob("*.cpp"))

config["name"]["bin"] = "yue"
libs = ["yue.lua", "exlib/debugger/debugger.lua", "exlib/serpent/src/serpent.lua"]
Return("lobjs", "libs")
