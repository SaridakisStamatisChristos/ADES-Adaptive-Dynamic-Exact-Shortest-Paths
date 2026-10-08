cmake_minimum_required(VERSION 3.20)
project(ades VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(ZLIB REQUIRED)
add_library(ades src/graph.cpp src/coordinates.cpp src/shortest_paths.cpp src/dynamic_repair.cpp src/ades.cpp src/trace.cpp)
target_include_directories(ades PUBLIC include)
target_link_libraries(ades PUBLIC ZLIB::ZLIB)
target_compile_options(ades PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ades_cli src/main.cpp)
target_link_libraries(ades_cli PRIVATE ades)
enable_testing()
add_executable(ades_tests tests/test_main.cpp)
target_link_libraries(ades_tests PRIVATE ades)
add_test(NAME ades_tests COMMAND ades_tests)

add_executable(ades_trace_identity_tests tests/trace_identity.cpp)
target_link_libraries(ades_trace_identity_tests PRIVATE ades)
add_test(NAME ades_trace_identity_tests COMMAND ades_trace_identity_tests)

add_executable(ades_memory_budget_tests tests/memory_budget.cpp)
target_link_libraries(ades_memory_budget_tests PRIVATE ades)
add_test(NAME ades_memory_budget_tests COMMAND ades_memory_budget_tests)

add_executable(ades_telemetry_tests tests/telemetry.cpp)
target_link_libraries(ades_telemetry_tests PRIVATE ades)
add_test(NAME ades_telemetry_tests COMMAND ades_telemetry_tests)

add_executable(ades_bench benchmarks/runner.cpp)
target_link_libraries(ades_bench PRIVATE ades)

add_executable(ades_catastrophic_cut benchmarks/catastrophic_cut.cpp)
target_link_libraries(ades_catastrophic_cut PRIVATE ades)

add_executable(ades_trace_replay tests/trace_replay.cpp)
target_link_libraries(ades_trace_replay PRIVATE ades)

add_executable(ades_controller_workload benchmarks/controller_workload.cpp)
target_link_libraries(ades_controller_workload PRIVATE ades)

add_executable(ades_phase benchmarks/phase_diagram.cpp)
target_link_libraries(ades_phase PRIVATE ades)
