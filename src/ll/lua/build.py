Import('env')
Import('build')
Import('name_config')

env.Append(LIBS = ['dl', 'luajit-5.1'])
objs = env.Object(Glob("*.cpp")),
lobjs = env.SharedObject(Glob("*.cpp"))
name_config["bin_name"] = "yue"
name_config["lib_names"] += ["yue.lua"]

Return("objs", "lobjs")

