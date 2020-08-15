cmake_minimum_required(VERSION 3.13.5)

SET(CMAKE_CXX_STANDARD 17)

project(stms_tests LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Unit tests for StoneMason")

option(STMS_SSL_TESTS_ENABLED "Enable SSL tests, should be disabled for travis" OFF)
if (STMS_SSL_TESTS_ENABLED)
    add_compile_definitions(STMS_SSL_TESTS_ENABLED)
endif()

add_subdirectory(../dep/googletest dep/googletest)

add_executable(stms_tests main.cpp)

target_compile_options(stms_tests PUBLIC ${STMS_COMPILE_FLAGS})

target_link_libraries(stms_tests PUBLIC stms_static gtest_main)
target_include_directories(stms_tests PUBLIC . ../include ../dep/googletest/googletest/include)
