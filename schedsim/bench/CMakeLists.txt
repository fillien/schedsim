# Performance benchmarks using Google Benchmark

include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

# Disable benchmark tests to speed up build
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(schedsim-bench
    engine_bench.cpp
)

target_link_libraries(schedsim-bench PRIVATE
    schedsim-algo
    benchmark::benchmark
    benchmark::benchmark_main
)
