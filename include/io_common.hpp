/*
** File Name:	io_common.hpp
** Author:	Aditya Ramesh
** Date:	06/11/2014
** Contact:	_@adityaramesh.com
*/

#ifndef Z3BD34381_B22E_4963_8FB9_A5B89E9AFB9A
#define Z3BD34381_B22E_4963_8FB9_A5B89E9AFB9A

#include <system_error>
#include <ccbase/error.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_LINUX || \
    PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/mman.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <aio.h>
#else
	#error "Unsupported kernel."
#endif

static std::system_error
current_system_error()
{ return std::system_error{errno, std::system_category()}; }

static cc::expected<ssize_t>
full_read(int fd, uint8_t* buf, size_t count, off_t offset)
{
	auto c = size_t{0};
	do {
		auto r = ::pread(fd, buf + c, count - c, offset + c);
		if (r > 0) {
			c += r;
		}
		else if (r == 0) {
			return c;
		}
		else {
			if (errno == EINTR) { continue; }
			return current_system_error();
		}
	}
	while (c < count);
	return c;
}

static cc::expected<ssize_t>
full_write(int fd, uint8_t* buf, size_t count, off_t offset)
{
	auto c = size_t{0};
	do {
		auto r = ::pwrite(fd, buf + c, count - c, offset + c);
		if (r > 0) {
			c += r;
		}
		else if (r == 0) {
			return c;
		}
		else {
			if (errno == EINTR) { continue; }
			return current_system_error();
		}
	}
	while (c < count);
	return c;
}

static cc::expected<int>
safe_open(const char* path, int flags)
{
	auto r = int{};
	do {
		r = ::open(path, flags, S_IRUSR | S_IWUSR);
	}
	while (r == -1 && errno == EINTR);

	if (r == -1) { return current_system_error(); }
	return r;
}

static cc::expected<void>
safe_close(int fd)
{
	auto r = ::close(fd);
	if (r == 0) { return true; }
	if (errno != EINTR) { return current_system_error(); }

	for (;;) {
		r = ::close(fd);
		if (errno == EBADF) { return true; }
		if (errno != EINTR) { return current_system_error(); }
	}
}

static cc::expected<off_t>
file_size(int fd)
{
	using stat = struct stat;
	auto st = stat{};
	auto r = ::fstat(fd, &st);
	if (r == -1) { return current_system_error(); }
	return st.st_size;
}

namespace detail {

struct malloc_deleter
{
	void operator()(uint8_t* p) const noexcept
	{ std::free(p); }
};

using buffer_type = std::unique_ptr<uint8_t[], malloc_deleter>;

}

static detail::buffer_type
allocate_aligned(size_t align, size_t count)
{
	auto p = (uint8_t*)nullptr;
	auto r = ::posix_memalign((void**)&p, align, count);
	if (r != 0) { throw std::system_error{r, std::system_category()}; }
	return detail::buffer_type{p};
}

static void
truncate(int fd, off_t fs)
{
	if (::ftruncate(fd, fs) == -1) {
		throw current_system_error();
	}
}

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU

static inline cc::expected<void>
purge_cache()
{
	if (std::system("purge") == -1) {
		throw std::runtime_error{"Failed to purge cache."};
	}
	return true;
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

static void
disable_cache(int fd)
{
	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		throw current_system_error();
	}
}

static void
enable_rdahead(int fd)
{
	if (::fcntl(fd, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}
}

static void
enable_rdadvise(int fd, off_t fs)
{
	using radvisory = struct radvisory;
	auto rd = radvisory{};
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(fd, F_RDADVISE, &rd) == -1) {
		throw std::system_error{errno, std::system_category()};
	}
}

#endif

#if PLATFORM_KERNEL == PLATFORM_KERNEL_LINUX

static inline cc::expected<void>
purge_cache()
{
	if (std::system("sync; echo 3 > /proc/sys/vm/drop_caches") == -1) {
		throw std::runtime_error{"Failed to purge cache."};
	}
	return true;
}

static void
fadvise_sequential_read(int fd, off_t fs)
{
	// It turns out that this makes things slower on the machines that I
	// tested.
	// ::posix_fadvise(fd, 0, fs, POSIX_FADV_WILLNEED);
	::posix_fadvise(fd, 0, fs, POSIX_FADV_NOREUSE);
	::posix_fadvise(fd, 0, fs, POSIX_FADV_SEQUENTIAL);
}

static void
preallocate(int fd, size_t count)
{
	auto r = ::posix_fallocate(fd, 0, count);
	if (r != 0) {
		throw std::system_error{r, std::system_category()};
	}
}

#endif

#endif
