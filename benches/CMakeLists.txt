project(bilog_bench VERSION ${PROJECT_VERSION})

find_package(benchmark REQUIRED)
find_package(spdlog REQUIRED)

add_executable(file_bench file_bench.cpp)
target_link_libraries(file_bench PRIVATE
    bilog
    benchmark::benchmark
    benchmark::benchmark_main
    spdlog::spdlog
)

add_executable(ringbuff_bench ringbuff_bench.cpp)
target_link_libraries(ringbuff_bench PRIVATE
    bilog
    benchmark::benchmark
    benchmark::benchmark_main
    spdlog::spdlog
)
