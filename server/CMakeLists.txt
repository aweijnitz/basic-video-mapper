cmake_minimum_required(VERSION 3.20)

project(projection_server VERSION 0.1.0 LANGUAGES CXX)

set(SERVER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(projection_server_lib
    ${SERVER_SOURCE_DIR}/ServerApp.cpp
    ${SERVER_SOURCE_DIR}/ServerApp.h
)

target_include_directories(projection_server_lib
    PUBLIC
        ${SERVER_SOURCE_DIR}
)

target_compile_features(projection_server_lib PUBLIC cxx_std_17)

target_link_libraries(projection_server_lib
    PUBLIC
        projection_core
)

add_executable(projection_server
    ${SERVER_SOURCE_DIR}/main.cpp
)

target_compile_features(projection_server PRIVATE cxx_std_17)

target_link_libraries(projection_server
    PRIVATE
        projection_server_lib
)

add_executable(projection_server_tests
    tests/ServerApp_test.cpp
)

target_compile_features(projection_server_tests PRIVATE cxx_std_17)

target_link_libraries(projection_server_tests
    PRIVATE
        projection_server_lib
        Catch2::Catch2WithMain
)

add_test(NAME projection_server_tests COMMAND projection_server_tests)
