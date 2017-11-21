#!/bin/bash

num_of_aio=(1, 2, 4, 8, 12, 16)

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

dd if=/dev/urandom of=input count=1000 bs=4096

for n in "${num_of_aio[@]}";  do
    echo "" > output
    printf "\n\nTest for ${n}\n"
    ./aio_copy input output $n
    printf "\nDiff result:"
    diff input output
    printf ""
done
