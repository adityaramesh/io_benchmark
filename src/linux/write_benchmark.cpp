/*
** File Name:	write_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/16/2014
** Contact:	_@adityaramesh.com
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <ccbase/format.hpp>

#include <write_common.hpp>
#include <test.hpp>

static void
write_direct(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT).get();
	auto buf = allocate_aligned(4096, buf_size);
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_preallocate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size);
	preallocate(fd, count);
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_preallocate_truncate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size);
	preallocate(fd, count);
	truncate(fd, count);
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_async_direct(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	async_write_loop(fd, buf1.get(), buf2.get(), buf_size, count);
	::close(fd);
}

static void
write_async_preallocate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	preallocate(fd, count);
	async_write_loop(fd, buf1.get(), buf2.get(), buf_size, count);
	::close(fd);
}

static void
write_async_preallocate_truncate(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf1 = allocate_aligned(4096, buf_size);
	auto buf2 = allocate_aligned(4096, buf_size);
	preallocate(fd, count);
	truncate(fd, count);
	async_write_loop(fd, buf1.get(), buf2.get(), buf_size, count);
	::close(fd);
}

static void
write_mmap(const char* path, size_t count)
{
	auto fd = safe_open(path, O_RDWR | O_CREAT | O_TRUNC).get();
	// TODO does O_DIRECT help?
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
	test_write_range(std::bind(write_plain, _1, _2, count), path, "write_plain", sizes, count);
	test_write_range(std::bind(write_direct, _1, _2, count), path, "write_direct", sizes, count);
	test_write_range(std::bind(write_preallocate, _1, _2, count), path, "write_preallocate", sizes, count);
	test_write_range(std::bind(write_preallocate_truncate, _1, _2, count), path, "write_preallocate_truncate", sizes, count);
	test_write_range(std::bind(write_async_plain, _1, _2, count), path, "write_async_plain", sizes, count);
	test_write_range(std::bind(write_async_direct, _1, _2, count), path, "write_async_direct", sizes, count);
	test_write_range(std::bind(write_async_preallocate, _1, _2, count), path, "write_async_preallocate", sizes, count);
	test_write_range(std::bind(write_async_preallocate_truncate, _1, _2, count), path, "write_async_preallocate_truncate", sizes, count);
	test_write(std::bind(write_mmap, path, count), "write_mmap");
}
