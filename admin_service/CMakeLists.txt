if(NEBULA_ENABLE_GRPC)
    add_executable(nebula_admin_service
        src/main.cpp
        src/AdminServiceImpl.cpp
        src/AdminServiceContext.cpp
    )

    target_include_directories(nebula_admin_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_admin_service
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
else()
    add_executable(nebula_admin_service
        src/main.cpp
    )

    target_include_directories(nebula_admin_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_admin_service
        PRIVATE
            nebula_common
    )
endif()
