/*
** File Name:	common.hpp
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
	#include <unistd.h>
	#include <fcntl.h>
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

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU

static cc::expected<void>
purge_cache()
{
	if (std::system("purge") == -1) {
		throw std::runtime_error{"Failed to purge cache."};
	}
	return true;
}

#endif

#endif
