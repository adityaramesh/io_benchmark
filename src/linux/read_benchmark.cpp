/*
** File Name:	read_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/04/2014
** Contact:	_@adityaramesh.com
**
** This is a simple benchmark that compares various methods for sequentially
** reading a file.
**
** - Scheme for Linux:
**   - read_plain 32 KB
**   - read_async_plain 32 KB
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
read_direct(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_DIRECT | O_NOATIME).get();
	auto buf = allocate_aligned(4096, buf_size);
	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_fadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_NOATIME).get();
	auto fs = file_size(fd).get();
	auto buf = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	fadvise_sequential_read(fd, fs);

	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
aio_read_direct(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_DIRECT | O_NOATIME).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
aio_read_fadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_NOATIME).get();
	auto fs = file_size(fd).get();
	auto buf1 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	auto buf2 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	fadvise_sequential_read(fd, fs);

	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_plain(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_NOATIME).get();
	auto buf1 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	auto buf2 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_direct(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_DIRECT | O_NOATIME).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_fadvise(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY | O_NOATIME).get();
	auto fs = file_size(fd).get();
	auto buf1 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	auto buf2 = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	fadvise_sequential_read(fd, fs);

	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_mmap_fadvise(const char* path)
{
	auto fd = safe_open(path, O_RDONLY | O_NOATIME).get();
	auto fs = file_size(fd).get();
	fadvise_sequential_read(fd, fs);

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

	print_header();
	test_read_range(read_plain, path, "read_plain", sizes, fs, count);
	test_read_range(read_direct, path, "read_direct", sizes, fs, count);
	test_read_range(read_fadvise, path, "read_fadvise", sizes, fs, count);
	test_read_range(aio_read_direct, path, "aio_read_direct", sizes, fs, count);
	test_read_range(aio_read_fadvise, path, "aio_read_fadvise", sizes, fs, count);
	test_read_range(read_async_plain, path, "read_async_plain", sizes, fs, count);
	test_read_range(read_async_direct, path, "read_async_direct", sizes, fs, count);
	test_read_range(read_async_fadvise, path, "read_async_fadvise", sizes, fs, count);
	test_read(std::bind(read_mmap_plain, path), "mmap_plain", count, fs);
	test_read(std::bind(read_mmap_fadvise, path), "mmap_fadvise", count, fs);
}
