cmake_minimum_required(VERSION 3.8)

project(gomoku_ai)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(sources
    src/gomoku.cpp
    src/grandom.cpp
   )

add_library(gomoku_ai ${sources})

target_include_directories(gomoku_ai PUBLIC include)

set(test_sources
    testsrc/gtest.cpp
   )

add_executable(gtest ${test_sources})

target_include_directories(gtest PUBLIC include)

target_link_libraries(gtest gomoku_ai)
