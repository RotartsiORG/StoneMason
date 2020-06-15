cmake_minimum_required(VERSION 3.13.5)

SET(CMAKE_CXX_STANDARD 17)

set(STMS_VERSION 0.0.1)
set(STMS_COMPILE_FLAGS -Wshadow -Wall -Wextra -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wno-unused-parameter -O3 -g3 -ggdb -std=c++17)
set(STMS_DESCRIPTION "An attempt at a C++ game engine")

project(stms_shared LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION ${STMS_DESCRIPTION})
project(stms_static LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION ${STMS_DESCRIPTION})

set(GLEW_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

if (APPLE)
    # This is a bug in CMake that causes it to prefer the system version over
    # the one in the specified ROOT folder.
    set(OPENSSL_ROOT_DIR /usr/local/Cellar/openssl@1.1/1.1.1g)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib CACHE FILEPATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_ROOT_DIR}/lib/libssl.dylib CACHE FILEPATH "" FORCE)
endif()

find_package(GLEW REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)

set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/glfw)

#add_subdirectory(dep/bullet3)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/spdlog)

#add_subdirectory(dep/fmt)

file(GLOB_RECURSE STMS_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE STMS_INCLUDE ${PROJECT_SOURCE_DIR}/include/*.hpp)

message("-- Found STMS Sources ${STMS_SOURCES}")
message("-- STMS project directory is ${PROJECT_SOURCE_DIR}")

add_library(stms_shared SHARED ${STMS_SOURCES} ${STMS_INCLUDE})
add_library(stms_static STATIC ${STMS_SOURCES} ${STMS_INCLUDE})

target_compile_options(stms_shared PUBLIC ${STMS_COMPILE_FLAGS})
target_compile_options(stms_static PUBLIC ${STMS_COMPILE_FLAGS})

target_include_directories(stms_shared PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})
target_include_directories(stms_static PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})

# Do not include spdlog in shared lib, linknig will fail
target_link_libraries(stms_shared PUBLIC glfw ${OPENAL_LIBRARY} OpenSSL::SSL OpenSSL::Crypto CURL::libcurl)
target_link_libraries(stms_static PUBLIC glfw spdlog ${OPENAL_LIBRARY} OpenSSL::SSL OpenSSL::Crypto CURL::libcurl)

target_include_directories(stms_shared PUBLIC ${Vulkan_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})
target_include_directories(stms_static PUBLIC ${Vulkan_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})

target_link_libraries(stms_shared PUBLIC Vulkan::Vulkan OpenGL::GL GLEW::glew_s)
target_link_libraries(stms_static PUBLIC Vulkan::Vulkan OpenGL::GL GLEW::glew_s)

option(STMS_BUILD_TESTS "Builds the tests of StoneMason (off by default)" ON)
option(STMS_GENERATE_DOCS "Generates the documentation for StoneMason using doxygen (off by default)" ON)
option(STMS_BUILD_SAMPLES "Builds example programs for StoneMason (off by default)" ON)

# TODO: Remove later. Here bc otherwise clion won't properly see our CMake project.
set(STMS_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(STMS_GENERATE_DOCS ON CACHE BOOL "" FORCE)
set(STMS_BUILD_SAMPLES ON CACHE BOOL "" FORCE)

if (STMS_BUILD_TESTS)
    message("-- STMS tests enabled")
    add_subdirectory(tests)
endif (STMS_BUILD_TESTS)

if (STMS_BUILD_SAMPLES)
    message("-- STMS samples enabled")
    add_subdirectory(samples)
endif (STMS_BUILD_SAMPLES)

if (STMS_GENERATE_DOCS)
    message("-- STMS docs enabled")
    find_package(Doxygen REQUIRED)
    # Use a dummy call to project() to populate variables.
    project(StoneMason LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION ${STMS_DESCRIPTION})
    configure_file("Doxyfile.in" "../Doxyfile" @ONLY)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_GENERATE_MAN NO)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_HTML_OUTPUT "docs")
        set(DOXYGEN_OUTPUT_DIR ${PROJECT_SOURCE_DIR})

        doxygen_add_docs(docs ${PROJECT_SOURCE_DIR}/include ALL COMMENT "Generate documentation with doxygen")
    else (DOXYGEN_FOUND)
        message("Doxygen could not be found! Skipping docs!")
    endif (DOXYGEN_FOUND)
endif (STMS_GENERATE_DOCS)
