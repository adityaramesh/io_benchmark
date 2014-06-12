/*
** File Name:	read_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/03/2014
** Contact:	_@adityaramesh.com
**
** This is a simple benchmark that compares various methods to sequentially read
** a file.
**
** - Best results on Macbook Pro:
**   - 256 Mb or above: read + nocache + 16384 Kb
**   - 128 Mb or below: read + advise + 4 Kb
*/

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <ratio>
#include <ccbase/format.hpp>
#include <boost/range/numeric.hpp>
#include <common.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <aio.h>
	#include <sys/mman.h>
#else
	#error "Unsupported kernel."
#endif

// Number of times to run each method.
static constexpr auto num_trials = 5;
// Special byte value used to verify correctness.
static constexpr auto needle = uint8_t{0xFF};

static auto
read_loop(int fd, uint8_t* buf, size_t buf_size)
{
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto r = full_read(fd, buf, buf_size, off).get();
		if (r == 0) { break; }
		assert(r == buf_size);

		off += buf_size;
		count += std::count_if(buf, buf + buf_size,
			[](auto x) { return x == needle; });
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

	auto l = std::array<aiocb*, 1>{&cb};
	auto off = off_t{0};
	auto count = off_t{0};

	auto buf1_active = true;
	auto r = full_read(fd, buf1, buf_size, off).get();
	assert(r == buf_size);
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
		r = ::aio_return(&cb);
		if (r == -1) { throw current_system_error(); }
		if (r == 0) { break; }
		assert(r == buf_size);
		off += buf_size;
	}
	return count;
}

static void
read_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
	std::atomic<unsigned>& cv1,
	std::atomic<unsigned>& cv2
)
{
	auto off = off_t(buf_size);
	auto buf1_active = true;

	for (;;) {
		if (buf1_active) {
			while (cv2 != 0) {}

			auto r = full_read(fd, buf2, buf_size, off).get();
			if (r == 0) {
				cv2 = 2;
				return;
			}
			else {
				cv2 = 1;
			}
		}
		else {
			while (cv1 != 0) {}

			auto r = full_read(fd, buf1, buf_size, off).get();
			if (r == 0) {
				cv1 = 2;
				return;
			}
			else {
				cv1 = 1;
			}
		}
		buf1_active = !buf1_active;
		off += buf_size;
	}
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
	auto fs = file_size(fd).get();
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

	std::atomic<unsigned> cv1{1};
	std::atomic<unsigned> cv2{0};
	auto count = off_t{0};
	auto off = unsigned{0};
	auto buf1_active = true;

	auto t = std::thread(read_worker, fd, buf1.get(), buf2.get(), buf_size,
		std::ref(cv1), std::ref(cv2));
	auto r = full_read(fd, buf1.get(), buf_size, 0).get();
	assert(r == buf_size);

	for (;;) {
		if (buf1_active) {
			while (cv1 == 0) {}
			if (cv1 == 2) { goto exit; }
			count += std::count_if(buf1.get(), buf1.get() + buf_size,
				[](auto x) { return x == needle; });
			cv1 = 0;
		}
		else {
			while (cv2 == 0) {}
			if (cv2 == 2) { goto exit; }
			count += std::count_if(buf2.get(), buf2.get() + buf_size,
				[](auto x) { return x == needle; });
			cv2 = 0;
		}
		buf1_active = !buf1_active;
	}
exit:
	t.join();
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

template <class Function>
static void test(const Function& f, const char* name, unsigned count)
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using milliseconds = duration<double, std::ratio<1, 1000>>;

	auto mean = double{0};
	auto sample = std::array<double, num_trials>{};

	for (auto i = 0; i != num_trials; ++i) {
		auto t1 = high_resolution_clock::now();
		if (f() != count) { throw std::runtime_error{"Invalid count."}; }
		auto t2 = high_resolution_clock::now();
		auto ms = duration_cast<milliseconds>(t2 - t1).count();
		sample[i] = ms;
		mean += ms;
		purge_cache().get();
	}

	mean /= num_trials;
	auto stddev = std::sqrt(1.0 / num_trials * boost::accumulate(
		sample, 0.0, [&](auto x, auto y) {
			return x + std::pow(y - mean, 2);
		}));

	std::printf("%-40s%-20f%f\n", name, mean, stddev);
	std::fflush(stdout);
}

template <class Function, class Range>
static void test_range(
	const Function& func,
	const char* path,
	const char* name,
	const Range& range,
	unsigned count,
	off_t file_size
)
{
	static constexpr auto kb = 1024;
	auto buf = std::array<char, 40>{};

	for (const auto& bs : range) {
		if (bs * kb <= file_size) {
			std::snprintf(buf.data(), 40, "%s %d Kb", name, bs);
			test(std::bind(func, path, bs * kb), buf.data(), count);
		}
	}
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
	auto sizes = {4, 16, 64, 256, 1024, 4096, 16384, 65536, 262144};
	purge_cache().get();

	std::printf("%-40s%-20s%s\n", "Method", "Mean (ms)", "Stddev (ms)");
	std::fflush(stdout);
	//test_range(&read_plain, path, "read_plain", sizes, count, fs);
	//test_range(&read_nocache, path, "read_nocache", sizes, count, fs);
	//test_range(&read_readahead, path, "read_readahead", sizes, count, fs);
	//test_range(&read_rdadvise, path, "read_rdadvise", sizes, count, fs);
	//test_range(&read_aio_nocache, path, "read_aio_nocache", sizes, count, fs);
	test_range(&read_aio_rdadvise, path, "read_aio_rdadvise", sizes, count, fs);
	test_range(&read_async_rdadvise, path, "read_async_rdadvise", sizes, count, fs);
	//test(std::bind(&mmap_plain, path), "mmap_plain", count);
	//test(std::bind(&mmap_readahead, path), "mmap_readahead", count);
	//test(std::bind(&mmap_rdadvise, path), "mmap_rdadvise", count);
}
