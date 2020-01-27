CMK_EXE="/snap/bin/cmake"

mkdir build
$CMK_EXE -DSTMS_GENERATE_DOCS=ON -DSTMS_BUILD_TESTS=ON -DSTMS_BUILD_SAMPLES=ON -Bbuild -S.

cd build
$CMK_EXE --build .

cd ..

./build/tests/stms_tests
