# toolchain
CC="gcc"
AR="ar"
# output dirs
OBJ=".obj"
DEP=".dep"
	
module BuildHelper
	# utilities
	module Util
		# trace
		def trace(s) p s end
		module_function :trace

		# checking attribute
		def srcdir?(path) 
			# do not start '.' and actually exist
			(not path =~ /.*?\.[^\/]*?/) and (not FileTest.file?(path))
		end
		module_function :srcdir?

		# enumerate/operate filenames
		def srclist(path, ext = nil)
			return FileList[path + "/*\." + (ext ? ext : "cpp")]
		end
		def change_ext(path, ext) 
			idx = path.rindex(".")
			return idx ? (path[0..idx] + ext) : path 
		end
		module_function :srclist, :change_ext

		# invoke command
		def sh_with_output(cmd)
			out = nil
			IO.popen(cmd) do |p|
				out = p.read
			end
			out
		end
		def cpu_type
			sh_with_output("uname -m")
		end
		def is_32bit_cpu
			cpu_type !~ /x86_64/
		end
		module_function :sh_with_output, :cpu_type, :is_32bit_cpu
	end	
	
	
	
	module MakeDepend
		# make dependencies
		def build_filerule(s, optexp)
			opath = OBJ + "/" + s.pathmap("%X") + ".o"
			dpath = DEP + "/" + s.pathmap("%X") + ".d"
			sh "mkdir -p #{opath.pathmap("%d") + "/"}"
			sh "mkdir -p #{dpath.pathmap("%d") + "/"}"
			a = make_dep_dependency(s, dpath)
			file dpath => a do |t|
				cmd = "#{CC} #{eval(optexp)} #{t.prerequisites[0]} -MM > #{t.name}.org"
				begin
					sh cmd do |ok,res|
						if ok then 
							make_obj_dependency(opath, t.name, optexp) 
						else
							Util.trace "'#{cmd}' fails rc=" + res.exitstatus.to_s
							File.unlink(t.name + ".org") 
						end
					end
				rescue
				end
			end
			import dpath
		end
		def make_obj_dependency(opath, dpath, optexp)
			File.open(dpath, "w") do |f|
				[opath, Util.change_ext(opath, "lo")].each do |path|
					inf = File.new(dpath + ".org")
					ln = 0
					inf.each do |line|
						if ln == 0 then
							f.write("file '#{path}' => ")
							tmp = line.split(':')[1].split(' ')
							f.write("['#{tmp.shift}'")
							line = tmp.join(' ')
						end
						line.split(' ').each do |l|
							next if l == '\\'
							f.write(",'" + l + "'")
						end
						ln += 1
					end
					if path.match(/\.lo$/) then
						optexp = optexp + "+ \" -fPIC\""
					end
					f.write("] do |t|\n")
					f.write("sh \"\#{CC} \#{#{optexp}} \#{t.prerequisites[0]} -c -o \#{t.name}\"\n")
					f.write("end\n")
				end
			end
		end
		def make_dep_dependency(src, dpath)
			r = Array.new
			r.push(src)
			begin 
				f = File.open(dpath + ".org", "r")
			rescue
				Util.trace "fail to open : " + dpath + ".org"
				return r
			end
			f.each do |line|
				a = line.split(' ')
				a[1..a.size].each do |fn|
					next if fn == '\\'
					begin
						File.open(fn, "r") do |tmp|
							r.push(fn) if fn != src
						end
					rescue
						Util.trace "files are missing: re-build depfile:" + fn
						begin
							File.unlink(dpath)
						rescue
						end
						return [src]
					end
				end
			end
			Util.trace r
			return r
		end
		module_function :build_filerule, :make_obj_dependency, :make_dep_dependency
	end



	# represent one rake-able dirs. 
	# scanning source files and make dependencies
	class BuildableDirectory
		include MakeDepend
		include Util
		def initialize(targets, rakefile_path, scanner)
			@scanner = scanner
			@targets = targets
			idx = rakefile_path.rindex("/")
			@path = idx ? rakefile_path[0 .. idx] : "."
			@targets.incs.push(@path)
			#trace "DirScanner: path = " + @path
			@srcs = srclist(@path)
			if idx then
				name = @path.gsub(/\//, '_')
				@tasks = namespace name.to_sym do
					load rakefile_path
				end
			end
		end
		attr :path
		def exec_task(tname)
			if @tasks and t = @tasks[tname.to_sym] then
				t.execute({:buildtarget => @targets, 
					:builddir => self,
					:scanner => @scanner})
			end
		end
		def build_rule
			@srcs.each do |s| 
				build_filerule(s)
			end
			@srcs.ext("o").each do |o|
				@targets.binobjs.push(OBJ + "/" + o)
			end
			@srcs.ext("lo").each do |lo|
				@targets.libobjs.push(OBJ + "/" + lo)
			end	
			exec_task(:build_rule)
		end
	end
	
	
	
	# stores information for build
	class BuildTargets
		def initialize(cfg)
			@cfg = cfg 
			@incs = Array.new
			@binobjs = Array.new
			@libobjs = Array.new
			@exlibs = Array.new
		end
		def dump
			p "binobjs=> #{binobjs.join(' ')}"
			p "libobjs=> #{libobjs.join(' ')}"
			p "exlibs=> #{exlibs.join(' ')}"
			p "incs=> #{incs.join(' ')}"
		end
		attr :cfg
		attr :incs
		attr :exlibs
		attr :binobjs
		attr :libobjs
	end
	
	
	
	# base scanner
	class Scanner
		def initialize(build_cfg, lib_cfg, module_cfg) 
			@targets = BuildTargets.new(build_cfg)
			@main = nil
			@modules = Hash.new
			@libs = Hash.new
			@MODULES = module_cfg
			@LIBDIRS = lib_cfg
		end
		attr :modules
		attr :main
		attr :libs
		attr :targets
		def execute
			# find buildable directory and add as component
			FileList["**/rakefile"].each do |f|
				next if add_as_main(f)
				next if add_as_module(f)
				next if add_as_lib(f)
			end
			each do |ds|
				ds.exec_task(:prebuild)
			end
		end
		def new_buildable_directory(targets, path)
			BuildableDirectory.new(targets, path, self)
		end
		# find directory /module_category/module_name and add it as module
		def add_as_module(path)
			a = path.split('/');
			if a.length > 2 and @MODULES[a[0]] == a[1] then
				@targets.incs.push(a[0])
				@modules[a[0]] = new_buildable_directory(@targets, path)
			else
				nil
			end
		end
		# top directory regard as main
		def add_as_main(path)
			if path == "rakefile" then
				@main = new_buildable_directory(@targets, path)
			else
				nil
			end
		end
		# others which specified LIBDIRS regards as library 
		def add_as_lib(path)
			@LIBDIRS.each do |name|
				if name == path[0..name.length - 1] then
					@libs[name] = new_buildable_directory(@targets, path)
					return @libs[name]
				end 
			end
			nil
		end
		def each(&block)
			block.call(@main)
			@modules.each do |name, ds|
				block.call(ds)
			end
			@libs.each do |name, ds|
				block.call(ds)
			end
		end
	end
end
