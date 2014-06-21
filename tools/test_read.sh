#! /usr/bin/env bash

#sizes=(8 16 24 32 40 48 56 64 80 96 112 128 160 192 224 256 320 384 448 512 640 768 896 1024)
sizes=(8 16 32 64 80 96 112 256) #512 1024)

set -x
for s in ${sizes[@]}; do
	./out/read_benchmark.run data/test_$s.bin | tee results/read_$s.csv
done
cat results/read_*.csv > results/read_results.csv
