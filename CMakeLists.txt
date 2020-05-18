cmake_minimum_required(VERSION 3.15.3)

SET(CMAKE_CXX_STANDARD 20)

set(STMS_VERSION 0.0.1)
set(STMS_DESCRIPTION "An attempt at a C++ game engine")

project(stms_shared LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION ${STMS_DESCRIPTION})
project(stms_static LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION ${STMS_DESCRIPTION})

find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(OpenSSL REQUIRED)

set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/glfw)

#add_subdirectory(dep/bullet3)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/spdlog)

#add_subdirectory(dep/fmt)

add_library(stms_shared SHARED src/stms/async.cpp include/stms/async.hpp
        include/stms/audio.hpp src/stms/audio.cpp include/stms/logging.hpp src/stms/logging.cpp include/stms/config.hpp
        include/stms/util.hpp src/stms/util.cpp src/stms/net/dtls_server.cpp include/stms/net/dtls_server.hpp include/stms/net/ssl.hpp
        src/stms/net/ssl.cpp src/stms/timers.cpp include/stms/timers.hpp src/stms/net/dtls_client.cpp include/stms/net/dtls_client.hpp)

add_library(stms_static STATIC src/stms/async.cpp include/stms/async.hpp
        include/stms/audio.hpp src/stms/audio.cpp include/stms/logging.hpp src/stms/logging.cpp include/stms/config.hpp
        include/stms/util.hpp src/stms/util.cpp src/stms/net/dtls_server.cpp include/stms/net/dtls_server.hpp include/stms/net/ssl.hpp
        src/stms/net/ssl.cpp src/stms/timers.cpp include/stms/timers.hpp src/stms/net/dtls_client.cpp include/stms/net/dtls_client.hpp)

target_compile_options(stms_shared PUBLIC -Wall -Wextra -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wno-unused-parameter -O3 -g3 -ggdb -std=c++17)
target_compile_options(stms_static PUBLIC -Wall -Wextra -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wno-unused-parameter -O3 -g3 -ggdb -std=c++17)

target_include_directories(stms_shared PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_include_directories(stms_static PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

# Do not include spdlog in shared lib, linknig will fail
target_link_libraries(stms_shared PUBLIC glfw ${OPENAL_LIBRARY} ${OPENSSL_LIBRARIES})
target_link_libraries(stms_static PUBLIC glfw spdlog ${OPENAL_LIBRARY} ${OPENSSL_LIBRARIES})

target_include_directories(stms_shared PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(stms_static PUBLIC ${Vulkan_INCLUDE_DIRS})

target_link_libraries(stms_shared PUBLIC ${Vulkan_LIBRARIES})
target_link_libraries(stms_static PUBLIC ${Vulkan_LIBRARIES})

option(STMS_BUILD_TESTS "Builds the tests of StoneMason (off by default)" ON)
option(STMS_GENERATE_DOCS "Generates the documentation for StoneMason using doxygen (off by default)" ON)
option(STMS_BUILD_SAMPLES "Builds example programs for StoneMason (off by default)" ON)

# TODO: Remove later. Here bc otherwise clion won't properly see our CMake project.
set(STMS_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(STMS_GENERATE_DOCS ON CACHE BOOL "" FORCE)
set(STMS_BUILD_SAMPLES ON CACHE BOOL "" FORCE)

if (STMS_BUILD_TESTS)
    message("STMS tests enabled")
    add_subdirectory(tests)
endif (STMS_BUILD_TESTS)

if (STMS_BUILD_SAMPLES)
    message("STMS samples enabled")
    add_subdirectory(samples)
endif (STMS_BUILD_SAMPLES)

if (STMS_GENERATE_DOCS)
    message("STMS docs enabled")
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
