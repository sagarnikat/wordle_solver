add_executable(test_feedback test_feedback.cpp)
target_link_libraries(test_feedback PRIVATE wordle_core)
add_test(NAME test_feedback COMMAND test_feedback)

add_executable(test_solver test_solver.cpp)
target_link_libraries(test_solver PRIVATE wordle_core)
target_compile_definitions(test_solver PRIVATE
    WORDLE_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME test_solver COMMAND test_solver)