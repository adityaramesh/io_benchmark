/*
** File Name:	write_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/04/2014
** Contact:	_@adityaramesh.com
**
** - https://github.com/Feh/write-patterns
** - http://blog.plenz.com/2014-04/so-you-want-to-write-to-a-file-real-fast.html
** - https://blog.mozilla.org/tglek/2010/09/09/help-wanted-does-fcntlf_preallocate-work-as-advertised-on-osx/
**
** - Scheme for OS X:
**   - Synchronous:
**     - 4096 KB
**   - Asynchronous:
**     - Below 56 MB:
**       - 256 KB
**     - 56 MB and above:
**       - 1024 KB
**
** - Best results for OS X:
**   - 8 MB:
**     - 4096 KB
**     - 256 KB
**   - 16 MB:
**     - 4096 KB
**     - 256 KB
**   - 24 MB:
**     - 4096 KB
**     - 256 KB
**   - 32 MB:
**     - 16384 KB
**     - 256 KB
**   - 64 MB:
**     - 4096 KB
**     - 256 KB, 1024 KB
**   - 48 MB:
**     - 16384 KB
**     - 256 KB, 1024 KB
**   - 56 MB:
**     - 4096 KB
**     - 1024 KB
**   - 64 MB:
**     - 16384 KB
**     - 256 KB, 1024 KB
**   - 80 MB:
**     - 16384 KB
**     - 1024 KB
**   - 96 MB:
**     - 907 KB
**     - 4096 KB, 1024 KB
**   - 112 MB:
**     - 4096 KB
**     - 1024 KB
**   - 128 MB:
**     - 4096 KB, 16384 KB
**     - 1024 KB, 4096 KB
*/

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <random>
#include <ratio>
#include <ccbase/format.hpp>

#include <write_common.hpp>
#include <test.hpp>

static void
write_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size).get();
	disable_cache(fd);

	write_loop(fd, buf, buf_size, count);
	::close(fd);
	std::free(buf);
}

static void
write_preallocate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	preallocate(fd, count);
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_preallocate_truncate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	preallocate(fd, count);
	truncate(fd, count);
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_preallocate_truncate_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size).get();
	disable_cache(fd);
	preallocate(fd, count);
	truncate(fd, count);

	write_loop(fd, buf, buf_size, count);
	::close(fd);
	std::free(buf);
}

static void
async_write_preallocate_truncate_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf1 = allocate_aligned(4096, buf_size).get();
	auto buf2 = allocate_aligned(4096, buf_size).get();
	disable_cache(fd);
	preallocate(fd, count);
	truncate(fd, count);

	async_write_loop(fd, buf1, buf2, buf_size, count);
	::close(fd);
	std::free(buf1);
	std::free(buf2);
}

static void
write_mmap(const char* path, size_t count)
{
	auto fd = safe_open(path, O_RDWR | O_CREAT | O_TRUNC).get();
	disable_cache(fd);
	preallocate(fd, count);
	truncate(fd, count);

	auto p = (uint8_t*)::mmap(nullptr, count, PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (p == (void*)-1) { throw current_system_error(); }
	fill_buffer(p, count);
	::close(fd);
}

int main(int argc, char** argv)
{
	using namespace std::placeholders;

	if (argc < 2) {
		cc::errln("Error: too few arguments.");
		return EXIT_FAILURE;
	}
	else if (argc > 2) {
		cc::errln("Error: too many arguments.");
		return EXIT_FAILURE;
	}

	auto count = std::atoi(argv[1]);
	if (count <= 0) {
		cc::errln("Error: count must be positive.");
		return EXIT_FAILURE;
	}

	auto path = "data/test.bin";
	auto kb = 1024;
	auto sizes = {4, 8, 12, 16, 24, 32, 40, 48, 56, 64, 256, 1024, 4096, 16384, 65536, 262144};

	// Dummy write to create file.
	write_plain(path, 4 * kb, count);

	std::printf("%s, %s, %s\n", "Method", "Mean (ms)", "Stddev (ms)");
	std::fflush(stdout);
	//test_write_range(std::bind(write_plain, _1, _2, count), path, "write_plain", sizes, count);
	//test_write_range(std::bind(write_nocache, _1, _2, count), path, "write_nocache", sizes, count);
	//test_write_range(std::bind(write_preallocate, _1, _2, count), path, "write_preallocate", sizes, count);
	//test_write_range(std::bind(write_preallocate_truncate, _1, _2, count), path, "write_preallocate_truncate", sizes, count);
	test_write_range(std::bind(write_preallocate_truncate_nocache, _1, _2, count), path, "write_preallocate_truncate_nocache", sizes, count);
	test_write_range(std::bind(async_write_preallocate_truncate_nocache, _1, _2, count), path, "async_write_preallocate_truncate_nocache", sizes, count);
	//test_write(std::bind(write_mmap, path, count), path);
}
