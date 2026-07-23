if(NEBULA_ENABLE_GRPC AND NEBULA_ENABLE_STORAGE)
    add_executable(nebula_device_service
        src/main.cpp
        src/DeviceServiceImpl.cpp
        src/DeviceServiceContext.cpp
    )

    target_include_directories(nebula_device_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_device_service
        PRIVATE
            nebula_common
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
else()
    add_executable(nebula_device_service
        src/main.cpp
    )

    target_include_directories(nebula_device_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(nebula_device_service
        PRIVATE
            nebula_common
    )
endif()
