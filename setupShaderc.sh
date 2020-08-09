#!/bin/bash

STARTING_DIR=$(pwd)
cd dep/shaderc

./utils/git-sync-deps
./utils/add_copyright.py
cd third_party/glslang

git clone https://github.com/google/googletest.git External/googletest
./update_glslang_sources.py

cd $STARTING_DIR
