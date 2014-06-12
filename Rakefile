require 'rake/clean'

cxx       = ENV['CXX']
boost     = ENV['BOOST_INCLUDE_PATH']
ccbase    = ENV['CCBASE_INCLUDE_PATH']
langflags = "-std=c++1y"
wflags    = "-Wall -Wextra -pedantic -Wno-return-type-c-linkage"
archflags = "-march=native"
incflags  = "-I include -isystem #{boost} -isystem #{ccbase}"

if cxx.include? "clang"
	optflags = "-Ofast -fno-fast-math -flto -DNDEBUG -DNEO_NO_DEBUG -ggdb"
elsif cxx.include? "g++"
	optflags = "-Ofast -fno-fast-math -flto -fwhole-program"
end

cxxflags = "#{langflags} #{wflags} #{archflags} #{incflags} #{optflags}"
tests = ["out/read_benchmark.run"]

task :default => ["out"] + tests

directory "out"

file "out/read_benchmark.run" => ["out"] do
	sh "#{cxx} #{cxxflags} -o out/read_benchmark.run src/os_x/read_benchmark.cpp"
end

task :clobber do
	FileList["out/*.run"].each{|f| File.delete(f)}
end
