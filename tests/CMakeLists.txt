cmake_minimum_required(VERSION 3.13.5)

SET(CMAKE_CXX_STANDARD 17)

project(stms_tests LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Unit tests for StoneMason")

add_subdirectory(../dep/googletest dep/googletest)

add_executable(stms_tests main.cpp)

target_compile_options(stms_tests PUBLIC ${STMS_COMPILE_FLAGS})

target_link_libraries(stms_tests PUBLIC stms_static gtest_main)
target_include_directories(stms_tests PUBLIC . ../include ../dep/googletest/googletest/include)
