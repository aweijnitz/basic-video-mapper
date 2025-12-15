cmake_minimum_required(VERSION 3.20)

add_executable(commandlineclient
    main.cpp
)

target_include_directories(commandlineclient
    PRIVATE
        ${CMAKE_SOURCE_DIR}/server/third_party
        ${CMAKE_SOURCE_DIR}/core/third_party
)

target_compile_features(commandlineclient PRIVATE cxx_std_17)
