/*
** File Name:	aio_test_mac.cpp
** Author:	Aditya Ramesh
** Date:	07/08/2013
** Contact:	_@adityaramesh.com
*/

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <aio.h>
#include <fcntl.h>
#include <time.h>

int main()
{
	using namespace std::chrono;
	static constexpr std::size_t n = 81920;

	auto t1 = high_resolution_clock::now();
	auto fd = open("dat/test.dat", O_RDONLY);
	if (fd < 0) {
		std::cerr << "open error" << std::endl;
		return EXIT_FAILURE;
	}
	if (::fcntl(fd, F_NOCACHE, 1) == -1) {
		std::cerr << "fcntl error" << std::endl;
		return EXIT_FAILURE;
	}
	auto t2 = high_resolution_clock::now();
	auto c1 = duration_cast<duration<double>>(t2 - t1).count();
	std::cout << "Time getting fd: " << c1 << std::endl;

	//char* p;
	//auto r = ::posix_memalign((void**)&p, 4096, n);
	//if (r != 0) {
	//	std::cerr << "posix_memalign failed" << std::endl;
	//	return EXIT_FAILURE;
	//}
	//auto del = [](char* p) { std::free(p); };
	//std::unique_ptr<char[], decltype(del)> buf{p, del};
	t1 = high_resolution_clock::now();
	std::unique_ptr<char[]> buf{new char[n]};
	t2 = high_resolution_clock::now();
	auto c2 = duration_cast<duration<double>>(t2 - t1).count();
	std::cout << "Time allocating buffer: " << c2 << std::endl;

	aiocb b;
	std::memset(&b, 0, sizeof(b));
	b.aio_fildes = fd;
	b.aio_sigevent.sigev_notify = SIGEV_NONE;

	b.aio_buf = buf.get();
	b.aio_offset = 0;
	b.aio_nbytes = n;

	t1 = high_resolution_clock::now();
	if (aio_read(&b) != 0) {
		std::cerr << "read error" << std::endl;
		return EXIT_FAILURE;
	}
	t2 = high_resolution_clock::now();
	auto c3 = duration_cast<duration<double>>(t2 - t1).count();

	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 0;

	const aiocb* l[] = {&b};

	t1 = high_resolution_clock::now();
	while (aio_suspend(l, 1, &t) != 0) {
		std::cout << "Reading..." << std::endl;
	}
	t2 = high_resolution_clock::now();
	auto c4 = duration_cast<duration<double>>(t2 - t1).count();

	std::cout << "Time to submit: " << c3 << std::endl;
	std::cout << "Time spend waiting: " << c4 << std::endl;

	auto r = aio_return(&b);
	if (r == -1) {
		std::cerr << "error reading" << std::endl;
		return EXIT_FAILURE;
	}
	else {
		std::cout << r << " bytes read" << std::endl;
		return EXIT_SUCCESS;
	}
}
