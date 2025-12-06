cmake_minimum_required(VERSION 3.20)

project(projection_core VERSION 0.1.0 LANGUAGES CXX)

set(CORE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(projection_core
    ${CORE_SOURCE_DIR}/projection/core/CoreInit.cpp
    ${CORE_SOURCE_DIR}/projection/core/Ids.h
    ${CORE_SOURCE_DIR}/projection/core/Enums.h
    ${CORE_SOURCE_DIR}/projection/core/Feed.cpp
    ${CORE_SOURCE_DIR}/projection/core/Feed.h
    ${CORE_SOURCE_DIR}/projection/core/RendererProtocol.cpp
    ${CORE_SOURCE_DIR}/projection/core/RendererProtocol.h
    ${CORE_SOURCE_DIR}/projection/core/Serialization.cpp
    ${CORE_SOURCE_DIR}/projection/core/Serialization.h
    ${CORE_SOURCE_DIR}/projection/core/Surface.cpp
    ${CORE_SOURCE_DIR}/projection/core/Surface.h
    ${CORE_SOURCE_DIR}/projection/core/Scene.cpp
    ${CORE_SOURCE_DIR}/projection/core/Scene.h
    ${CORE_SOURCE_DIR}/projection/core/Cue.cpp
    ${CORE_SOURCE_DIR}/projection/core/Cue.h
    ${CORE_SOURCE_DIR}/projection/core/Validation.cpp
    ${CORE_SOURCE_DIR}/projection/core/Validation.h
)

target_include_directories(projection_core
    PUBLIC
        ${CORE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_features(projection_core PUBLIC cxx_std_17)

add_library(Catch2 INTERFACE)

target_include_directories(Catch2
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch2/include
)

target_compile_features(Catch2 INTERFACE cxx_std_17)

add_library(Catch2WithMain
    third_party/catch2/src/catch_main.cpp
)

target_link_libraries(Catch2WithMain PUBLIC Catch2)

target_include_directories(Catch2WithMain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch2/include
)

target_compile_features(Catch2WithMain PUBLIC cxx_std_17)

add_library(Catch2::Catch2 ALIAS Catch2)
add_library(Catch2::Catch2WithMain ALIAS Catch2WithMain)

add_executable(projection_core_tests
    tests/CoreInit_test.cpp
    tests/IdsEnums_test.cpp
    tests/FeedConfig_test.cpp
    tests/FeedSceneSurfaceCue_test.cpp
    tests/Serialization_test.cpp
    tests/RendererProtocol_LoadSceneDefinition_test.cpp
    tests/RendererProtocol_test.cpp
    tests/Validation_test.cpp
)

target_compile_features(projection_core_tests PRIVATE cxx_std_17)

target_link_libraries(projection_core_tests
    PRIVATE
        projection_core
        Catch2::Catch2WithMain
)

include(CTest)

add_test(NAME projection_core_tests COMMAND projection_core_tests)
