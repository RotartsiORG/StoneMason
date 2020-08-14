cmake_minimum_required(VERSION 3.13.5)

SET(CMAKE_CXX_STANDARD 17)

set(STMS_VERSION 0.0.0)
set(STMS_COMPILE_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-function -std=c++17)
set(STMS_DESCRIPTION "An attempt at a C++ game engine")

add_compile_options(-fPIC -O2)
add_link_options(-fPIC -O2)

option(STMS_BUILD_TESTS "Builds the tests of StoneMason (off by default)" ON)
option(STMS_BUILD_SAMPLES "Builds example programs for StoneMason (off by default)" ON)
option(STMS_DEBUG_SYMBOLS "Builds STMS with debug symbols" ON)
option(STMS_ASAN "Builds STMS with address sanitizer" OFF)

if (STMS_ASAN)
    message("-- Compiling STMS with ASAN!")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

if (STMS_DEBUG_SYMBOLS)
    message("-- Compiling STMS with max debug symbols!")
    add_compile_options(-g -ggdb -g3)
endif()

option(STMS_ENABLE_OPENGL "Builds STMS with OpenGL support" ON)
option(STMS_ENABLE_VULKAN "Builds STMS with Vulkan Support" ON)

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

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenAL REQUIRED)

set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(dep/glfw)

add_subdirectory(dep/freetype2)

#add_subdirectory(dep/bullet3)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# only support importing of GLTF, COLLADA (DAE), and OBJ
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(dep/assimp)

add_subdirectory(dep/fmt)

file(GLOB_RECURSE STMS_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE STMS_INCLUDE ${PROJECT_SOURCE_DIR}/include/*.hpp)

message("-- Found STMS Sources ${STMS_SOURCES}")
message("-- STMS project directory is ${PROJECT_SOURCE_DIR}")

add_library(stms_shared SHARED ${STMS_SOURCES} ${STMS_INCLUDE})
add_library(stms_static STATIC ${STMS_SOURCES} ${STMS_INCLUDE})

target_compile_options(stms_shared PUBLIC ${STMS_COMPILE_FLAGS})
target_compile_options(stms_static PUBLIC ${STMS_COMPILE_FLAGS})

target_include_directories(stms_shared PUBLIC include src dep dep/glfw/include dep/fmt/include dep/glm dep/imgui
                            dep/assimp/include)
target_include_directories(stms_static PUBLIC include src dep dep/glfw/include dep/fmt/include dep/glm dep/imgui
                            dep/assimp/include)

target_include_directories(stms_shared PUBLIC dep/freetype2/include ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR}
                            ${CURL_INCLUDE_DIRS})
target_include_directories(stms_static PUBLIC dep/freetype2/include ${OPENAL_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR}
                            ${CURL_INCLUDE_DIRS})

target_link_libraries(stms_shared PUBLIC glfw fmt ${OPENAL_LIBRARY} OpenSSL::SSL OpenSSL::Crypto CURL::libcurl
                        assimp freetype)
target_link_libraries(stms_static PUBLIC glfw fmt ${OPENAL_LIBRARY} OpenSSL::SSL OpenSSL::Crypto CURL::libcurl
                        assimp freetype)


if (STMS_ENABLE_OPENGL)
    add_compile_definitions(stms_static STMS_ENABLE_OPENGL)
    add_compile_definitions(stms_shared STMS_ENABLE_OPENGL)

    message("-- OpenGL support enabled for STMS")

    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)

    target_include_directories(stms_shared PUBLIC ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})
    target_include_directories(stms_static PUBLIC ${OPENGL_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS})

    target_link_libraries(stms_shared PUBLIC OpenGL::GL GLEW::glew_s)
    target_link_libraries(stms_static PUBLIC OpenGL::GL GLEW::glew_s)
endif()

if (STMS_ENABLE_VULKAN)
    add_compile_definitions(stms_static STMS_ENABLE_VULKAN)
    add_compile_definitions(stms_shared STMS_ENABLE_VULKAN)
    message("-- Vulkan support enabled for STMS")

    find_package(Vulkan REQUIRED)

    target_include_directories(stms_shared PUBLIC ${Vulkan_INCLUDE_DIRS} dep/shaderc/libshaderc_util/include dep/shaderc/libshaderc/include)
    target_include_directories(stms_static PUBLIC ${Vulkan_INCLUDE_DIRS} dep/shaderc/libshaderc_util/include dep/shaderc/libshaderc/include)

    target_link_libraries(stms_shared PUBLIC Vulkan::Vulkan)
    target_link_libraries(stms_static PUBLIC Vulkan::Vulkan)


    set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(SHADERC_SPVC_ENABLE_DIRECT_LOGGING ON CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)

    set(SHADERC_ENABLE_SHARED_CRT OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(SHADERC_ENABLE_WERROR_COMPILE OFF CACHE BOOL "" FORCE)
    set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    set(ENABLE_HLSL ON CACHE BOOL "" FORCE) # I must leave HLSL on bc shaderc stubbornly wants to link it
    # I'm not sure if we need spirv-opt
#     set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    add_subdirectory(dep/shaderc)

    target_link_libraries(stms_shared PRIVATE shaderc)
    target_link_libraries(stms_static PRIVATE shaderc)
endif()

if (STMS_BUILD_TESTS)
    message("-- STMS tests enabled")
    add_subdirectory(tests)
endif (STMS_BUILD_TESTS)

if (STMS_BUILD_SAMPLES)
    message("-- STMS samples enabled")
    add_subdirectory(samples)
endif (STMS_BUILD_SAMPLES)

configure_file("Doxyfile.in" "../Doxyfile" @ONLY)