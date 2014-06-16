require 'rake/clean'

cxx       = ENV['CXX']
boost     = ENV['BOOST_INCLUDE_PATH']
ccbase    = "include/ccbase"
langflags = "-std=c++1y"
wflags    = "-Wall -Wextra -pedantic -Wno-unused-function -Wno-return-type-c-linkage"
archflags = "-march=native"
incflags  = "-I include -isystem #{boost} -isystem #{ccbase}"
ldflags   = ""

if cxx.include? "clang"
	optflags = "-Ofast -fno-fast-math -flto -DNDEBUG -DNEO_NO_DEBUG"
elsif cxx.include? "g++"
	optflags = "-Ofast -fno-fast-math -flto -fwhole-program"
end

source_dir = ""
sources = ""

if RUBY_PLATFORM.include? "darwin"
	source_dir = "src/os_x"
	sources = FileList["src/os_x/*"]
elsif RUBY_PLATFORM.include? "linux"
	source_dir = "src/linux"
	sources = FileList["src/linux/*"]
end

cxxflags = "#{langflags} #{wflags} #{archflags} #{incflags} #{optflags}"
dirs = ["data", "out"]
tests = sources.map{|f| f.sub(source_dir, "out").ext("run")}

task :default => dirs + tests

dirs.each do |d|
	directory d
end

tests.each do |f|
	src = f.sub("out", source_dir).ext("cpp")
	file f => [src] + dirs do
		sh "#{cxx} #{cxxflags} -o #{f} #{src} #{ldflags}"
	end
end

task :clobber do
	FileList["out/*.run"].each{|f| File.delete(f)}
end
