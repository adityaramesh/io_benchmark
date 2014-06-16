<!--
  ** File Name:	README.md
  ** Author:	Aditya Ramesh
  ** Date:	06/04/2014
  ** Contact:	_@adityaramesh.com
-->

# Introduction

This repository contains a set of IO benchmarks for Linux and OS X. These
benchmarks are intended to determine the fastest ways to perform each the
following IO operations on the target platform:

  - Sequentially reading a file.
  - Sequentially overwriting a preallocated file.
  - Replacing the contents of an existing file with those of another file.

Currently, these benchmarks only target OS X and Linux.

# Prerequisites

- A C++11-conformant compiler that accepts flags in GCC's format.
- Ruby, version 2.0 or newer.
- Rake.

# Instructions

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
