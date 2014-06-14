#! /usr/bin/env bash

sizes=(8 16 24 32 40 48 56 64 80 96 112 128 160 192 224 256 320 384 448 512 640 768 896 1024)

set -x
for s in ${sizes[@]}; do
	./out/write_benchmark.run $[s * 2**20] | tee results/write_$s.csv
done
