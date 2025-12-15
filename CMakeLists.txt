cmake_minimum_required(VERSION 3.20)

project(projection-mapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(core)
add_subdirectory(server)
add_subdirectory(renderer)
add_subdirectory(clients/commandlineclient)
