Import('env')
Import('build')

#objs = env.Object(Glob("*.cpp"))
lobjs = env.SharedObject(Glob("*.cpp"))
Return("lobjs")
