/*
** File Name:	read_common.hpp
** Author:	Aditya Ramesh
** Date:	06/16/2014
** Contact:	_@adityaramesh.com
*/

#ifndef Z7FE6A1BA_51C8_492E_97C4_983095F7E88E
#define Z7FE6A1BA_51C8_492E_97C4_983095F7E88E

#include <array>
#include <atomic>
#include <thread>
#include <io_common.hpp>
#include <configuration.hpp>

static auto
read_loop(int fd, uint8_t* buf, size_t buf_size)
{
	auto off = off_t{0};
	auto count = off_t{0};

	for (;;) {
		auto n = (size_t)full_read(fd, buf, buf_size, off).get();
		count += std::count_if(buf, buf + n,
			[](auto x) { return x == needle; });
		if (n < buf_size) { break; }
		off += n;
	}
	return count;
}

static auto
aio_read_loop(int fd, uint8_t* buf1, uint8_t* buf2, size_t buf_size)
{
	using aiocb = struct aiocb;
	auto cb = aiocb{};
	cb.aio_fildes = fd;
	cb.aio_nbytes = buf_size;

	auto l = std::array<aiocb*, 1>{{&cb}};
	auto off = off_t{0};
	auto count = off_t{0};

	auto buf1_active = true;
	auto n = full_read(fd, buf1, buf_size, off).get();
	if (size_t(n) < buf_size) {
		return (off_t)std::count_if(buf1, buf1 + n,
			[](auto x) { return x == needle; });
	}
	off += buf_size;

	for (;;) {
		if (buf1_active) {
			cb.aio_buf = buf2;
			cb.aio_offset = off;
			if (::aio_read(&cb) == -1) { throw current_system_error(); }

			count += std::count_if(buf1, buf1 + buf_size,
				[](auto x) { return x == needle; });
		}
		else {
			cb.aio_buf = buf1;
			cb.aio_offset = off;
			if (::aio_read(&cb) == -1) { throw current_system_error(); }

			count += std::count_if(buf2, buf2 + buf_size,
				[](auto x) { return x == needle; });
		}

		buf1_active = !buf1_active;
		if (::aio_suspend(l.data(), 1, nullptr) == -1) { throw current_system_error(); }
		if (::aio_error(&cb) == -1) { throw current_system_error(); }
		n = ::aio_return(&cb);
		if (n == -1) { throw current_system_error(); }

		if (size_t(n) < buf_size) {
			if (buf1_active) {
				count += std::count_if(buf1, buf1 + n,
					[](auto x) { return x == needle; });
			}
			else {
				count += std::count_if(buf2, buf2 + n,
					[](auto x) { return x == needle; });
			}
			return count;
		}
		off += buf_size;
	}
}

static void
read_worker(
	int fd,
	uint8_t* buf1,
	uint8_t* buf2,
	size_t buf_size,
	std::atomic<int>& cv1,
	std::atomic<int>& cv2
)
{
	auto r = int{};
	auto off = off_t(buf_size);
	auto buf1_active = true;

	for (;;) {
		if (buf1_active) {
			while (cv2.load(std::memory_order_acquire) != -1) {}
			r = full_read(fd, buf2, buf_size, off).get();
			cv2.store(r, std::memory_order_release);
		}
		else {
			while (cv1.load(std::memory_order_acquire) != -1) {}
			r = full_read(fd, buf1, buf_size, off).get();
			cv1.store(r, std::memory_order_release);
		}
		if (size_t(r) < buf_size) { return; }
		buf1_active = !buf1_active;
		off += buf_size;
	}
}

static auto
async_read_loop(int fd, uint8_t* buf1, uint8_t* buf2, size_t buf_size)
{
	auto r = full_read(fd, buf1, buf_size, 0).get();
	if (size_t(r) < buf_size) {
		return (off_t)std::count_if(buf1, buf1 + r,
			[](auto x) { return x == needle; });
	}

	std::atomic<int> cv1(buf_size);
	std::atomic<int> cv2{-1};
	auto count = off_t{0};
	auto buf1_active = true;
	auto t = std::thread(read_worker, fd, buf1, buf2, buf_size,
		std::ref(cv1), std::ref(cv2));

	for (;;) {
		if (buf1_active) {
			while (cv1.load(std::memory_order_acquire) == -1) {}
			r = cv1.load(std::memory_order_relaxed);
			count += std::count_if(buf1, buf1 + r,
				[](auto x) { return x == needle; });
			if (size_t(r) < buf_size) { goto exit; }
			cv1.store(-1, std::memory_order_release);
		}
		else {
			while (cv2.load(std::memory_order_acquire) == -1) {}
			r = cv2.load(std::memory_order_relaxed);
			count += std::count_if(buf2, buf2 + r,
				[](auto x) { return x == needle; });
			if (size_t(r) < buf_size) { goto exit; }
			cv2.store(-1, std::memory_order_release);
		}
		buf1_active = !buf1_active;
	}
exit:
	t.join();
	return count;
}

static auto
check(const char* path)
{
	static constexpr auto buf_size = 65536;
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
read_plain(const char* path, size_t buf_size)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[buf_size]);
	auto count = read_loop(fd, buf.get(), buf_size);
	::close(fd);
	return count;
}

static auto
mmap_plain(const char* path)
{
	auto fd = safe_open(path, O_RDONLY).get();
	auto fs = file_size(fd).get();
	auto p = (uint8_t*)::mmap(nullptr, fs, PROT_READ, MAP_PRIVATE, fd, 0);
	auto count = std::count_if(p, p + fs, [](auto x) { return x == needle; });
	::munmap(p, fs);
	return count;
}

#endif
