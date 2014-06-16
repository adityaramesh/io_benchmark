/*
** File Name:	read_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/03/2014
** Contact:	_@adityaramesh.com
**
** This is a simple benchmark that compares various methods to sequentially read
** a file.
**
** - Scheme for OS X:
**   - Synchronous:
**     - Under 256 MB: read_rdadvise 56 KB
**     - 256 MB and above: read_rdahead 4096 KB
**   - Asynchronous:
**     - Under 128 MB: read_rdadvise 1024 KB
**     - 128 MB and above: read_rdahead 4096 KB
**
** - Best results for OS X:
**   - 8 MB:
**     - read_rdadvise 32 KB -- 256 KB
**   - 16 MB:
**     - read_rdadvise 24 KB -- 56 KB
**   - 24 MB:
**     - read_rdadvise 64 KB, 8 KB, 16 KB, 1024 KB
**   - 32 MB:
**     - read_async_rdadvise 256 KB
**     - read_rdadvise 4 KB, 1024 KB
**   - 40 MB:
**     - read_rdadvise 48 KB, 24 KB, 1024 KB, 4 KB, 8 KB, 64 KB
**   - 48 MB:
**     - read_async_rdadvise 1024 KB
**     - read_rdadvise 1024 KB, 24 KB, 64 KB, 16 KB, 56 KB
**   - 56 MB:
**     - read_async_rdadvise 32 KB
**     - read_rdadvise 24 KB, 16384 KB, 256 KB
**   - 64 MB:
**     - read_async_rdadvise 1024 KB, 
**     - read_rdadvise 1024 KB, 256 KB, 8 KB, 48 KB
**   - 80 MB:
**     - read_async_rdadvise 4096 KB, 1024 KB, 8 KB, 256 KB
**     - read_rdadvise 8 KB, 24 KB, 32 KB
**   - 96 MB:
**     - read_async_rdadvise 4096 KB, 256 KB, 1024 KB, 64 KB
**     - read_rdadvise 16384 KB, 1024 KB, 16 KB, 48 KB, 4 KB, 32 KB, 56 KB, 64 KB
**   - 112 MB:
**     - read_async_rdadvise 40 KB, 256 KB, 4096 KB, nocache 4096 KB
**     - read_rdadvise 64 KB, 4096 KB
**   - 128 MB:
**     - read_async_rdahead 16384 KB, 4096 KB
**     - read_rdadvise 56 KB, 48 KB, 16 KB, 8 KB
**   - 160 MB:
**     - read_async_rdahead 4096 KB, 16384 KB, rdadvise 40 KB, 4096 KB, nocache 16384 KB
**     - read_rdadvise 56 KB, 4096 KB
**   - 192 MB:
**     - read_async_rdadvise 4096 KB, 16384 KB
**     - read_rdadvise 8 KB, 1024 KB
**   - 224 MB:
**     - read_async_rdadvise 16384 KB, rdahead 4096 KB
**     - read_rdadvise 4096 KB
**   - 256 MB:
**     - read_async_rdahead 16384 KB, rdahead 4096 KB
**     - read_rdahead 4096 KB
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>
#include <ccbase/format.hpp>

#include <read_common.hpp>
#include <test.hpp>

static auto
read_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = allocate_aligned(4096, buf_size);
	disable_cache(fd);

	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_rdahead(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdahead(fd);

	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_rdadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdadvise(fd, fs);

	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_aio_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	disable_cache(fd);

	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_aio_rdahead(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdahead(fd);

	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_aio_rdadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdadvise(fd, fs);

	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	disable_cache(fd);

	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_rdahead(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdahead(fd);

	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_rdadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	enable_rdadvise(fd, fs);

	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
mmap_rdahead(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	enable_rdahead(fd);

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
mmap_rdadvise(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	enable_rdadvise(fd, fs);

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		cc::errln("Error: too few arguments.");
		return EXIT_FAILURE;
	}
	else if (argc > 2) {
		cc::errln("Error: too many arguments.");
		return EXIT_FAILURE;
	}

	auto path = argv[1];
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	safe_close(fd).get();

	auto count = check(path);
	auto sizes = {4, 8, 12, 16, 24, 32, 40, 48, 56, 64, 256, 1024, 4096, 16384, 65536, 262144};
	purge_cache().get();

	std::printf("%s, %s, %s\n", "Method", "Mean (ms)", "Stddev (ms)");
	std::fflush(stdout);
	test_read_range(read_plain, path, "read_plain", sizes, fs, count);
	test_read_range(read_nocache, path, "read_nocache", sizes, fs, count);
	test_read_range(read_rdahead, path, "read_rdahead", sizes, fs, count);
	test_read_range(read_rdadvise, path, "read_rdadvise", sizes, fs, count);
	//test_read_range(read_aio_nocache, path, "read_aio_nocache", sizes, fs, count);
	//test_read_range(read_aio_rdahead, path, "read_aio_rdahead", sizes, fs, count);
	//test_read_range(read_aio_rdadvise, path, "read_aio_rdadvise", sizes, fs, count);
	test_read_range(read_async_nocache, path, "read_async_nocache", sizes, fs, count);
	test_read_range(read_async_rdahead, path, "read_async_rdahead", sizes, fs, count);
	test_read_range(read_async_rdadvise, path, "read_async_rdadvise", sizes, fs, count);
	//test_read(std::bind(mmap_plain, path), "mmap_plain", count);
	//test_read(std::bind(mmap_rdahead, path), "mmap_rdahead", count);
	//test_read(std::bind(mmap_rdadvise, path), "mmap_rdadvise", count);
}
