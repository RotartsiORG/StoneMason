#!/bin/bash
doxygen -d Preprocessor

CMK_EXE="cmake"

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

#export CC=/usr/bin/gcc
#export CXX=/usr/bin/g++

$CMK_EXE -GNinja -Wdev -Wdeprecated -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DSTMS_SSL_TESTS_ENABLED=ON -Bbuild -S.

cd build || exit
$CMK_EXE --build . -j 8

#touch .inferconfig
#echo '{"infer-blacklist-path-regex":[".*\/dep\/.*"]}' > .inferconfig

infer run --report-blacklist-path-regex ".*\/dep\/.*" --compilation-database compile_commands.json

cd .. || exit

./build/tests/stms_tests


