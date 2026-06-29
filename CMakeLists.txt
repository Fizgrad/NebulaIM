cmake_minimum_required(VERSION 3.16)

project(NebulaIM
    VERSION 0.1.0
    DESCRIPTION "C++17 high-performance distributed instant messaging system"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic)

enable_testing()

add_subdirectory(proto)
add_subdirectory(common)
add_subdirectory(gateway)
add_subdirectory(user_service)
add_subdirectory(message_service)
add_subdirectory(relation_service)
add_subdirectory(push_service)
add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(benchmark)
