cmake_minimum_required(VERSION 3.0.0)

# add_executable(test1 tests/test1.cpp)
# add_executable(test2 tests/test2.cpp)
# add_executable(test3 tests/test3.cpp)
add_executable(test4 tests/test4.cpp)

# include_directories(test1 "include")
# include_directories(test2 "include")
# include_directories(test3 "include")
include_directories(test4 "include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++20")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -ggdb") # debug compilation
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
