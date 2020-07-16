cmake_minimum_required(VERSION 3.13.5)

SET(CMAKE_CXX_STANDARD 17)

project(stms_dtls_cli LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
add_executable(stms_dtls_cli DTLS/client.cpp)
target_compile_options(stms_dtls_cli PUBLIC ${STMS_COMPILE_FLAGS})
target_include_directories(stms_dtls_cli PUBLIC ../include)
target_link_libraries(stms_dtls_cli stms_static)

project(stms_dtls_serv LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
add_executable(stms_dtls_serv DTLS/server.cpp)
target_compile_options(stms_dtls_serv PUBLIC ${STMS_COMPILE_FLAGS})
target_include_directories(stms_dtls_serv PUBLIC ../include)
target_link_libraries(stms_dtls_serv stms_static)

if (STMS_ENABLE_OPENGL)
    message("-- OpenGL demos enabled for STMS")

    project(stms_ogl_demo LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
    add_executable(stms_ogl_demo ogl_demo.cpp)
    target_compile_options(stms_ogl_demo PUBLIC ${STMS_COMPILE_FLAGS})
    target_include_directories(stms_ogl_demo PUBLIC ../include)
    target_link_libraries(stms_ogl_demo stms_shared) # for some reason linking fails with g++ when we use a static lib, but works fine with clang
endif()

if (STMS_ENABLE_VULKAN)
    message("-- Vulkan demos enabled for STMS")

    project(stms_vk_demo LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
    add_executable(stms_vk_demo vk_demo.cpp)
    target_compile_options(stms_vk_demo PUBLIC ${STMS_COMPILE_FLAGS})
    target_include_directories(stms_vk_demo PUBLIC ../include)
    target_link_libraries(stms_vk_demo stms_static) # for some reason linking fails with g++ when we use a static lib, but works fine with clang
endif()
