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
	"impl_hpp":			'impl.hpp',
}
rootdir = (Dir('#').abspath + "/")


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
env = Environment(CXX="gcc", CCFLAGS=cflags[build], LIBS=["stdc++","pthread"])
if env['PLATFORM'] == 'darwin':
	env.Append(FRAMEWORKS=("CoreFoundation", "IOKit"))
else:
	env.Append(LIBS="rt")
	config["linkflags"]["lib"] += ["-Wl,-Bsymbolic"]
	#config["linkflags"]["lib"] += ["-Wl,-t"]
Export("env", "build", "config", "linkflags")
objs = []
lobjs = []


#-------------------------------------------------------------------
# apply 'modules' configuration to paths, env and impl.h
#-------------------------------------------------------------------
f = open(rootdir + filenames["impl_h"], 'w')
fpp = open(rootdir + filenames["impl_hpp"], 'w')
cppaths = []
for path in paths:
	cppaths += ['#' + path]
for name in modules:
	cppaths += ['#' + name, ('#' + name + "/" + modules[name])]
	env.Append(CCFLAGS=("-D_" + name.upper() + "=" + modules[name]))
	f.write("#if defined(__%s_H__)\n" % (name.upper()))
	f.write("#include \"%s/%s/%s.h\"\n" % (name, modules[name], modules[name]))
	f.write("#endif\n")
	hpp_path = "%s/%s/%s.hpp" % (name, modules[name], modules[name])
	if os.path.isfile(rootdir + hpp_path):
		fpp.write("#include \"%s\"\n" % (hpp_path))

f.close()
fpp.close()
env.Append(CPPPATH = cppaths)

for name in modules:
	# run each module project specific SConscript
	r = SConscript(name + "/" + modules[name] + "/build.py")
	if r:
		if type(r) is tuple: 
			lobjs += r[0]
			if len(r) > 1 and r[1]:
				for file in r[1]:
					config["name"]["libs"] += [{
						"path" : (name + "/" + modules[name]), 
						"file" : file
					}]
		else:
			lobjs += r

#-------------------------------------------------------------------
# autoconf
#-------------------------------------------------------------------
conf = Configure(env)
if conf.CheckCHeader("sys/timerfd.h", "<>"):
	conf.env.Append(CCFLAGS="-D__ENABLE_TIMER_FD__")
if conf.CheckCHeader("sys/sendfile.h", "<>"):
	conf.env.Append(CCFLAGS="-D__ENABLE_SENDFILE__")
if conf.CheckCHeader("sys/epoll.h", "<>"):
	conf.env.Append(CCFLAGS="-D__ENABLE_EPOLL__")
if conf.CheckCHeader("sys/inotify.h", "<>"):
	conf.env.Append(CCFLAGS="-D__ENABLE_INOTIFY__")
elif conf.CheckCHeader("sys/event.h", "<>"):
	conf.env.Append(CCFLAGS="-D__ENABLE_KQUEUE__")
else:
	print "no suitable poller method"
	Exit(1)
	
endian = sys.byteorder
if endian == "little":
	conf.env.Append(CCFLAGS="-D__NBR_BYTE_ORDER__=__NBR_LITTLE_ENDIAN__")
elif endian == "big":
	conf.env.Append(CCFLAGS="-D__NBR_BYTE_ORDER__=__NBR_BIG_ENDIAN__")
else:
	print "not supported endian type"
	Exit(1)

if sys.maxsize <= 2**32:
	conf.env.Append(CCFLAGS="-march=i686")
env = conf.Finish()


#-------------------------------------------------------------------
# yue & libyue dependencies and build
#-------------------------------------------------------------------
for path in paths:
	lobjs = (env.SharedObject(Glob(path + "/*.cpp")) + lobjs)

bin_name = config["name"]["bin"]
lib_names = config["name"]["libs"]
#print env.Dump()
env.SharedLibrary(bin_name, lobjs, LINKFLAGS=(linkflags[build] + config["linkflags"]["lib"]))
env.Append(LINKFLAGS=(linkflags[build] + config["linkflags"]["bin"]))
# unit test, main server will be build in sconstruct

Return("env", "bin_name", "lib_names")

