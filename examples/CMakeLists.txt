if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(echo_server
        echo_server.cpp
    )

    target_link_libraries(echo_server
        PRIVATE
            nebula_common
    )

    add_executable(protocol_echo_server
        protocol_echo_server.cpp
    )

    target_link_libraries(protocol_echo_server
        PRIVATE
            nebula_common
    )
else()
    message(WARNING "network examples are skipped on ${CMAKE_SYSTEM_NAME}; they require Linux epoll")
endif()

if(NEBULA_ENABLE_STORAGE)
    add_executable(storage_demo
        storage_demo.cpp
    )

    target_link_libraries(storage_demo
        PRIVATE
            nebula_common
    )
endif()

if(NEBULA_ENABLE_GRPC)
    add_executable(grpc_user_client
        grpc_user_client.cpp
    )

    target_link_libraries(grpc_user_client
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(protobuf_packet_demo
        protobuf_packet_demo.cpp
    )

    target_link_libraries(protobuf_packet_demo
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(user_register_login_client
        user_register_login_client.cpp
    )

    target_link_libraries(user_register_login_client
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(gateway_client_demo
        gateway_client_demo.cpp
    )

    target_link_libraries(gateway_client_demo
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(gateway_websocket_client_demo
        gateway_websocket_client_demo.cpp
    )

    target_link_libraries(gateway_websocket_client_demo
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(gateway_push_demo
        gateway_push_demo.cpp
    )

    target_link_libraries(gateway_push_demo
        PRIVATE
            nebula_common
    )

    add_executable(push_service_client
        push_service_client.cpp
    )

    target_link_libraries(push_service_client
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(push_kafka_produce_demo
        push_kafka_produce_demo.cpp
    )

    target_link_libraries(push_kafka_produce_demo
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(message_service_client
        message_service_client.cpp
    )

    target_link_libraries(message_service_client
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )

    add_executable(relation_service_client
        relation_service_client.cpp
    )

    target_link_libraries(relation_service_client
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
endif()
