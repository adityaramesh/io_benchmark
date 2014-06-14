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
** - Best results:
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
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <ratio>
#include <ccbase/format.hpp>
#include <boost/range/numeric.hpp>

#include <configuration.hpp>
#include <io_common.hpp>
#include <test.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <aio.h>
	#include <sys/mman.h>
#else
	#error "Unsupported kernel."
#endif

static auto
read_loop(int fd, uint8_t* buf, size_t buf_size)
{
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto n = (size_t)full_read(fd, buf, buf_size, off).get();
		count += std::count_if(buf, buf + n,
			[](auto x) { return x == needle; });
		if (n < buf_size) { break; }
		off += n;
	}
	return count;
}

static auto
aio_read_loop(int fd, uint8_t* buf1, uint8_t* buf2, size_t buf_size)
{
	using aiocb = struct aiocb;
	auto cb = aiocb{};
	cb.aio_fildes = fd;
	cb.aio_nbytes = buf_size;

	auto l = std::array<aiocb*, 1>{{&cb}};
	auto off = off_t{0};
	auto count = off_t{0};

	auto buf1_active = true;
	auto n = full_read(fd, buf1, buf_size, off).get();
	if (size_t(n) < buf_size) {
		return (off_t)std::count_if(buf1, buf1 + n,
			[](auto x) { return x == needle; });
	}
	off += buf_size;

	for (;;) {
		if (buf1_active) {
			cb.aio_buf = buf2;
			cb.aio_offset = off;
			if (::aio_read(&cb) == -1) { throw current_system_error(); }

			count += std::count_if(buf1, buf1 + buf_size,
				[](auto x) { return x == needle; });
		}
		else {
			cb.aio_buf = buf1;
			cb.aio_offset = off;
			if (::aio_read(&cb) == -1) { throw current_system_error(); }

			count += std::count_if(buf2, buf2 + buf_size,
				[](auto x) { return x == needle; });
		}

		buf1_active = !buf1_active;
		if (::aio_suspend(l.data(), 1, nullptr) == -1) { throw current_system_error(); }
		if (::aio_error(&cb) == -1) { throw current_system_error(); }
		n = ::aio_return(&cb);
		if (n == -1) { throw current_system_error(); }

		if (size_t(n) < buf_size) {
			if (buf1_active) {
				count += std::count_if(buf1, buf1 + n,
					[](auto x) { return x == needle; });
			}
			else {
				count += std::count_if(buf2, buf2 + n,
					[](auto x) { return x == needle; });
			}
			return count;
		}
		off += buf_size;
	}
}

static void
read_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
	std::atomic<int>& cv1,
	std::atomic<int>& cv2
)
{
	auto r = int{};
	auto off = off_t(buf_size);
	auto buf1_active = true;

	for (;;) {
		if (buf1_active) {
			while (cv2.load(std::memory_order_acquire) != -1) {}
			r = full_read(fd, buf2, buf_size, off).get();
			cv2.store(r, std::memory_order_release);
		}
		else {
			while (cv1.load(std::memory_order_acquire) != -1) {}
			r = full_read(fd, buf1, buf_size, off).get();
			cv1.store(r, std::memory_order_release);
		}
		if (size_t(r) < buf_size) { return; }
		buf1_active = !buf1_active;
		off += buf_size;
	}
}

static auto
async_read_loop(int fd, uint8_t* buf1, uint8_t* buf2, size_t buf_size)
{
	auto r = full_read(fd, buf1, buf_size, 0).get();
	if (size_t(r) < buf_size) {
		return (off_t)std::count_if(buf1, buf1 + r,
			[](auto x) { return x == needle; });
	}

	std::atomic<int> cv1(buf_size);
	std::atomic<int> cv2{-1};
	auto count = off_t{0};
	auto buf1_active = true;
	auto t = std::thread(read_worker, fd, buf1, buf2, buf_size,
		std::ref(cv1), std::ref(cv2));

	for (;;) {
		if (buf1_active) {
			while (cv1.load(std::memory_order_acquire) == -1) {}
			r = cv1.load(std::memory_order_relaxed);
			count += std::count_if(buf1, buf1 + r,
				[](auto x) { return x == needle; });
			if (size_t(r) < buf_size) { goto exit; }
			cv1.store(-1, std::memory_order_release);
		}
		else {
			while (cv2.load(std::memory_order_acquire) == -1) {}
			r = cv2.load(std::memory_order_relaxed);
			count += std::count_if(buf2, buf2 + r,
				[](auto x) { return x == needle; });
			if (size_t(r) < buf_size) { goto exit; }
			cv2.store(-1, std::memory_order_release);
		}
		buf1_active = !buf1_active;
	}
exit:
	t.join();
	return count;
}

static auto
check(const char* path)
{
	static constexpr auto buf_size = 65536;
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_plain(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw std::system_error{r, std::system_category()};
	}

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	auto count = read_loop(fd, buf, buf_size);
	::close(fd);
	std::free(buf);
	return count;
}

static auto
read_readahead(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);

	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	using radvisory = struct radvisory;
	auto rd = radvisory{};
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(fd, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_aio_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	using radvisory = struct radvisory;
	auto rd = radvisory{};
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(fd, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	auto count = aio_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_async_nocache(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	using radvisory = struct radvisory;
	auto rd = radvisory{};
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(fd, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	auto count = async_read_loop(fd, buf1.get(), buf2.get(), buf_size);
	::close(fd);
	return count;
}

static auto
mmap_plain(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
mmap_readahead(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();

	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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

	using radvisory = struct radvisory;
	auto rd = radvisory{};
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(fd, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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
	test_read_range(read_readahead, path, "read_readahead", sizes, fs, count);
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
