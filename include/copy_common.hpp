/*
** File Name:	copy_common.hpp
** Author:	Aditya Ramesh
** Date:	06/16/2014
** Contact:	_@adityaramesh.com
*/

#ifndef ZD867F4DE_A8CC_4B2A_807C_F44862C521A4
#define ZD867F4DE_A8CC_4B2A_807C_F44862C521A4

#include <atomic>
#include <thread>
#include <io_common.hpp>
#include <configuration.hpp>

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
copy_worker(
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
	auto t = std::thread(copy_worker, out, buf1, buf2, buf_size,
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

#endif
