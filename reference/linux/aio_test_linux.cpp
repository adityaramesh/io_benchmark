/*
** File Name:	aio_test_1.cpp
** Author:	Aditya Ramesh
** Date:	07/08/2013
** Contact:	_@adityaramesh.com
*/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <memory>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
// For `__NR_*` system call definitions.
#include <sys/syscall.h>
#include <linux/aio_abi.h>

static int
io_setup(unsigned n, aio_context_t* c)
{
	return syscall(__NR_io_setup, n, c);
}

static int
io_destroy(aio_context_t c)
{
	return syscall(__NR_io_destroy, c);
}

static int
io_submit(aio_context_t c, long n, iocb** b)
{
	return syscall(__NR_io_submit, c, n, b);
}

static int
io_getevents(aio_context_t c, long min, long max, io_event* e, timespec* t)
{
	return syscall(__NR_io_getevents, c, min, max, e, t);
}

int main()
{
	using namespace std::chrono;
	static constexpr std::size_t n = 16384;
	
	// Initialize the file descriptor. If O_DIRECT is not used, the kernel
	// will block on `io_submit` until the job finishes, because non-direct
	// IO via the `aio` interface is not implemented.
	auto fd = open("dat/test.dat", O_RDONLY | O_DIRECT | O_NOATIME);
	if (fd < 0) {
		std::cerr << "open error" << std::endl;
		return EXIT_FAILURE;
	}

	char* p;
	auto r = ::posix_memalign((void**)&p, 512, n);
	if (r != 0) {
		std::cerr << "posix_memalign failed" << std::endl;
		return EXIT_FAILURE;
	}
	auto del = [](char* p) { std::free(p); };
	std::unique_ptr<char[], decltype(del)> buf{p, del};

	// Initialize the IO context.
	aio_context_t c{0};
	r = io_setup(128, &c);
	if (r < 0) {
		std::cerr << "io_setup error" << std::endl;
		return EXIT_FAILURE;
	}

	// Setup I/O control block.
	iocb b;
	std::memset(&b, 0, sizeof(b));
	b.aio_fildes = fd;
	b.aio_lio_opcode = IOCB_CMD_PREAD;

	// Command-specific options for `pread`.
	b.aio_buf = (uint64_t)buf.get();
	b.aio_offset = 0;
	b.aio_nbytes = n;
	iocb* bs[1] = {&b};

	auto t1 = high_resolution_clock::now();
	r = io_submit(c, 1, bs);
	if (r != 1) {
		if (r < 0) {
			std::cerr << "io_submit error" << std::endl;
		}
		else {
			std::cerr << "could not submit event" << std::endl;
		}
		return EXIT_FAILURE;
	}
	auto t2 = high_resolution_clock::now();
	auto count = duration_cast<duration<double>>(t2 - t1).count();
	std::cout << "Took " << count << " seconds to submit job." << std::endl;

	io_event e[1];
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 0;

	t1 = high_resolution_clock::now();
	do {
		r = io_getevents(c, 1, 1, e, &t);
		std::cout << "Reading...\n";
	}
	while (r == 0);
	t2 = high_resolution_clock::now();
	count = duration_cast<duration<double>>(t2 - t1).count();

	std::cout << "Waited for " << count << " seconds." << std::endl;
	std::cout << "Return code: " << r << "." << std::endl;
	//std::cout << buf.get() << std::endl;

	r = io_destroy(c);
	if (r < 0) {
		std::cerr << "io_destroy error" << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
