if(NEBULA_ENABLE_GRPC AND NEBULA_ENABLE_STORAGE)
    add_executable(nebula_message_service
        src/main.cpp
        src/MessageServiceImpl.cpp
        src/MessageServiceContext.cpp
        src/MessageOutboxWorkerContext.cpp
    )

    target_include_directories(nebula_message_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_message_service
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
else()
    add_executable(nebula_message_service
        src/main.cpp
    )

    target_include_directories(nebula_message_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_message_service
        PRIVATE
            nebula_common
    )
endif()
