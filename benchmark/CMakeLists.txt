add_library(nebula_benchmark_stats STATIC
    Stats.cpp
)

target_include_directories(nebula_benchmark_stats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(bench_tcp_connections bench_tcp_connections.cpp)
target_link_libraries(bench_tcp_connections PRIVATE nebula_benchmark_stats)

if(NEBULA_ENABLE_GRPC)
    add_library(nebula_benchmark_gateway STATIC
        GatewayBenchClient.cpp
    )
    target_include_directories(nebula_benchmark_gateway PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(nebula_benchmark_gateway PUBLIC nebula_benchmark_stats nebula_common nebula_proto)

    foreach(BENCH_TARGET
        bench_gateway_login
        bench_single_message
        bench_group_message
        bench_push_e2e
    )
        add_executable(${BENCH_TARGET} ${BENCH_TARGET}.cpp)
        target_link_libraries(${BENCH_TARGET} PRIVATE nebula_benchmark_gateway)
    endforeach()
else()
    message(WARNING "gRPC/protobuf is disabled; Gateway business benchmarks are skipped.")
endif()
