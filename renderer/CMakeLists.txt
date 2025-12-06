cmake_minimum_required(VERSION 3.20)

project(renderer LANGUAGES CXX)

set(RENDERER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(renderer_state
    ${RENDERER_SRC_DIR}/RenderState.cpp
    ${RENDERER_SRC_DIR}/RenderState.h
)

target_include_directories(renderer_state
    PUBLIC
        ${RENDERER_SRC_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(renderer_state
    PUBLIC
        projection_core
)

target_compile_features(renderer_state PUBLIC cxx_std_17)

add_library(renderer_net
    ${RENDERER_SRC_DIR}/net/RendererServer.cpp
    ${RENDERER_SRC_DIR}/net/RendererServer.h
)

target_include_directories(renderer_net
    PUBLIC
        ${RENDERER_SRC_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(renderer_net
    PUBLIC
        projection_core
)

target_compile_features(renderer_net PUBLIC cxx_std_17)

add_executable(renderer_app
    ${RENDERER_SRC_DIR}/main.cpp
    ${RENDERER_SRC_DIR}/ofApp.cpp
    ${RENDERER_SRC_DIR}/ofApp.h
)

target_include_directories(renderer_app
    PRIVATE
        ${RENDERER_SRC_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(renderer_app
    PRIVATE
        renderer_net
        renderer_state
        projection_core
)

target_compile_features(renderer_app PRIVATE cxx_std_17)

add_executable(renderer_tests
    tests/RendererServer_test.cpp
    tests/RenderState_test.cpp
)

target_link_libraries(renderer_tests
    PRIVATE
        renderer_net
        renderer_state
        projection_core
        Catch2::Catch2WithMain
)

target_include_directories(renderer_tests
    PRIVATE
        ${RENDERER_SRC_DIR}
        ${CMAKE_SOURCE_DIR}/core/third_party/catch2/include
)

target_compile_features(renderer_tests PRIVATE cxx_std_17)

include(CTest)
add_test(NAME renderer_tests COMMAND renderer_tests)

