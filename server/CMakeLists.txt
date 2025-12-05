cmake_minimum_required(VERSION 3.20)

project(projection_server VERSION 0.1.0 LANGUAGES CXX)

set(SERVER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(SQLite3 REQUIRED)

add_library(projection_server_lib
    ${SERVER_SOURCE_DIR}/db/SqliteConnection.cpp
    ${SERVER_SOURCE_DIR}/db/SqliteConnection.h
    ${SERVER_SOURCE_DIR}/db/SchemaMigrations.cpp
    ${SERVER_SOURCE_DIR}/db/SchemaMigrations.h
)

set_target_properties(projection_server_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(projection_server_lib
    PUBLIC
        ${SERVER_SOURCE_DIR}
)

target_link_libraries(projection_server_lib
    PUBLIC
        projection_core
        SQLite::SQLite3
)

add_executable(projection_server ${SERVER_SOURCE_DIR}/main.cpp)

set_target_properties(projection_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(projection_server
    PRIVATE
        projection_server_lib
)

add_executable(projection_server_tests
    tests/SqliteConnection_test.cpp
    tests/SchemaMigrations_test.cpp
)

set_target_properties(projection_server_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(projection_server_tests
    PRIVATE
        projection_server_lib
        Catch2::Catch2WithMain
)

add_test(NAME projection_server_tests COMMAND projection_server_tests)
