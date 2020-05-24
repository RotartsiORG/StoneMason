cmake_minimum_required(VERSION 3.15.3)

SET(CMAKE_CXX_STANDARD 17)

project(stms_dtls_cli LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
add_executable(stms_dtls_cli DTLS/client.cpp)
target_compile_options(stms_dtls_cli PUBLIC -O3 -Wall -Wextra -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wno-unused-parameter -g3 -ggdb -std=c++17)
target_include_directories(stms_dtls_cli PUBLIC ../include)
target_link_libraries(stms_dtls_cli stms_static)

project(stms_dtls_serv LANGUAGES CXX VERSION ${STMS_VERSION} DESCRIPTION "Samples for StoneMason")
add_executable(stms_dtls_serv DTLS/server.cpp)
target_compile_options(stms_dtls_serv PUBLIC -O3 -Wall -Wextra -Wno-unused-variable -Wno-unused-function -Wno-unused-value -Wno-unused-parameter -g3 -ggdb -std=c++17)
target_include_directories(stms_dtls_serv PUBLIC ../include)
target_link_libraries(stms_dtls_serv stms_static)
