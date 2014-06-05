/*
** File Name:	copy_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/03/2014
** Contact:	_@adityaramesh.com
**
** - Best results: advise + preallocate + truncate + 64 Kb.
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <ratio>
#include <system_error>
#include <thread>
#include <ccbase/format.hpp>
#include <ccbase/platform.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <cstdlib>
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <sys/types.h>
#else
	#error "Unsupported kernel."
#endif

// Number of times to run each IO function.
static constexpr auto num_trials = 5;
// Special byte value used to verify correctness.
static constexpr auto needle = uint8_t{0xFF};

// mmap (advise, prealloc + truncate)

static off_t
file_size(const int fd)
{
	struct stat buf;
	auto r = ::fstat(fd, &buf);
	if (r == -1) {
		throw std::system_error{errno, std::system_category()};
	}
	return buf.st_size;
}

static void
purge_cache()
{
	if (std::system("purge") == -1) {
		throw std::runtime_error{"Failed to purge cache."};
	}
}

static ssize_t
full_read(int fd, void* buf, size_t count, off_t offset)
{
	auto c = size_t{0};
	do {
		auto r = ::pread(fd, (uint8_t*)buf + c, count - c, offset + c);
		if (r > 0) {
			c += r;
		}
		else if (r == 0) {
			return c;
		}
		else {
			if (errno == EINTR) { continue; }
			return r;
		}
	}
	while (c < count);
	return c;
}

static ssize_t
full_write(int fd, void* buf, std::size_t count, off_t offset)
{
	auto c = size_t{0};
	do {
		auto r = ::pwrite(fd, (uint8_t*)buf + c, count - c, offset + c);
		if (r > 0) {
			c += r;
		}
		else if (r == 0) {
			return c;
		}
		else {
			if (errno == EINTR) { continue; }
			return r;
		}
	}
	while (c < count);
	return c;
}

static auto
get_fd(const char* path, int flags)
{
	auto fd = ::open(path, flags, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		throw std::system_error{errno, std::system_category()};
	}
	return fd;
}

static auto
simple_copy(const char* src, const char* dst, const std::size_t buf_size)
{
	auto in = get_fd(src, O_RDONLY);
	auto out = get_fd(dst, O_RDWR | O_CREAT | O_TRUNC);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};

	for (;;) {
		auto r = full_read(in, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);

		r = full_write(out, buf.get(), buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);
		off += buf_size;
	}
	::close(in);
	::close(out);
}

static auto
copy_nocache(const char* src, const char* dst, const std::size_t buf_size)
{
	auto in = get_fd(src, O_RDONLY);
	auto out = get_fd(dst, O_RDWR | O_CREAT | O_TRUNC);
	auto buf = (uint8_t*)nullptr;
	auto off = off_t{0};

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw std::system_error{r, std::system_category()};
	}

	for (;;) {
		auto r = full_read(in, buf, buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);

		r = full_write(out, buf, buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);
		off += buf_size;
	}
	::close(in);
	::close(out);
	std::free(buf);
}

static auto
copy_advise_preallocate(const char* src, const char* dst, const std::size_t buf_size)
{
	auto in = get_fd(src, O_RDONLY);
	auto out = get_fd(dst, O_RDWR | O_CREAT | O_TRUNC);
	auto fsz = file_size(in);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};

	struct radvisory rd;
	rd.ra_offset = 0;
	rd.ra_count = fsz;
	if (::fcntl(in, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = fsz;
	if (::fcntl(out, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(out, F_PREALLOCATE, &fs) == -1) {
			cc::errln("Warning: failed to preallocate space.");
			goto copy;
		}
	}
	if (::ftruncate(out, fsz) == -1) {
		throw std::system_error{errno, std::system_category()};
	}
copy:
	for (;;) {
		auto r = full_read(in, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);

		r = full_write(out, buf.get(), buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}
		assert(r == buf_size);
		off += buf_size;
	}
	::close(in);
	::close(out);
}

template <class Function>
static auto test_function(const Function& f, const char* name)
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using milliseconds = duration<double, std::ratio<1, 1000>>;
	auto mean = double{0};

	for (auto i = 0; i != num_trials; ++i) {
		auto t1 = high_resolution_clock::now();
		f();
		auto t2 = high_resolution_clock::now();
		auto ms = duration_cast<milliseconds>(t2 - t1).count();
		mean += ms;
		purge_cache();
	}
	mean /= num_trials;
	cc::println("Function: $. Mean time: $ ms.", name, mean);
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		cc::errln("Error: too few arguments.");
		return EXIT_FAILURE;
	}
	else if (argc > 3) {
		cc::errln("Error: too many arguments.");
		return EXIT_FAILURE;
	}

	auto kb = 1024;
	auto src = argv[1];
	auto dst = argv[2];

	test_function(std::bind(&simple_copy, src, dst, 4 * kb), "simple copy 4 Kb");
	test_function(std::bind(&simple_copy, src, dst, 16 * kb), "simple copy 16 Kb");
	test_function(std::bind(&simple_copy, src, dst, 64 * kb), "simple copy 64 Kb");
	test_function(std::bind(&simple_copy, src, dst, 256 * kb), "simple copy 256 Kb");
	test_function(std::bind(&simple_copy, src, dst, 1024 * kb), "simple copy 1024 Kb");
	test_function(std::bind(&simple_copy, src, dst, 4096 * kb), "simple copy 4096 Kb");
	test_function(std::bind(&simple_copy, src, dst, 16384 * kb), "simple copy 16384 Kb");
	test_function(std::bind(&simple_copy, src, dst, 65536 * kb), "simple copy 65536 Kb");
	//test_function(std::bind(&simple_copy, src, dst, 262144 * kb), "simple copy 262144 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 4 * kb), "copy nocache 4 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 16 * kb), "copy nocache 16 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 64 * kb), "copy nocache 64 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 256 * kb), "copy nocache 256 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 1024 * kb), "copy nocache 1024 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 4096 * kb), "copy nocache 4096 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 16384 * kb), "copy nocache 16384 Kb");
	test_function(std::bind(&copy_nocache, src, dst, 65536 * kb), "copy nocache 65536 Kb");
	//test_function(std::bind(&copy_nocache, src, dst, 262144 * kb), "copy nocache 262144 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 4 * kb), "copy advise preallocate 4 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 16 * kb), "copy advise preallocate 16 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 64 * kb), "copy advise preallocate 64 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 256 * kb), "copy advise preallocate 256 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 1024 * kb), "copy advise preallocate 1024 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 4096 * kb), "copy advise preallocate 4096 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 16384 * kb), "copy advise preallocate 16384 Kb");
	test_function(std::bind(&copy_advise_preallocate, src, dst, 65536 * kb), "copy advise preallocate 65536 Kb");
	//test_function(std::bind(&copy_advise_preallocate, src, dst, 262144 * kb), "copy advise preallocate 262144 Kb");
}
