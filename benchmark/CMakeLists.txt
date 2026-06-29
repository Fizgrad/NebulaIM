add_library(nebula_benchmark_stats STATIC
    Stats.cpp
)

target_include_directories(nebula_benchmark_stats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

foreach(BENCH_TARGET
    bench_tcp_connections
    bench_gateway_login
    bench_single_message
    bench_group_message
    bench_push_e2e
)
    add_executable(${BENCH_TARGET} ${BENCH_TARGET}.cpp)
    target_link_libraries(${BENCH_TARGET} PRIVATE nebula_benchmark_stats)
endforeach()
