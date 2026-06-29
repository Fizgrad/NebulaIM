add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE nebula_common)
add_test(NAME test_logger COMMAND test_logger WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE nebula_common)
add_test(NAME test_config COMMAND test_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_thread_pool test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE nebula_common)
add_test(NAME test_thread_pool COMMAND test_thread_pool WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_buffer test_buffer.cpp)
target_link_libraries(test_buffer PRIVATE nebula_common)
add_test(NAME test_buffer COMMAND test_buffer WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_time_util test_time_util.cpp)
target_link_libraries(test_time_util PRIVATE nebula_common)
add_test(NAME test_time_util COMMAND test_time_util WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_message_type test_message_type.cpp)
target_link_libraries(test_message_type PRIVATE nebula_common)
add_test(NAME test_message_type COMMAND test_message_type WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_protocol_error test_protocol_error.cpp)
target_link_libraries(test_protocol_error PRIVATE nebula_common)
add_test(NAME test_protocol_error COMMAND test_protocol_error WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_packet_codec test_packet_codec.cpp)
target_link_libraries(test_packet_codec PRIVATE nebula_common)
add_test(NAME test_packet_codec COMMAND test_packet_codec WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_metrics_registry test_metrics_registry.cpp)
target_link_libraries(test_metrics_registry PRIVATE nebula_common)
add_test(NAME test_metrics_registry COMMAND test_metrics_registry WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_metrics_http_server test_metrics_http_server.cpp)
target_link_libraries(test_metrics_http_server PRIVATE nebula_common)
add_test(NAME test_metrics_http_server COMMAND test_metrics_http_server WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_connection_manager test_connection_manager.cpp)
target_link_libraries(test_connection_manager PRIVATE nebula_common)
add_test(NAME test_connection_manager COMMAND test_connection_manager WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_gateway_packet_helper test_gateway_packet_helper.cpp)
target_link_libraries(test_gateway_packet_helper PRIVATE nebula_common nebula_proto ${NEBULA_PROTOBUF_LIB})
add_test(NAME test_gateway_packet_helper COMMAND test_gateway_packet_helper WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_gateway_online_manager test_gateway_online_manager.cpp)
target_link_libraries(test_gateway_online_manager PRIVATE nebula_common)

add_executable(test_gateway_router test_gateway_router.cpp)
target_link_libraries(test_gateway_router PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})
add_test(NAME test_gateway_router COMMAND test_gateway_router WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_gateway_service_impl
    test_gateway_service_impl.cpp
    ${PROJECT_SOURCE_DIR}/gateway/src/GatewayServiceImpl.cpp
)
target_include_directories(test_gateway_service_impl PRIVATE ${PROJECT_SOURCE_DIR}/gateway/include)
target_link_libraries(test_gateway_service_impl PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})
add_test(NAME test_gateway_service_impl COMMAND test_gateway_service_impl WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_gateway_client_flow test_gateway_client_flow.cpp)
target_link_libraries(test_gateway_client_flow PRIVATE nebula_common)
add_test(NAME test_gateway_client_flow COMMAND test_gateway_client_flow WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_online_status_manager test_online_status_manager.cpp)
target_link_libraries(test_online_status_manager PRIVATE nebula_common)

