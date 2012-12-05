Import('env')
Import('build')
Import('config')
Import('linkflags')

if env['PLATFORM'] == 'darwin':
	config["linkflags"]["bin"] += ["-pagezero_size", "10000", "-image_base", "100000000"]
env.Append(LIBS = ['dl', 'libluajit-5.1.a'])
#objs = env.Object(Glob("*.cpp"))
lobjs = env.SharedObject(Glob("*.cpp"))

config["name"]["bin"] = "yue"
libs = ["yue.lua", "exlib/debugger/debugger.lua", "exlib/serpent/src/serpent.lua"]
Return("lobjs", "libs")
