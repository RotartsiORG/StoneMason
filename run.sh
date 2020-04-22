CMK_EXE="/snap/bin/cmake"

mkdir build
$CMK_EXE -Wdev -Wdeprecated -DSTMS_GENERATE_DOCS=ON -DSTMS_BUILD_TESTS=ON -DSTMS_BUILD_SAMPLES=ON -Bbuild -S.

cd build || exit
$CMK_EXE --build . -j 8

cd ..

./build/tests/stms_tests
