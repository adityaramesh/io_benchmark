#! /usr/bin/env bash

./out/overwrite_benchmark.run 33554432 > results/overwrite_32.dat
./out/overwrite_benchmark.run 67108864 > results/overwrite_64.dat
./out/overwrite_benchmark.run 134217728 > results/overwrite_128.dat
./out/overwrite_benchmark.run 268435456 > results/overwrite_256.dat
./out/overwrite_benchmark.run 536870912 > results/overwrite_512.dat
./out/overwrite_benchmark.run 1073741824 > results/overwrite_1024.dat
