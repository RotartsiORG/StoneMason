cmake_minimum_required(VERSION 3.15.3)

SET(CMAKE_CXX_STANDARD 20)

project(stms_samples LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")

#target_compile_options(stms_samples PUBLIC -O3 -Wall -g3 -ggdb -std=c++17)
# Cannot specify options since we dont have a call to add_executable yet.
