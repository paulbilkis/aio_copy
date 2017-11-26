#!/bin/bash

num_of_aio=(1, 2, 4, 8, 12, 16)
num_of_buf_sizes=(512, 1024, 2048, 4096, 8192, 16384, 32768)

if [ ! -f "aio_copy" ]
then
    printf "\nMaking an executable\n"
    make clean
    make
fi

if [ ! -f "input" ]
then
    printf "\nInput is not found, creating one\n"
    touch input
fi

if [ ! -f "output" ]
then
    printf "\nOutput is not found, creating one\n"
    touch output
fi

echo "" > async_stats.txt

dd if=/dev/urandom of=input count=128 bs=1M

for b in "${num_of_buf_sizes[@]}";  do
    echo "" > output
    printf "\n\nTest for ${b} bufsize\n"
    ./aio_copy input output 1 $b
    printf "\nDiff result:"
    diff input output
    printf ""
done

for n in "${num_of_aio[@]}";  do
    echo "" > output
    printf "\n\nTest for ${n} aio operations\n"
    ./aio_copy input output $n $b
    printf "\nDiff result:"
    diff input output
    printf ""
done
