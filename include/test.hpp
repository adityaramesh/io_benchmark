/*
** File Name:	test.hpp
** Author:	Aditya Ramesh
** Date:	06/12/2014
** Contact:	_@adityaramesh.com
*/

#ifndef ZD477B76C_977A_4861_963D_3C448BD98F34
#define ZD477B76C_977A_4861_963D_3C448BD98F34

#include <cstdio>
#include <chrono>
#include <io_common.hpp>

/*
** The `*_read` and `*_write` functions have not been abstracted into one
** function, because clang was emitting bad code or getting ICE's when nested
** lambdas were used in certain situations.
*/

template <class Function>
static void test_read(const Function& func, const char* name, unsigned count)
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using milliseconds = duration<double, std::ratio<1, 1000>>;

	auto mean = double{0};
	auto sample = std::array<double, num_trials>{};

	for (auto i = 0; i != num_trials; ++i) {
		auto t1 = high_resolution_clock::now();
		if (func() != count) { throw std::runtime_error{"Mismatching count."}; }
		auto t2 = high_resolution_clock::now();
		auto ms = duration_cast<milliseconds>(t2 - t1).count();
		sample[i] = ms;
		mean += ms;
		purge_cache().get();
	}

	mean /= num_trials;
	auto stddev = std::sqrt(1.0 / num_trials * boost::accumulate(
		sample, 0.0, [&](auto x, auto y) {
			return x + std::pow(y - mean, 2);
		}));

	std::printf("%s, %f, %f\n", name, mean, stddev);
	std::fflush(stdout);
}

template <class Function, class Range>
static void test_read_range(
	const Function& func,
	const char* path,
	const char* name,
	const Range& range,
	off_t file_size,
	unsigned count
)
{
	static constexpr auto kb = 1024;
	auto buf = std::array<char, 64>{};

	for (const auto& bs : range) {
		if (bs * kb <= file_size) {
			std::snprintf(buf.data(), 64, "%s %d Kb", name, bs);
			test_read(std::bind(func, path, bs * kb), buf.data(), count);
		}
	}
}

template <class Function>
static void test_write(const Function& func, const char* name)
{
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using milliseconds = duration<double, std::ratio<1, 1000>>;

	auto mean = double{0};
	auto sample = std::array<double, num_trials>{};

	for (auto i = 0; i != num_trials; ++i) {
		auto t1 = high_resolution_clock::now();
		func();
		auto t2 = high_resolution_clock::now();
		auto ms = duration_cast<milliseconds>(t2 - t1).count();
		sample[i] = ms;
		mean += ms;
	}

	mean /= num_trials;
	auto stddev = std::sqrt(1.0 / num_trials * boost::accumulate(
		sample, 0.0, [&](auto x, auto y) {
			return x + std::pow(y - mean, 2);
		}));

	std::printf("%s, %f, %f\n", name, mean, stddev);
	std::fflush(stdout);
}

template <class Function, class Range>
static void test_write_range(
	const Function& func,
	const char* path,
	const char* name,
	const Range& range,
	off_t count
)
{
	static constexpr auto kb = 1024;
	auto buf = std::array<char, 64>{};

	for (const auto& bs : range) {
		if (bs * kb <= count) {
			std::snprintf(buf.data(), 64, "%s %d Kb", name, bs);
			test_write(std::bind(func, path, bs * kb), buf.data());
		}
	}
}

#endif
