#! /usr/bin/env ruby

def make_test_files()
	block_size = 64 * 2**10
	[8, 16, 32, 64, 128, 256, 512, 1024].each do |i|
		file_size = i * 2**20
		count = file_size / block_size
		puts "Creating #{i} MB test file."
		`dd if=/dev/urandom of=data/test_#{i}.bin bs=#{block_size} count=#{count}`
	end
end

make_test_files()
