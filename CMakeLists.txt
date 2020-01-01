cmake_minimum_required(VERSION 3.15.3)

project(StoneMasonShared LANGUAGES CXX VERSION 0.0.1 DESCRIPTION "An attempt at a C++ game engine")
project(StoneMasonStatic LANGUAGES CXX VERSION 0.0.1 DESCRIPTION "An attempt at a C++ game engine")

find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)

set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/glfw)

add_library(StoneMasonShared SHARED src/stms/async.cpp include/stms/async.hpp)
add_library(StoneMasonStatic STATIC src/stms/async.cpp include/stms/async.hpp)

target_include_directories(StoneMasonShared PUBLIC include src dep dep/glfw/include ${Vulkan_INCLUDE_DIRS} ${OPENAL_INCLUDE_DIR})
target_include_directories(StoneMasonStatic PUBLIC include src dep dep/glfw/include ${Vulkan_INCLUDE_DIRS} ${OPENAL_INCLUDE_DIR})

target_link_libraries(StoneMasonShared PUBLIC glfw ${Vulkan_LIBRARIES} ${OPENAL_LIBRARY})
target_link_libraries(StoneMasonStatic PUBLIC glfw ${Vulkan_LIBRARIES} ${OPENAL_LIBRARY})
