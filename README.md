<!--
  ** File Name:	README.md
  ** Author:	Aditya Ramesh
  ** Date:	06/04/2014
  ** Contact:	_@adityaramesh.com
-->

# Introduction

This repository contains a comprehensive set of IO benchmarks for Linux and OS
X. These benchmarks are intended to determine the fastest ways to perform the
following IO operations on each target platform:

  - Sequentially reading a file.
  - Sequentially overwriting a preallocated file.
  - Replacing the contents of an existing file with those of another file.

# Prerequisites

- Linux (version 2.6.33 or newer) or OS X.
- A C++11-conformant compiler that accepts flags in GCC's format (e.g. `g++` or
`clang++`).
- Boost.
- Ruby.
- Rake.

# Compilation

Before compiling the benchmarks, you will need to set two environment variables.

  - Set `CXX` to your desired C++11-conformant compiler.
  - Set `BOOST_INCLUDE_PATH` to the path containing the Boost header files.

Afterwards, you can compile the benchmarks by running `rake`.

# Usage

The `tools` directory contains a set of scripts that you will need to run the
benchmarks. In order to run these scripts, you will need to type `chmod +x
tools/*`.

  - The `tools/make_data.rb` script uses `dd` to create a set of files in the
  `data` directory. These files are used to perform the benchmarks.
  - The `tools/test_read.sh` and `tools/test_write.sh` scripts perform the
  reading and writing benchmarks, respectively.
  - I did not create a script to run the copy benchmark. Based on existing
  results, it is clear that the fastest way to copy a file on OS X is
  `copy_mmap`, and `splice_preallocate_fadvise` or
  `sendfile_preallocate_fadvise` on Linux.

In both `test_read.sh` and `test_write.sh`, you will see the following lines:

	#sizes=(8 16 24 32 40 48 56 64 80 96 112 128 160 192 224 256 320 384 448 512 640 768 896 1024)
	sizes=(8 16 32 64 80 96 112 256) #512 1024)

These lines declare the sizes of the files (in megabytes) that are used for the
benchmarks. By default, only a small set of files ranging in size from 8 MB to
256 MB are used. The first line in the pair refers to the full set of files
produced by `tools/make_data.rb`. Even on a machine with a fast PCIe SSD, the
read benchmark did not finish overnight. So only uncomment this line if you know
that you will be able to leave the benchmark running for a long time (half a day
to several days, depending on the speed of your hard drive).

The results of the benchmarks are saved in the `results` directory. This
directory already contains results generated from a couple of systems.

# References

- The manual pages for [OS X][darwin_man] and [Linux][linux_man].
- A very useful [benchmark][write_patterns] on write patterns.
- The [blog post][plenz_blog_post] by the same author.
- A Mozilla [blog post][moz_blog_post] about `F_PREALLOCATE` on OS X.

[darwin_man]:
https://developer.apple.com/library/mac/documentation/Darwin/Reference/Manpages/
"Mac OS X Manual Pages"

[linux_man]:
http://linux.die.net/man/
"Linux Manual Pages"

[write_patterns]:
https://github.com/Feh/write-patterns
"Write Patterns"

[plenz_blog_post]:
http://blog.plenz.com/2014-04/so-you-want-to-write-to-a-file-real-fast.html
"Write Patterns Blog Post"

[moz_blog_post]:
https://blog.mozilla.org/tglek/2010/09/09/help-wanted-does-fcntlf_preallocate-work-as-advertised-on-osx/
"F_PREALLOCATE Blog Post"
