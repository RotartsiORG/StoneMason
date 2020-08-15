#!/bin/bash
doxygen -d Preprocessor

CMK_EXE="cmake"

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

#export CC=/usr/bin/gcc
#export CXX=/usr/bin/g++

$CMK_EXE -GNinja -Wdev -Wdeprecated -DSTMS_SSL_TESTS_ENABLED=ON -Bbuild -S.

cd build || exit
$CMK_EXE --build . -j 8

cd .. || exit

./build/tests/stms_tests
