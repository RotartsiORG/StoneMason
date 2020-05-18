CMK_EXE="/snap/bin/cmake"

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

mkdir build
$CMK_EXE -Wdev -Wdeprecated -DSTMS_GENERATE_DOCS=ON -DSTMS_BUILD_TESTS=ON -DSTMS_BUILD_SAMPLES=ON -Bbuild -S.

cd build || exit
$CMK_EXE --build . -j 8

cd ..

./build/tests/stms_tests
