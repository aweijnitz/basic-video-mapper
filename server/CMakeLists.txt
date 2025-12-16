cmake_minimum_required(VERSION 3.20)

project(lumi_server VERSION 0.1.0 LANGUAGES CXX)

find_package(SQLite3 QUIET)

if (NOT SQLite3_FOUND)
    find_path(SQLite3_INCLUDE_DIR sqlite3.h)
    find_library(SQLite3_LIBRARY sqlite3)

    if (SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)
        set(SQLite3_FOUND TRUE)
        set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
        set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
    else()
        message(FATAL_ERROR "SQLite3 not found. Please install SQLite3 development files.")
    endif()
endif()

set(SERVER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_library(lumi_server_lib
    ${SERVER_SOURCE_DIR}/Config.cpp
    ${SERVER_SOURCE_DIR}/Config.h
    ${SERVER_SOURCE_DIR}/ServerApp.cpp
    ${SERVER_SOURCE_DIR}/ServerApp.h
    ${SERVER_SOURCE_DIR}/db/SqliteConnection.cpp
    ${SERVER_SOURCE_DIR}/db/SqliteConnection.h
    ${SERVER_SOURCE_DIR}/db/SchemaMigrations.cpp
    ${SERVER_SOURCE_DIR}/db/SchemaMigrations.h
    ${SERVER_SOURCE_DIR}/http/HttpServer.cpp
    ${SERVER_SOURCE_DIR}/http/HttpServer.h
    ${SERVER_SOURCE_DIR}/renderer/RendererClient.cpp
    ${SERVER_SOURCE_DIR}/renderer/RendererClient.h
    ${SERVER_SOURCE_DIR}/repo/FeedRepository.cpp
    ${SERVER_SOURCE_DIR}/repo/FeedRepository.h
    ${SERVER_SOURCE_DIR}/repo/SurfaceRepository.cpp
    ${SERVER_SOURCE_DIR}/repo/SurfaceRepository.h
    ${SERVER_SOURCE_DIR}/repo/SceneRepository.cpp
    ${SERVER_SOURCE_DIR}/repo/SceneRepository.h
    ${SERVER_SOURCE_DIR}/repo/CueRepository.cpp
    ${SERVER_SOURCE_DIR}/repo/CueRepository.h
    ${SERVER_SOURCE_DIR}/repo/ProjectRepository.cpp
    ${SERVER_SOURCE_DIR}/repo/ProjectRepository.h
)

target_include_directories(lumi_server_lib
    PUBLIC
        ${SERVER_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${SQLite3_INCLUDE_DIRS}
)

target_compile_features(lumi_server_lib PUBLIC cxx_std_17)

if (TARGET SQLite::SQLite3)
    set(SQLITE_TARGET SQLite::SQLite3)
elseif (TARGET SQLite3::SQLite3)
    set(SQLITE_TARGET SQLite3::SQLite3)
endif()

if (SQLITE_TARGET)
    target_link_libraries(lumi_server_lib
        PUBLIC
            projection_core
            ${SQLITE_TARGET}
    )
else()
    target_link_libraries(lumi_server_lib
        PUBLIC
            projection_core
            ${SQLite3_LIBRARIES}
    )
endif()

add_executable(lumi_server
    ${SERVER_SOURCE_DIR}/main.cpp
)

target_compile_features(lumi_server PRIVATE cxx_std_17)

target_link_libraries(lumi_server
    PRIVATE
        lumi_server_lib
)

add_executable(lumi_server_tests
    tests/Config_test.cpp
    tests/ServerApp_test.cpp
    tests/Db_test.cpp
    tests/DbSurfaces_test.cpp
    tests/Repository_test.cpp
    tests/SurfaceRepository_test.cpp
    tests/ProjectRepository_test.cpp
    tests/HttpApi_test.cpp
    tests/HttpScenesSurfaces_test.cpp
    tests/HttpRendererIntegration_test.cpp
    tests/DemoFlow_test.cpp
    tests/RendererClient_test.cpp
    tests/RendererClient_LoadSceneDefinition_test.cpp
)

target_compile_features(lumi_server_tests PRIVATE cxx_std_17)

target_link_libraries(lumi_server_tests
    PRIVATE
        lumi_server_lib
        Catch2::Catch2WithMain
)

add_test(NAME lumi_server_tests COMMAND lumi_server_tests)
