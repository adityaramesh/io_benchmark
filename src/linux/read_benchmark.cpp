/*
** File Name:	read_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/04/2014
** Contact:	_@adityaramesh.com
*/

// read plain
// read direct
// read fadvise
// mmap plain
// mmap direct
// mmap fadvise

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

#if PLATFORM_KERNEL == PLATFORM_KERNEL_LINUX
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
	if (std::system("sync; echo 1 > /proc/sys/vm/drop_caches") == -1) {
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
read_check(const char* path)
{
	static constexpr auto buf_size = 65536;
	auto fd = get_fd(path, O_RDONLY);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto r = full_read(fd, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		count += std::count_if(buf.get(), buf.get() + buf_size,
			[](auto x) { return x == needle; });
	}
	::close(fd);
	return count;
}

static auto
read_plain(const char* path, const std::size_t buf_size)
{
	auto fd = get_fd(path, O_RDONLY | O_NOATIME);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto r = full_read(fd, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		count += std::count_if(buf.get(), buf.get() + buf_size,
			[](auto x) { return x == needle; });
	}
	::close(fd);
	return count;
}

static auto
read_direct(const char* path, const std::size_t buf_size)
{
	auto fd = get_fd(path, O_RDONLY | O_NOATIME | O_DIRECT);
	auto buf = 
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto r = full_read(fd, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		count += std::count_if(buf.get(), buf.get() + buf_size,
			[](auto x) { return x == needle; });
	}
	::close(fd);
	return count;
}

static auto
read_advise(const char* path, const std::size_t buf_size)
{
	auto fd = get_fd(path, O_RDONLY);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};
	auto count = off_t{0};

	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	for (;;) {
		auto r = full_read(fd, buf.get(), buf_size, off);
		if (r == 0) {
			break;
		}
		else if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		count += std::count_if(buf.get(), buf.get() + buf_size,
			[](auto x) { return x == needle; });
	}
	::close(fd);
	return count;
}

/*
static auto
mmap_plain(const char* path)
{
	auto fd = get_fd(path, O_RDONLY);
	auto fs = file_size(fd);
	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
mmap_direct(const char* path)
{
	auto fd = get_fd(path, O_RDONLY);
	auto fs = file_size(fd);
	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

static auto
mmap_advise(const char* path)
{
	auto fd = get_fd(path, O_RDONLY);
	auto fs = file_size(fd);
	struct radvisory rd;
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
*/

template <class Function>
static auto test_function(
	const Function& f,
	const char* name,
	const unsigned count
)
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using milliseconds = duration<double, std::ratio<1, 1000>>;
	auto mean = double{0};

	for (auto i = 0; i != num_trials; ++i) {
		auto t1 = high_resolution_clock::now();
		if (f() != count) { throw std::runtime_error{"Invalid count."}; }
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
	if (argc < 2) {
		cc::errln("Error: too few arguments.");
		return EXIT_FAILURE;
	}
	else if (argc > 2) {
		cc::errln("Error: too many arguments.");
		return EXIT_FAILURE;
	}

	auto path = argv[1];
	auto count = read_check(path);
	purge_cache();

	auto kb = 1024;
	//test_function(std::bind(&read_plain, path, 4 * kb), "plain read 4 Kb", count);
	//test_function(std::bind(&read_plain, path, 16 * kb), "plain read 16 Kb", count);
	//test_function(std::bind(&read_plain, path, 64 * kb), "plain read 64 Kb", count);
	//test_function(std::bind(&read_plain, path, 256 * kb), "plain read 256 Kb", count);
	//test_function(std::bind(&read_plain, path, 1024 * kb), "plain read 1024 Kb", count);
	//test_function(std::bind(&read_plain, path, 4096 * kb), "plain read 4096 Kb", count);
	//test_function(std::bind(&read_plain, path, 16384 * kb), "plain read 16384 Kb", count);
	//test_function(std::bind(&read_plain, path, 65536 * kb), "plain read 65536 Kb", count);
	//test_function(std::bind(&read_plain, path, 262144 * kb), "plain read 262144 Kb", count);
	//test_function(std::bind(&read_nocache, path, 4 * kb), "read nocache 4 Kb", count);
	//test_function(std::bind(&read_nocache, path, 16 * kb), "read nocache 16 Kb", count);
	//test_function(std::bind(&read_nocache, path, 64 * kb), "read nocache 64 Kb", count);
	//test_function(std::bind(&read_nocache, path, 256 * kb), "read nocache 256 Kb", count);
	//test_function(std::bind(&read_nocache, path, 1024 * kb), "read nocache 1024 Kb", count);
	//test_function(std::bind(&read_nocache, path, 4096 * kb), "read nocache 4096 Kb", count);
	//test_function(std::bind(&read_nocache, path, 16384 * kb), "read nocache 16384 Kb", count);
	//test_function(std::bind(&read_nocache, path, 65536 * kb), "read nocache 65536 Kb", count);
	//test_function(std::bind(&read_nocache, path, 262144 * kb), "read nocache 262144 Kb", count);
	//test_function(std::bind(&read_readahead, path, 4 * kb), "read readahead 4 Kb", count);
	//test_function(std::bind(&read_readahead, path, 16 * kb), "read readahead 16 Kb", count);
	//test_function(std::bind(&read_readahead, path, 64 * kb), "read readahead 64 Kb", count);
	//test_function(std::bind(&read_readahead, path, 256 * kb), "read readahead 256 Kb", count);
	//test_function(std::bind(&read_readahead, path, 1024 * kb), "read readahead 1024 Kb", count);
	//test_function(std::bind(&read_readahead, path, 4096 * kb), "read readahead 4096 Kb", count);
	//test_function(std::bind(&read_readahead, path, 16384 * kb), "read readahead 16384 Kb", count);
	//test_function(std::bind(&read_readahead, path, 65536 * kb), "read readahead 65536 Kb", count);
	//test_function(std::bind(&read_readahead, path, 262144 * kb), "read readahead 262144 Kb", count);
	//test_function(std::bind(&read_advise, path, 4 * kb), "read advise 4 Kb", count);
	//test_function(std::bind(&read_advise, path, 16 * kb), "read advise 16 Kb", count);
	//test_function(std::bind(&read_advise, path, 64 * kb), "read advise 64 Kb", count);
	//test_function(std::bind(&read_advise, path, 256 * kb), "read advise 256 Kb", count);
	//test_function(std::bind(&read_advise, path, 1024 * kb), "read advise 1024 Kb", count);
	//test_function(std::bind(&read_advise, path, 4096 * kb), "read advise 4096 Kb", count);
	//test_function(std::bind(&read_advise, path, 16384 * kb), "read advise 16384 Kb", count);
	//test_function(std::bind(&read_advise, path, 65536 * kb), "read advise 65536 Kb", count);
	//test_function(std::bind(&read_advise, path, 262144 * kb), "read advise 262144 Kb", count);
	//test_function(std::bind(&mmap_plain, path), "mmap plain", count);
	//test_function(std::bind(&mmap_readahead, path), "mmap readahead", count);
	//test_function(std::bind(&mmap_advise, path), "mmap advise", count);
}