add_executable(test_gateway_client test_gateway_client.cpp)
target_link_libraries(test_gateway_client PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

add_executable(test_push_retry_manager test_push_retry_manager.cpp)
target_link_libraries(test_push_retry_manager PRIVATE nebula_common)

add_executable(test_message_id_generator test_message_id_generator.cpp)
target_link_libraries(test_message_id_generator PRIVATE nebula_common)
add_test(NAME test_message_id_generator COMMAND test_message_id_generator WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_conversation_id test_conversation_id.cpp)
target_link_libraries(test_conversation_id PRIVATE nebula_common)
add_test(NAME test_conversation_id COMMAND test_conversation_id WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_password_hasher test_password_hasher.cpp)
target_link_libraries(test_password_hasher PRIVATE nebula_common)
add_test(NAME test_password_hasher COMMAND test_password_hasher WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_token_manager test_token_manager.cpp)
target_link_libraries(test_token_manager PRIVATE nebula_common)
add_test(NAME test_token_manager COMMAND test_token_manager WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_websocket_handshake test_websocket_handshake.cpp)
target_link_libraries(test_websocket_handshake PRIVATE nebula_common)
add_test(NAME test_websocket_handshake COMMAND test_websocket_handshake WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_websocket_codec test_websocket_codec.cpp)
target_link_libraries(test_websocket_codec PRIVATE nebula_common)
add_test(NAME test_websocket_codec COMMAND test_websocket_codec WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_token_bucket test_token_bucket.cpp)
target_link_libraries(test_token_bucket PRIVATE nebula_common)
add_test(NAME test_token_bucket COMMAND test_token_bucket WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_circuit_breaker test_circuit_breaker.cpp)
target_link_libraries(test_circuit_breaker PRIVATE nebula_common)
add_test(NAME test_circuit_breaker COMMAND test_circuit_breaker WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_trace_context test_trace_context.cpp)
target_link_libraries(test_trace_context PRIVATE nebula_common)
add_test(NAME test_trace_context COMMAND test_trace_context WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_service_discovery test_service_discovery.cpp)
target_link_libraries(test_service_discovery PRIVATE nebula_common)
add_test(NAME test_service_discovery COMMAND test_service_discovery WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_grpc_tls_config test_grpc_tls_config.cpp)
target_link_libraries(test_grpc_tls_config PRIVATE nebula_common)
add_test(NAME test_grpc_tls_config COMMAND test_grpc_tls_config WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test_backend_final_e2e test_backend_final_e2e.cpp)
add_test(NAME test_backend_final_e2e COMMAND test_backend_final_e2e WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(NEBULA_ENABLE_GRPC)
    add_executable(test_protobuf_message test_protobuf_message.cpp)
    target_link_libraries(test_protobuf_message PRIVATE nebula_proto ${NEBULA_PROTOBUF_LIB})
    add_test(NAME test_protobuf_message COMMAND test_protobuf_message WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_message_recall test_message_recall.cpp)
    target_link_libraries(test_message_recall PRIVATE nebula_proto ${NEBULA_PROTOBUF_LIB})
    add_test(NAME test_message_recall COMMAND test_message_recall WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_read_receipt test_read_receipt.cpp)
    target_link_libraries(test_read_receipt PRIVATE nebula_proto ${NEBULA_PROTOBUF_LIB})
    add_test(NAME test_read_receipt COMMAND test_read_receipt WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_admin_auth
        test_admin_auth.cpp
        ${PROJECT_SOURCE_DIR}/admin_service/src/AdminServiceImpl.cpp
    )
    target_include_directories(test_admin_auth PRIVATE ${PROJECT_SOURCE_DIR}/admin_service/include)
    target_link_libraries(test_admin_auth PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})
    add_test(NAME test_admin_auth COMMAND test_admin_auth WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_grpc_user_service
        test_grpc_user_service.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceContext.cpp
    )
    target_include_directories(test_grpc_user_service PRIVATE ${PROJECT_SOURCE_DIR}/user_service/include)
    target_link_libraries(test_grpc_user_service PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})
    add_test(NAME test_grpc_user_service COMMAND test_grpc_user_service WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()

if(NEBULA_ENABLE_STORAGE)
    add_executable(test_user_service_impl
        test_user_service_impl.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceContext.cpp
    )
    target_include_directories(test_user_service_impl PRIVATE ${PROJECT_SOURCE_DIR}/user_service/include)
    target_link_libraries(test_user_service_impl PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_user_service_integration
        test_user_service_integration.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/user_service/src/UserServiceContext.cpp
    )
    target_include_directories(test_user_service_integration PRIVATE ${PROJECT_SOURCE_DIR}/user_service/include)
    target_link_libraries(test_user_service_integration PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_relation_service_impl
        test_relation_service_impl.cpp
        ${PROJECT_SOURCE_DIR}/relation_service/src/RelationServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/relation_service/src/RelationServiceContext.cpp
    )
    target_include_directories(test_relation_service_impl PRIVATE ${PROJECT_SOURCE_DIR}/relation_service/include)
    target_link_libraries(test_relation_service_impl PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_relation_service_integration
        test_relation_service_integration.cpp
        ${PROJECT_SOURCE_DIR}/relation_service/src/RelationServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/relation_service/src/RelationServiceContext.cpp
    )
    target_include_directories(test_relation_service_integration PRIVATE ${PROJECT_SOURCE_DIR}/relation_service/include)
    target_link_libraries(test_relation_service_integration PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_push_dispatcher
        test_push_dispatcher.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushServiceContext.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushWorker.cpp
    )
    target_include_directories(test_push_dispatcher PRIVATE ${PROJECT_SOURCE_DIR}/push_service/include)
    target_link_libraries(test_push_dispatcher PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_push_service_impl
        test_push_service_impl.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushServiceContext.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushWorker.cpp
    )
    target_include_directories(test_push_service_impl PRIVATE ${PROJECT_SOURCE_DIR}/push_service/include)
    target_link_libraries(test_push_service_impl PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_push_worker
        test_push_worker.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushServiceContext.cpp
        ${PROJECT_SOURCE_DIR}/push_service/src/PushWorker.cpp
    )
    target_include_directories(test_push_worker PRIVATE ${PROJECT_SOURCE_DIR}/push_service/include)
    target_link_libraries(test_push_worker PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_message_deduplicator test_message_deduplicator.cpp)
    target_link_libraries(test_message_deduplicator PRIVATE nebula_common)

    add_executable(test_outbox_dao test_outbox_dao.cpp)
    target_link_libraries(test_outbox_dao PRIVATE nebula_common)
    add_test(NAME test_outbox_dao COMMAND test_outbox_dao WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_outbox_worker test_outbox_worker.cpp)
    target_link_libraries(test_outbox_worker PRIVATE nebula_common)
    add_test(NAME test_outbox_worker COMMAND test_outbox_worker WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_conversation_dao test_conversation_dao.cpp)
    target_link_libraries(test_conversation_dao PRIVATE nebula_common)
    add_test(NAME test_conversation_dao COMMAND test_conversation_dao WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_friend_request_flow test_friend_request_flow.cpp)
    target_link_libraries(test_friend_request_flow PRIVATE nebula_common)
    add_test(NAME test_friend_request_flow COMMAND test_friend_request_flow WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_message_service_impl
        test_message_service_impl.cpp
        ${PROJECT_SOURCE_DIR}/message_service/src/MessageServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/message_service/src/MessageServiceContext.cpp
    )
    target_include_directories(test_message_service_impl PRIVATE ${PROJECT_SOURCE_DIR}/message_service/include)
    target_link_libraries(test_message_service_impl PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_message_service_integration
        test_message_service_integration.cpp
        ${PROJECT_SOURCE_DIR}/message_service/src/MessageServiceImpl.cpp
        ${PROJECT_SOURCE_DIR}/message_service/src/MessageServiceContext.cpp
    )
    target_include_directories(test_message_service_integration PRIVATE ${PROJECT_SOURCE_DIR}/message_service/include)
    target_link_libraries(test_message_service_integration PRIVATE nebula_common nebula_proto ${NEBULA_GRPC_LIB} ${NEBULA_PROTOBUF_LIB})

    add_executable(test_relation_dao_extra test_relation_dao_extra.cpp)
    target_link_libraries(test_relation_dao_extra PRIVATE nebula_common)

    add_executable(test_group_dao_extra test_group_dao_extra.cpp)
    target_link_libraries(test_group_dao_extra PRIVATE nebula_common)

    add_executable(test_mysql_pool test_mysql_pool.cpp)
    target_link_libraries(test_mysql_pool PRIVATE nebula_common)

    add_executable(test_user_dao test_user_dao.cpp)
    target_link_libraries(test_user_dao PRIVATE nebula_common)

    add_executable(test_message_dao test_message_dao.cpp)
    target_link_libraries(test_message_dao PRIVATE nebula_common)

    add_executable(test_redis_client test_redis_client.cpp)
    target_link_libraries(test_redis_client PRIVATE nebula_common)

    add_executable(test_kafka_producer test_kafka_producer.cpp)
    target_link_libraries(test_kafka_producer PRIVATE nebula_common)

    add_executable(test_kafka_consumer test_kafka_consumer.cpp)
    target_link_libraries(test_kafka_consumer PRIVATE nebula_common)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(test_inet_address test_inet_address.cpp)
    target_link_libraries(test_inet_address PRIVATE nebula_common)
    add_test(NAME test_inet_address COMMAND test_inet_address WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_event_loop test_event_loop.cpp)
    target_link_libraries(test_event_loop PRIVATE nebula_common)
    add_test(NAME test_event_loop COMMAND test_event_loop WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

    add_executable(test_timer_queue test_timer_queue.cpp)
    target_link_libraries(test_timer_queue PRIVATE nebula_common)
    add_test(NAME test_timer_queue COMMAND test_timer_queue WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
else()
    message(WARNING "Linux network tests are skipped on ${CMAKE_SYSTEM_NAME}")
endif()
