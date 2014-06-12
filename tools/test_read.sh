#! /usr/bin/env bash

set -x
./out/read_benchmark.run data/test_8.bin | tee results/read_8.txt
./out/read_benchmark.run data/test_16.bin | tee results/read_16.txt
./out/read_benchmark.run data/test_32.bin | tee results/read_32.txt
./out/read_benchmark.run data/test_64.bin | tee results/read_64.txt
./out/read_benchmark.run data/test_128.bin | tee results/read_128.txt
./out/read_benchmark.run data/test_256.bin | tee results/read_256.txt
# ./out/read_benchmark.run data/test_512.bin | tee results/read_512.txt
# ./out/read_benchmark.run data/test_1024.bin | tee results/read_1024.txt
