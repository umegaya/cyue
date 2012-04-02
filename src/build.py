import os
import sys
from pprint import pprint

#-------------------------------------------------------------------
# configuration
#-------------------------------------------------------------------
modules = {
	"dbm":		"tc",
	"ll":		"lua",
	"serializer":	"mpak",
	"uuid":		"mac"
}

paths = Split("""
	.
	fiber
	handler
	net
	util
""")

cflags = {
	"debug":	["-g", "-Wall", "-D_DEBUG"],
	"release":	["-O4"]
}

linkflags = {
	"debug":	["-g"],
	"release":	["-O4"]
}

filenames = {
	"impl_h":			'impl.h',
}


#-------------------------------------------------------------------
# TODO: apply command line option to initial env
#-------------------------------------------------------------------
build = ARGUMENTS.get('build', 'debug')
config = {
	"name":{
		"bin":"",
		"libs":[]
	},
	"linkflags":{
		"bin":[],
		"lib":[]
	}
}
env = Environment(CXX="gcc", CCFLAGS=cflags[build], LIBS=["stdc++"])
if env['PLATFORM'] == 'darwin':
	env.Append(FRAMEWORKS=("CoreFoundation", "IOKit"))
Export("env", "build", "config")
objs = []
lobjs = []


#-------------------------------------------------------------------
# apply 'modules' configuration to paths, env and impl.h
#-------------------------------------------------------------------
f = open(filenames["impl_h"], 'w')
cppaths = []
for path in paths:
	cppaths += ['#' + path]
for name in modules:
	cppaths += ['#' + name, ('#' + name + "/" + modules[name])]
	env.Append(CCFLAGS=("-D_" + name.upper() + "=" + modules[name]))
	f.write("#if defined(__%s_H__)\n" % (name.upper()))
	f.write("#include \"%s/%s/%s.h\"\n" % (name, modules[name], modules[name]))
	f.write("#endif\n")

f.close()
env.Append(CPPPATH = cppaths)

for name in modules:
	# run each module project specific SConscript
	r = SConscript(name + "/" + modules[name] + "/build.py")
	if r: 
		objs += r[0]
		lobjs += r[1]


#-------------------------------------------------------------------
# autoconf
#-------------------------------------------------------------------
conf = Configure(env)
if conf.CheckCHeader("sys/timerfd.h"):
	conf.env.Append(CCFLAGS="-D__ENABLE_TIMER_FD__")
if conf.CheckCHeader("sys/sendfile.h"):
	conf.env.Append(CCFLAGS="-D__ENABLE_SENDFILE__")
if conf.CheckCHeader("sys/epoll.h"):
	conf.env.Append(CCFLAGS="-D__ENABLE_EPOLL__")
elif conf.CheckCHeader("sys/event.h"):
	conf.env.Append(CCFLAGS="-D__ENABLE_KQUEUE__")
else:
	print "no suitable poller method"
	Exit(1)

if sys.maxsize <= 2**32:
	conf.env.Append(CCFLAGS="-march=i686")
env = conf.Finish()


#-------------------------------------------------------------------
# yue & libyue dependencies and build
#-------------------------------------------------------------------
for path in paths:
	objs += env.Object(Glob(path + "/*.cpp"))
	lobjs += env.SharedObject(Glob(path + "/*.cpp"))

bin_name = config["name"]["bin"]
lib_names = config["name"]["libs"]
#print env.Dump()
env.SharedLibrary(bin_name, lobjs, LINKFLAGS=(linkflags[build] + config["linkflags"]["lib"]))
env.Append(LINKFLAGS=(linkflags[build] + config["linkflags"]["bin"]))
env.Program(bin_name, objs)

Return("env", "bin_name", "lib_names")

