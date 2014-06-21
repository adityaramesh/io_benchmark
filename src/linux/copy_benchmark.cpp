/*
** File Name:	copy_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/19/2014
** Contact:	_@adityaramesh.com
**
** - Best results:
**   - copy_splice_preallocate_fadvise (the buffer size does not really matter)
**   - copy_sendfile_preallocate_fadvise (no buffer allocation required)
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <memory>
#include <ccbase/format.hpp>
#include <ccbase/platform.hpp>

#include <configuration.hpp>
#include <copy_common.hpp>
#include <test.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_LINUX
	#include <sys/sendfile.h>
#else
	#error "Unsupported platform."
#endif

static auto
copy_direct(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY | O_DIRECT).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC | O_DIRECT).get();
	auto buf = allocate_aligned(4096, buf_size);
	copy_loop(in, out, buf.get(), buf_size);
	::close(in);
	::close(out);
}

static auto
copy_preallocate(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	auto buf = allocate_aligned(4096, buf_size);
	preallocate(out, fs);
	copy_loop(in, out, buf.get(), buf_size);
	::close(in);
	::close(out);
}

static auto
copy_mmap_plain(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	preallocate(out, fs);

	auto src_buf = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, in, 0);
	auto dst_buf = (uint8_t*)::mmap(nullptr, fs, PROT_WRITE, MAP_PRIVATE, out, 0);
	std::copy(src_buf, src_buf + fs, dst_buf);
	::close(in);
	::close(out);
}

static auto
copy_mmap_fadvise(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	fadvise_sequential_read(in, fs);
	preallocate(out, fs);

	auto src_buf = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, in, 0);
	auto dst_buf = (uint8_t*)::mmap(nullptr, fs, PROT_WRITE, MAP_PRIVATE, out, 0);
	std::copy(src_buf, src_buf + fs, dst_buf);
	::close(in);
	::close(out);
}

static auto
copy_splice(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();

	auto in_pipe = int{};
	auto out_pipe = int{};
	std::tie(out_pipe, in_pipe) = make_pipe().get();
	splice_loop(in, out, in_pipe, out_pipe, buf_size, fs);
	::close(in);
	::close(out);
}

static auto
copy_splice_preallocate(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	preallocate(out, fs);

	auto in_pipe = int{};
	auto out_pipe = int{};
	std::tie(out_pipe, in_pipe) = make_pipe().get();
	splice_loop(in, out, in_pipe, out_pipe, buf_size, fs);
	::close(in);
	::close(out);
}

static auto
copy_splice_fadvise(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	fadvise_sequential_read(in, fs);

	auto in_pipe = int{};
	auto out_pipe = int{};
	std::tie(out_pipe, in_pipe) = make_pipe().get();
	splice_loop(in, out, in_pipe, out_pipe, buf_size, fs);
	::close(in);
	::close(out);
}

static auto
copy_splice_preallocate_fadvise(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	fadvise_sequential_read(in, fs);
	preallocate(out, fs);

	auto in_pipe = int{};
	auto out_pipe = int{};
	std::tie(out_pipe, in_pipe) = make_pipe().get();
	splice_loop(in, out, in_pipe, out_pipe, buf_size, fs);
	::close(in);
	::close(out);
}

static auto
copy_sendfile(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();

	if (::sendfile(out, in, nullptr, fs) == -1) {
		throw current_system_error();
	}
	::close(in);
	::close(out);
}

static auto
copy_sendfile_preallocate(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	preallocate(out, fs);

	if (::sendfile(out, in, nullptr, fs) == -1) {
		throw current_system_error();
	}
	::close(in);
	::close(out);
}

static auto
copy_sendfile_fadvise(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	fadvise_sequential_read(in, fs);

	if (::sendfile(out, in, nullptr, fs) == -1) {
		throw current_system_error();
	}
	::close(in);
	::close(out);
}

static auto
copy_sendfile_preallocate_fadvise(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	preallocate(out, fs);
	fadvise_sequential_read(in, fs);

	if (::sendfile(out, in, nullptr, fs) == -1) {
		throw current_system_error();
	}
	::close(in);
	::close(out);
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

	auto src = argv[1];
	auto dst = argv[2];
	auto sizes = {4, 8, 12, 16, 24, 32, 40, 48, 56, 64, 256, 1024, 4096, 16384, 65536, 262144};

	auto fd = safe_open(src, O_RDONLY).get();
	auto fs = file_size(fd).get();
	safe_close(fd).get();

	std::printf("%s, %s, %s\n", "Method", "Mean (ms)", "Stddev (ms)");
	std::fflush(stdout);
	test_copy_range(copy_plain, src, dst, "copy_plain", sizes, fs);
	test_copy_range(copy_direct, src, dst, "copy_direct", sizes, fs);
	test_copy_range(copy_preallocate, src, dst, "copy_preallocate", sizes, fs);
	test_write(std::bind(copy_mmap_plain, src, dst), "copy_mmap_plain");
	test_write(std::bind(copy_mmap_fadvise, src, dst), "copy_mmap_fadvise");
	test_copy_range(copy_splice, src, dst, "copy_splice", sizes, fs);
	test_copy_range(copy_splice_preallocate, src, dst, "copy_splice_preallocate", sizes, fs);
	test_copy_range(copy_splice_preallocate_fadvise, src, dst, "copy_splice_preallocate_fadvise", sizes, fs);
	test_copy_range(copy_splice_fadvise, src, dst, "copy_splice_fadvise", sizes, fs);
	test_write(std::bind(copy_sendfile, src, dst), "copy_sendfile", fs);
	test_write(std::bind(copy_sendfile_preallocate, src, dst), "copy_sendfile_preallocate", fs);
	test_write(std::bind(copy_sendfile_preallocate_fadvise, src, dst), "copy_sendfile_preallocate_fadvise", fs);
	test_write(std::bind(copy_sendfile_fadvise, src, dst), "copy_sendfile_fadvise", fs);
}
