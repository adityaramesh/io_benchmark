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
** - Best results: preallocate + truncate + 256 Kb.
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <random>
#include <ratio>
#include <ccbase/format.hpp>
#include <boost/range/numeric.hpp>

#include <configuration.hpp>
#include <io_common.hpp>
#include <test.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <sys/mman.h>
#else
	#error "Unsupported kernel."
#endif

static void
fill_buffer(uint8_t* p, size_t count)
{
	auto gen = std::mt19937{std::random_device{}()};
	auto dist = std::uniform_int_distribution<uint8_t>(0, 255);
	std::generate(p, p + count, [&]() { return dist(gen); });
}

static void
preallocate(int fd, size_t count)
{
	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = count;
	if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
			throw std::runtime_error{"Warning: failed to preallocate space."};
		}
	}
}

static auto
write_loop(int fd, uint8_t* buf, size_t buf_size, size_t count)
{
	for (auto off = off_t{0}; off < off_t(count); off += buf_size) {
		fill_buffer(buf, buf_size);
		auto r = full_write(fd, buf, buf_size, off);
		assert(r == buf_size);
	}
}

static void
write_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	std::atomic<int>& cv1,
	std::atomic<int>& cv2
)
{
	auto r = int{};
	auto s = int{};
	auto off = off_t{0};
	auto buf1_active = false;

	for (;;) {
		if (buf1_active) {
			while (cv2.load(std::memory_order_acquire) == -1) {}
			s = cv2.load(std::memory_order_relaxed);
			if (s == -2) { return; }
			r = full_write(fd, buf2, s, off).get();
			cv2.store(-1, std::memory_order_release);
		}
		else {
			while (cv1.load(std::memory_order_acquire) == -1) {}
			s = cv1.load(std::memory_order_relaxed);
			if (s == -2) { return; }
			r = full_write(fd, buf1, s, off).get();
			cv1.store(-1, std::memory_order_release);
		}
		assert(r == s);
		buf1_active = !buf1_active;
		off += s;
	}
}

static void
async_write_loop(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
	size_t count
)
{
	if (count <= buf_size) {
		fill_buffer(buf1, count);
		auto r = full_write(fd, buf1, count, 0).get();
		assert(r == count);
		return;
	}

	fill_buffer(buf1, buf_size);
	std::atomic<int> cv1(buf_size);
	std::atomic<int> cv2{-1};
	auto rem = count - buf_size;
	auto buf1_active = false;
	auto t = std::thread(write_worker, fd, buf1, buf2,
		std::ref(cv1), std::ref(cv2));

	for (;;) {
		if (buf1_active) {
			while (cv1.load(std::memory_order_acquire) != -1) {}
			if (rem > buf_size) {
				fill_buffer(buf1, buf_size);
				cv1.store(buf_size, std::memory_order_release);
			}
			else {
				fill_buffer(buf1, rem);
				cv1.store(-2, std::memory_order_release);
				goto exit;
			}
		}
		else {
			while (cv2.load(std::memory_order_acquire) != -1) {}
			if (rem > buf_size) {
				fill_buffer(buf2, buf_size);
				cv2.store(buf_size, std::memory_order_release);
			}
			else {
				fill_buffer(buf2, rem);
				cv2.store(-2, std::memory_order_release);
				goto exit;
			}
		}
		buf1_active = !buf1_active;
		rem -= buf_size;
	}
exit:
	t.join();
}

static void
write_plain(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) { throw current_system_error(); }

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw current_system_error();
	}

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
	if (::ftruncate(fd, count) == -1) { throw current_system_error(); }
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

static void
write_preallocate_truncate_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) { throw current_system_error(); }
	if (::fcntl(fd, F_NOCACHE, 1) == -1) { throw current_system_error(); }
	preallocate(fd, count);
	if (::ftruncate(fd, count) == -1) { throw current_system_error(); }

	write_loop(fd, buf, buf_size, count);
	::close(fd);
	std::free(buf);
}

static void
async_write_preallocate_truncate_nocache(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf1 = (uint8_t*)nullptr;
	auto buf2 = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf1, 4096, buf_size);
	if (r != 0) { throw current_system_error(); }
	r = ::posix_memalign((void**)&buf2, 4096, buf_size);
	if (r != 0) { throw current_system_error(); }

	if (::fcntl(fd, F_NOCACHE, 1) == -1) { throw current_system_error(); }
	preallocate(fd, count);
	if (::ftruncate(fd, count) == -1) { throw current_system_error(); }

	async_write_loop(fd, buf1, buf2, buf_size, count);
	::close(fd);
	std::free(buf1);
	std::free(buf2);
}

static void
write_mmap(const char* path, size_t count)
{
	auto fd = safe_open(path, O_RDWR | O_CREAT | O_TRUNC).get();
	if (::fcntl(fd, F_NOCACHE, 1) == -1) { throw current_system_error(); }
	preallocate(fd, count);
	if (::ftruncate(fd, count) == -1) { throw current_system_error(); }

	auto p = (uint8_t*)::mmap(nullptr, count, PROT_READ | PROT_WRITE,
		MAP_PRIVATE, fd, 0);
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

	auto path = "data/test.dat";
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
