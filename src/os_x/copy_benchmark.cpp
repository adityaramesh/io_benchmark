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
#include <io_common.hpp>
#include <test.hpp>

#if PLATFORM_KERNEL == PLATFORM_KERNEL_XNU
	#include <sys/mman.h>
#else
	#error "Unsupported kernel."
#endif

// mmap (advise, prealloc + truncate)

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
copy_loop(int in, int out, uint8_t* buf, size_t buf_size)
{
	auto off = off_t{0};
	for (;;) {
		auto r = full_read(in, buf, buf_size, off).get();
		auto s = full_write(out, buf, r, off).get();
		assert(r == s);
		if (size_t(r) < buf_size) { return; }
		off += buf_size;
	}
}

static void
write_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
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
			r = full_write(fd, buf2, s, off).get();
			if (size_t(s) < buf_size) { return; }
			cv2.store(-1, std::memory_order_release);
		}
		else {
			while (cv1.load(std::memory_order_acquire) == -1) {}
			s = cv1.load(std::memory_order_relaxed);
			r = full_write(fd, buf1, s, off).get();
			if (size_t(s) < buf_size) { return; }
			cv1.store(-1, std::memory_order_release);
		}
		assert(r == s);
		buf1_active = !buf1_active;
		off += s;
	}
}

/*
** Note: this ends up being slower than other methods, because disk requests are
** serialized anyway.
*/
static void
async_copy_loop(
	int in,
	int out,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size
)
{
	auto r = full_read(in, buf1, buf_size, 0).get();
	if (size_t(r) < buf_size) {
		auto s = full_write(out, buf1, r, 0).get();
		assert(r == s);
		return;
	}

	std::atomic<int> cv1(buf_size);
	std::atomic<int> cv2{-1};
	auto off = off_t(buf_size);
	auto buf1_active = false;
	auto t = std::thread(write_worker, out, buf1, buf2, buf_size,
		std::ref(cv1), std::ref(cv2));

	for (;;) {
		if (buf1_active) {
			while (cv1.load(std::memory_order_acquire) != -1) {}
			r = full_read(in, buf1, buf_size, off).get();
			cv1.store(r, std::memory_order_release);
			if (size_t(r) < buf_size) { goto exit; }
		}
		else {
			while (cv2.load(std::memory_order_acquire) != -1) {}
			r = full_read(in, buf2, buf_size, off).get();
			cv2.store(r, std::memory_order_release);
			if (size_t(r) < buf_size) { goto exit; }
		}
		buf1_active = !buf1_active;
		off += buf_size;
	}
exit:
	t.join();
}

static auto
copy_plain(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	copy_loop(in, out, buf.get(), buf_size);
	::close(in);
	::close(out);
}

static auto
copy_nocache(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw current_system_error();
	}
	if (::fcntl(in, F_NOCACHE, 1) == -1) {
		throw current_system_error();
	}
	if (::fcntl(out, F_NOCACHE, 1) == -1) {
		throw current_system_error();
	}

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
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw current_system_error();
	}
	if (::fcntl(in, F_RDAHEAD, 1) == -1) {
		throw std::system_error{errno, std::system_category()};
	}

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
	auto buf = (uint8_t*)nullptr;

	auto r = ::posix_memalign((void**)&buf, 4096, buf_size);
	if (r != 0) {
		throw current_system_error();
	}

	struct radvisory rd;
	rd.ra_offset = 0;
	rd.ra_count = fs;
	if (::fcntl(in, F_RDADVISE, &rd) == -1) {
		throw current_system_error();
	}

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
copy_async(const char* src, const char* dst, size_t buf_size)
{
	auto in = safe_open(src, O_RDONLY).get();
	auto out = safe_open(dst, O_RDWR | O_CREAT | O_TRUNC).get();
	auto buf1 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto buf2 = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	async_copy_loop(in, out, buf1.get(), buf2.get(), buf_size);
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
	if (::fcntl(in, F_NOCACHE, 1) == -1) {
		throw current_system_error();
	}

	preallocate(out, fs);
	if (::ftruncate(out, fs) == -1) {
		throw current_system_error();
	}

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
