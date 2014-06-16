/*
** File Name:	write_common.hpp
** Author:	Aditya Ramesh
** Date:	06/16/2014
** Contact:	_@adityaramesh.com
*/

#ifndef ZFD327A4C_B13C_4813_9572_DDC0936536FE
#define ZFD327A4C_B13C_4813_9572_DDC0936536FE

#include <io_common.hpp>
#include <configuration.hpp>

static void
fill_buffer(uint8_t* p, size_t count)
{
	auto gen = std::mt19937{std::random_device{}()};
	auto dist = std::uniform_int_distribution<uint8_t>(0, 255);
	std::generate(p, p + count, [&]() { return dist(gen); });
}

static auto
write_loop(int fd, uint8_t* buf, size_t buf_size, size_t count)
{
	for (auto off = off_t{0}; off < off_t(count); off += buf_size) {
		fill_buffer(buf, buf_size);
		auto r = full_write(fd, buf, buf_size, off);
		assert(r == buf_size);
	}
}

static void
write_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
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
			if (s == -2) { return; }
			r = full_write(fd, buf2, s, off).get();
			cv2.store(-1, std::memory_order_release);
		}
		else {
			while (cv1.load(std::memory_order_acquire) == -1) {}
			s = cv1.load(std::memory_order_relaxed);
			if (s == -2) { return; }
			r = full_write(fd, buf1, s, off).get();
			cv1.store(-1, std::memory_order_release);
		}
		assert(r == s);
		buf1_active = !buf1_active;
		off += s;
	}
}

static void
async_write_loop(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
	size_t count
)
{
	if (count <= buf_size) {
		fill_buffer(buf1, count);
		auto r = full_write(fd, buf1, count, 0).get();
		assert(r == count);
		return;
	}

	fill_buffer(buf1, buf_size);
	std::atomic<int> cv1(buf_size);
	std::atomic<int> cv2{-1};
	auto rem = count - buf_size;
	auto buf1_active = false;
	auto t = std::thread(write_worker, fd, buf1, buf2,
		std::ref(cv1), std::ref(cv2));

	for (;;) {
		if (buf1_active) {
			while (cv1.load(std::memory_order_acquire) != -1) {}
			if (rem > buf_size) {
				fill_buffer(buf1, buf_size);
				cv1.store(buf_size, std::memory_order_release);
			}
			else {
				fill_buffer(buf1, rem);
				cv1.store(-2, std::memory_order_release);
				goto exit;
			}
		}
		else {
			while (cv2.load(std::memory_order_acquire) != -1) {}
			if (rem > buf_size) {
				fill_buffer(buf2, buf_size);
				cv2.store(buf_size, std::memory_order_release);
			}
			else {
				fill_buffer(buf2, rem);
				cv2.store(-2, std::memory_order_release);
				goto exit;
			}
		}
		buf1_active = !buf1_active;
		rem -= buf_size;
	}
exit:
	t.join();
}

static void
write_plain(const char* path, size_t buf_size, size_t count)
{
	auto fd = safe_open(path, O_WRONLY | O_CREAT | O_TRUNC).get();
	auto buf = std::unique_ptr<uint8_t[]>{new uint8_t[buf_size]};
	write_loop(fd, buf.get(), buf_size, count);
	::close(fd);
}

#endif
