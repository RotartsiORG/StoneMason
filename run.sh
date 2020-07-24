#!/bin/bash

CMK_EXE="cmake"

#export CC=/usr/bin/clang
#export CXX=/usr/bin/clang++

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

mkdir build
$CMK_EXE -Wdev -Wdeprecated -DSTMS_BUILD_TESTS=ON -DSTMS_BUILD_SAMPLES=ON -Bbuild -S.

cd build || exit
$CMK_EXE --build . -j 8

cd .. || exit

doxygen -d Preprocessor

./build/tests/stms_tests
