cmake_minimum_required(VERSION 3.15.3)

set(STMS_VERSION 0.0.1)

project(stms_shared LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "An attempt at a C++ game engine")
project(stms_static LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "An attempt at a C++ game engine")

find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)

set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/glfw)

add_library(stms_shared SHARED src/stms/async.cpp include/stms/async.hpp)
add_library(stms_static STATIC src/stms/async.cpp include/stms/async.hpp)

target_include_directories(stms_shared PUBLIC include src dep dep/glfw/include ${Vulkan_INCLUDE_DIRS} ${OPENAL_INCLUDE_DIR})
target_include_directories(stms_static PUBLIC include src dep dep/glfw/include ${Vulkan_INCLUDE_DIRS} ${OPENAL_INCLUDE_DIR})

target_link_libraries(stms_shared PUBLIC glfw ${Vulkan_LIBRARIES} ${OPENAL_LIBRARY})
target_link_libraries(stms_static PUBLIC glfw ${Vulkan_LIBRARIES} ${OPENAL_LIBRARY})

option(STMS_BUILD_TESTS "Builds the tests of stonemason (off by default)" ON)

#if (STMS_BUILD_TESTS)
add_subdirectory(tests)
#endif()
