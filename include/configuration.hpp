/*
** File Name:	configuration.hpp
** Author:	Aditya Ramesh
** Date:	06/12/2014
** Contact:	_@adityaramesh.com
*/

#ifndef Z86A588AA_6D5A_4CA1_B36E_3EE127F9AF1D
#define Z86A588AA_6D5A_4CA1_B36E_3EE127F9AF1D

// Number of times to run each method.
static constexpr auto num_trials = 10;
// Special byte value used to verify correctness for the read benchmark.
static constexpr auto needle = uint8_t{0xFF};

#endif
