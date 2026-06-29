if(NEBULA_ENABLE_GRPC AND NEBULA_ENABLE_STORAGE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(nebula_gateway
        src/main.cpp
        src/GatewayServiceImpl.cpp
        src/GatewayContext.cpp
        src/GatewayServer.cpp
    )

    target_include_directories(nebula_gateway
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_gateway
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
else()
    add_executable(nebula_gateway
        src/main.cpp
    )

    target_include_directories(nebula_gateway
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_gateway
        PRIVATE
            nebula_common
    )
endif()
