cmake_minimum_required(VERSION 3.16)
project(wordle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Core library shared by the CLI, the benchmark, and the tests.
add_library(wordle_core
    src/feedback.cpp
    src/word_list.cpp
    src/entropy.cpp
    src/solver.cpp
    src/game.cpp
    src/ui.cpp
)
target_include_directories(wordle_core PUBLIC include)

# Main CLI: play / solve / analyze.
add_executable(wordle src/main.cpp)
target_link_libraries(wordle PRIVATE wordle_core)

# Full benchmark: auto-play every possible answer.
add_executable(wordle-benchmark benchmarks/benchmark_solver.cpp)
target_link_libraries(wordle-benchmark PRIVATE wordle_core)

option(WORDLE_BUILD_TESTS "Build unit tests" ON)
if(WORDLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()