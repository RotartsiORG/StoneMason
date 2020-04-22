cmake_minimum_required(VERSION 3.15.3)

SET(CMAKE_CXX_STANDARD 17)

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

add_library(stms_shared SHARED src/stms/async.cpp include/stms/async.hpp include/stms/audio.hpp src/stms/audio.cpp include/stms/logging.hpp src/stms/logging.cpp include/stms/config.hpp include/stms/util.hpp src/stms/util.cpp)
add_library(stms_static STATIC src/stms/async.cpp include/stms/async.hpp include/stms/audio.hpp src/stms/audio.cpp include/stms/logging.hpp src/stms/logging.cpp include/stms/config.hpp include/stms/util.hpp src/stms/util.cpp)

target_compile_options(stms_shared PUBLIC -Wall -Wextra -O3 -g3 -ggdb -std=c++17)  # -O3
target_compile_options(stms_static PUBLIC -Wall -Wextra -O3 -g3 -ggdb -std=c++17) # -O3

target_include_directories(stms_shared PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_include_directories(stms_static PUBLIC include src dep dep/glfw/include dep/spdlog/include dep/glm ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

message("OpenSSL ${OPENSSL_LIBRARIES}")
target_link_libraries(stms_shared PUBLIC glfw ${OPENAL_LIBRARY} ${OPENSSL_LIBRARIES})
target_link_libraries(stms_static PUBLIC glfw spdlog ${OPENAL_LIBRARY} ${OPENSSL_LIBRARIES})

if (Vulkan_FOUND)
    target_include_directories(stms_shared PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_include_directories(stms_static PUBLIC ${Vulkan_INCLUDE_DIRS})

    target_link_libraries(stms_shared PUBLIC ${Vulkan_LIBRARIES})
    target_link_libraries(stms_static PUBLIC ${Vulkan_LIBRARIES})
else (Vulkan_FOUND)
    # TODO: Remove this code; They are required to make CLion stop shouting at me.
    # Last ditch effort to find vulkan if for some reason FindVulkan fails (*ahem CLION*)
    # Should work on Linux/Mac, but WILL NOT WORK on Windows! Tweak to point to correct paths on your machine.

    target_include_directories(stms_shared PUBLIC $ENV{VULKAN_SDK}/include)
    target_include_directories(stms_static PUBLIC $ENV{VULKAN_SDK}/include)

    target_link_libraries(stms_shared PUBLIC $ENV{VULKAN_SDK}/lib/libvulkan.so)
    target_link_libraries(stms_static PUBLIC $ENV{VULKAN_SDK}/lib/libvulkan.so)
endif (Vulkan_FOUND)

option(STMS_BUILD_TESTS "Builds the tests of StoneMason (off by default)" OFF)
option(STMS_GENERATE_DOCS "Generates the documentation for StoneMason using doxygen (off by default)" OFF)
option(STMS_BUILD_SAMPLES "Builds example programs for StoneMason (off by default)" OFF)

# TODO: Remove the following two lines; They make CLion happy on my machine.
target_include_directories(stms_static PUBLIC dep/googletest/googletest/include)
target_include_directories(stms_shared PUBLIC dep/googletest/googletest/include)

if (STMS_BUILD_TESTS)
    add_subdirectory(tests)
endif (STMS_BUILD_TESTS)

if (STMS_BUILD_SAMPLES)
    add_subdirectory(samples)
endif (STMS_BUILD_SAMPLES)

if (STMS_GENERATE_DOCS)
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
