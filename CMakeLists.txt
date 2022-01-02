cmake_minimum_required(VERSION 3.0.0)

add_executable(example tests/example.cpp)
add_executable(test8 tests/test8.cpp)
add_executable(test9 tests/test9.cpp)

include_directories(example "src")
include_directories(test8 "src")
include_directories(test9 "src")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++20")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -ggdb") # debug compilation
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -ggdb -fsanitize=address") # debug compilation
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pedantic")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
