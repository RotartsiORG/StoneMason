CMK_EXE="/snap/bin/cmake"

mkdir build
$CMK_EXE -Bbuild -S.

cd build
make

./tests/stms_tests
