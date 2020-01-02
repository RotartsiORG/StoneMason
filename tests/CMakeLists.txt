cmake_minimum_required(VERSION 3.15.3)

project(stms_tests LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Unit tests for StoneMason")

add_subdirectory(../dep/googletest dep/googletest)

add_executable(stms_tests main.cpp)

target_link_libraries(stms_tests PUBLIC stms_static gtest_main)
target_include_directories(stms_tests PUBLIC . ../include ../dep/googletest/googletest/include)
