/*
** File Name:	write_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/04/2014
** Contact:	_@adityaramesh.com
**
** - https://github.com/Feh/write-patterns
** - http://blog.plenz.com/2014-04/so-you-want-to-write-to-a-file-real-fast.html
** - https://blog.mozilla.org/tglek/2010/09/09/help-wanted-does-fcntlf_preallocate-work-as-advertised-on-osx/
** - Best results: preallocate + truncate + 256 Kb.
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
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

static constexpr auto num_trials = 5;

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

static void
fill_buffer(uint8_t* p, const std::size_t n)
{
	auto gen = std::mt19937{std::random_device{}()};
	auto dist = std::uniform_int_distribution<uint8_t>(0, 255);
	std::generate(p, p + n, [&]() { return dist(gen); });
}

static void
write_plain(const char* path, const std::size_t buf_size, const std::size_t n)
{
	auto fd = get_fd(path, O_WRONLY | O_CREAT | O_TRUNC);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};

	for (;;) {
		fill_buffer(buf.get(), buf_size);
		auto r = full_write(fd, buf.get(), buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		if (off >= n) { break; }
	}
	::close(fd);
}

static void
write_nocache(const char* path, const std::size_t buf_size, const std::size_t n)
{
	auto fd = get_fd(path, O_WRONLY | O_CREAT | O_TRUNC);
	auto buf = (uint8_t*)nullptr;
	auto off = off_t{0};

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw std::system_error{r, std::system_category()};
	}

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	for (;;) {
		fill_buffer(buf, buf_size);
		auto r = full_write(fd, buf, buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		if (off >= n) { break; }
	}
	::close(fd);
	std::free(buf);
}

static void
write_preallocate(const char* path, const std::size_t buf_size, const std::size_t n)
{
	auto fd = get_fd(path, O_WRONLY | O_CREAT | O_TRUNC);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};

	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = n;
	if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
			cc::errln("Warning: failed to preallocate space.");
			goto write;
		}
	}

write:
	for (;;) {
		fill_buffer(buf.get(), buf_size);
		auto r = full_write(fd, buf.get(), buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		if (off >= n) { break; }
	}
	::close(fd);
}

static void
write_preallocate_truncate(const char* path, const std::size_t buf_size, const std::size_t n)
{
	auto fd = get_fd(path, O_WRONLY | O_CREAT | O_TRUNC);
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto off = off_t{0};
	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = n;
	if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
			cc::errln("Warning: failed to preallocate space.");
			goto write;
		}
	}
	if (::ftruncate(fd, n) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

write:
	for (;;) {
		fill_buffer(buf.get(), buf_size);
		auto r = full_write(fd, buf.get(), buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		if (off >= n) { break; }
	}
	::close(fd);
}

static void
write_preallocate_truncate_nocache(const char* path, const std::size_t buf_size, const std::size_t n)
{
	auto fd = get_fd(path, O_WRONLY | O_CREAT | O_TRUNC);
	auto buf = (uint8_t*)nullptr;
	auto off = off_t{0};

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw std::system_error{r, std::system_category()};
	}

	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = n;
	if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
			cc::errln("Warning: failed to preallocate space.");
			goto write;
		}
	}
	if (::ftruncate(fd, n) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

write:
	for (;;) {
		fill_buffer(buf, buf_size);
		auto r = full_write(fd, buf, buf_size, off);
		if (r == -1) {
			throw std::system_error{errno, std::system_category()};
		}

		assert(r == buf_size);
		off += buf_size;
		if (off >= n) { break; }
	}
	::close(fd);
	std::free(buf);
}

static void
write_mmap(const char* path, const std::size_t n)
{
	auto fd = get_fd(path, O_RDWR | O_CREAT | O_TRUNC);
	struct fstore fs;
	fs.fst_posmode = F_ALLOCATECONTIG;
	fs.fst_offset = F_PEOFPOSMODE;
	fs.fst_length = n;
	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}
	if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
		fs.fst_posmode = F_ALLOCATEALL;
		if (::fcntl(fd, F_PREALLOCATE, &fs) == -1) {
			throw std::runtime_error{"Warning: failed to preallocate space."};
		}
	}
	if (::ftruncate(fd, n) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

write:
	auto p = (uint8_t*)::mmap(nullptr, n, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (p == (void*)-1) { throw std::system_error{errno, std::system_category()}; }
	fill_buffer(p, n);
	::close(fd);
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

	auto count = std::atoi(argv[1]);
	if (count <= 0) {
		cc::errln("Error: count must be positive.");
		return EXIT_FAILURE;
	}

	auto kb = 1024;
	auto path = "data/test.dat";

	// Dummy write to create file.
	write_plain(path, 4 * kb, count);

	test_function(std::bind(&write_mmap, path, count), "write mmap");
	test_function(std::bind(&write_plain, path, 4 * kb, count), "plain write 4 Kb");
	test_function(std::bind(&write_plain, path, 16 * kb, count), "plain write 16 Kb");
	test_function(std::bind(&write_plain, path, 64 * kb, count), "plain write 64 Kb");
	test_function(std::bind(&write_plain, path, 256 * kb, count), "plain write 256 Kb");
	test_function(std::bind(&write_plain, path, 1024 * kb, count), "plain write 1024 Kb");
	test_function(std::bind(&write_plain, path, 4096 * kb, count), "plain write 4096 Kb");
	test_function(std::bind(&write_plain, path, 16384 * kb, count), "plain write 16384 Kb");
	test_function(std::bind(&write_nocache, path, 4 * kb, count), "nocache write 4 Kb");
	test_function(std::bind(&write_nocache, path, 16 * kb, count), "nocache write 16 Kb");
	test_function(std::bind(&write_nocache, path, 64 * kb, count), "nocache write 64 Kb");
	test_function(std::bind(&write_nocache, path, 256 * kb, count), "nocache write 256 Kb");
	test_function(std::bind(&write_nocache, path, 1024 * kb, count), "nocache write 1024 Kb");
	test_function(std::bind(&write_nocache, path, 4096 * kb, count), "nocache write 4096 Kb");
	test_function(std::bind(&write_nocache, path, 16384 * kb, count), "nocache write 16384 Kb");
	test_function(std::bind(&write_plain, path, 65536 * kb, count), "plain write 65536 Kb");
	test_function(std::bind(&write_preallocate, path, 4 * kb, count), "preallocate write 4 Kb");
	test_function(std::bind(&write_preallocate, path, 16 * kb, count), "preallocate write 16 Kb");
	test_function(std::bind(&write_preallocate, path, 64 * kb, count), "preallocate write 64 Kb");
	test_function(std::bind(&write_preallocate, path, 256 * kb, count), "preallocate write 256 Kb");
	test_function(std::bind(&write_preallocate, path, 1024 * kb, count), "preallocate write 1024 Kb");
	test_function(std::bind(&write_preallocate, path, 4096 * kb, count), "preallocate write 4096 Kb");
	test_function(std::bind(&write_preallocate, path, 16384 * kb, count), "preallocate write 16384 Kb");
	test_function(std::bind(&write_preallocate, path, 65536 * kb, count), "preallocate write 65536 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 4 * kb, count), "preallocate truncate write 4 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 16 * kb, count), "preallocate truncate write 16 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 64 * kb, count), "preallocate truncate write 64 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 256 * kb, count), "preallocate truncate write 256 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 1024 * kb, count), "preallocate truncate write 1024 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 4096 * kb, count), "preallocate truncate write 4096 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 16384 * kb, count), "preallocate truncate write 16384 Kb");
	test_function(std::bind(&write_preallocate_truncate, path, 65536 * kb, count), "preallocate truncate write 65536 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 4 * kb, count), "preallocate truncate nocache write 4 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 16 * kb, count), "preallocate truncate nocache write 16 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 64 * kb, count), "preallocate truncate nocache write 64 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 256 * kb, count), "preallocate truncate nocache write 256 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 1024 * kb, count), "preallocate truncate nocache write 1024 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 4096 * kb, count), "preallocate truncate nocache write 4096 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 16384 * kb, count), "preallocate truncate nocache write 16384 Kb");
	test_function(std::bind(&write_preallocate_truncate_nocache, path, 65536 * kb, count), "preallocate truncate nocache write 65536 Kb");
}
