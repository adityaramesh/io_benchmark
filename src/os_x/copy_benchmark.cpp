/*
** File Name:	copy_benchmark.cpp
** Author:	Aditya Ramesh
** Date:	06/03/2014
** Contact:	_@adityaramesh.com
**
** - Best results: copy_mmap
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

#include <configuration.hpp>
#include <copy_common.hpp>
#include <test.hpp>

static auto
copy_nocache(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size).get();
	disable_cache(in);
	disable_cache(out);

	copy_loop(in, out, buf, buf_size);
	::close(in);
	::close(out);
	std::free(buf);
}

static auto
copy_rdahead_preallocate(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto buf = allocate_aligned(4096, buf_size).get();
	enable_rdahead(in);

	//preallocate(out, fs);
	//if (::fcntl(out, F_NOCACHE, 1) == -1) {
	//	throw current_system_error();
	//}
	//if (::ftruncate(out, fs) == -1) {
	//	throw current_system_error();
	//}

	copy_loop(in, out, buf, buf_size);
	::close(in);
	::close(out);
	std::free(buf);
}

static auto
copy_rdadvise_preallocate(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();
	auto buf = allocate_aligned(4096, buf_size).get();
	enable_rdadvise(in, fs);

	//preallocate(out, fs);
	//if (::fcntl(out, F_NOCACHE, 1) == -1) {
	//	throw current_system_error();
	//}
	//if (::ftruncate(out, fs) == -1) {
	//	throw current_system_error();
	//}

	copy_loop(in, out, buf, buf_size);
	::close(in);
	::close(out);
}

static auto
copy_mmap(const char* src, const char* dst)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto fs = file_size(in).get();

	/*
	** Strangely, copying is fastest when we use `F_NOCACHE` for reading but
	** not for writing. I do not know why. The `F_RDAHEAD` and `F_RDADVISE`
	** flags do not help.
	*/
	disable_cache(in);
	preallocate(out, fs);
	truncate(out, fs);

	auto src_buf = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, in, 0);
	auto dst_buf = (uint8_t*)::mmap(nullptr, fs, PROT_WRITE, MAP_PRIVATE, out, 0);
	std::copy(src_buf, src_buf + fs, dst_buf);
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
	test_copy_range(copy_nocache, src, dst, "copy_nocache", sizes, fs);
	test_copy_range(copy_rdahead_preallocate, src, dst, "copy_rdahead_preallocate", sizes, fs);
	test_copy_range(copy_rdadvise_preallocate, src, dst, "copy_rdadvise_preallocate", sizes, fs);
	test_write(std::bind(copy_mmap, src, dst), "copy_mmap");
}
