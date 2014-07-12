/*
** File Name:	read_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/03/2014
** Contact:	_@adityaramesh.com
**
** This is a simple benchmark that compares various methods for sequentially
** reading a file.
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
read_mmap_nocache(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	disable_cache(fd);

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_SHARED, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
read_mmap_rdahead(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	enable_rdahead(fd);

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_SHARED, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
read_mmap_rdadvise(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	enable_rdadvise(fd, fs);

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_SHARED, fd, 0);
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

	std::printf("%s, %s, %s, %s\n", "File Size", "Method", "Mean (ms)", "Stddev (ms)");
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
	test_read(std::bind(read_mmap_plain, path), "mmap_plain", count, fs);
	test_read(std::bind(read_mmap_nocache, path), "mmap_nocache", count, fs);
	test_read(std::bind(read_mmap_rdahead, path), "mmap_rdahead", count, fs);
	test_read(std::bind(read_mmap_rdadvise, path), "mmap_rdadvise", count, fs);
}
