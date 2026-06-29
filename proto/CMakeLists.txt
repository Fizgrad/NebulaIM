find_package(Protobuf QUIET)
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(GRPCPP QUIET IMPORTED_TARGET grpc++)
endif()

if(TARGET Protobuf::libprotobuf)
    add_library(nebula_protobuf INTERFACE IMPORTED GLOBAL)
    target_link_libraries(nebula_protobuf INTERFACE Protobuf::libprotobuf)
    set(NEBULA_PROTOBUF_LIB nebula_protobuf)
elseif(TARGET protobuf::libprotobuf)
    add_library(nebula_protobuf INTERFACE IMPORTED GLOBAL)
    target_link_libraries(nebula_protobuf INTERFACE protobuf::libprotobuf)
    set(NEBULA_PROTOBUF_LIB nebula_protobuf)
elseif(Protobuf_LIBRARIES)
    set(NEBULA_PROTOBUF_LIB ${Protobuf_LIBRARIES})
endif()

if(TARGET Protobuf::protoc)
    set(NEBULA_PROTOC_COMMAND Protobuf::protoc)
elseif(TARGET protobuf::protoc)
    set(NEBULA_PROTOC_COMMAND protobuf::protoc)
elseif(Protobuf_PROTOC_EXECUTABLE)
    set(NEBULA_PROTOC_COMMAND ${Protobuf_PROTOC_EXECUTABLE})
else()
    find_program(NEBULA_PROTOC_COMMAND protoc)
endif()

if(TARGET PkgConfig::GRPCPP)
    add_library(nebula_grpcpp INTERFACE IMPORTED GLOBAL)
    target_link_libraries(nebula_grpcpp INTERFACE PkgConfig::GRPCPP)
    set(NEBULA_GRPC_LIB nebula_grpcpp)
else()
    find_library(NEBULA_GRPC_LIB grpc++)
endif()

find_program(NEBULA_GRPC_CPP_PLUGIN grpc_cpp_plugin)

if(NOT Protobuf_FOUND OR NOT NEBULA_PROTOBUF_LIB OR NOT NEBULA_PROTOC_COMMAND OR NOT NEBULA_GRPC_LIB OR NOT NEBULA_GRPC_CPP_PLUGIN)
    message(WARNING "Protobuf/gRPC not found; nebula_proto and gRPC targets are skipped. Install protobuf-compiler, libprotobuf-dev, protobuf-compiler-grpc, and libgrpc++-dev.")
    set(NEBULA_ENABLE_GRPC OFF CACHE BOOL "Enable Protobuf/gRPC targets" FORCE)
    return()
endif()

set(NEBULA_ENABLE_GRPC ON CACHE BOOL "Enable Protobuf/gRPC targets" FORCE)
set(NEBULA_PROTOBUF_LIB ${NEBULA_PROTOBUF_LIB} CACHE INTERNAL "Nebula protobuf library" FORCE)
set(NEBULA_GRPC_LIB ${NEBULA_GRPC_LIB} CACHE INTERNAL "Nebula grpc++ library" FORCE)

set(PROTO_FILES
    common.proto
    user.proto
    message.proto
    relation.proto
    push.proto
    gateway.proto
)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(GENERATED_SRCS)
set(GENERATED_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE})
    set(PB_CC ${GENERATED_DIR}/${PROTO_NAME}.pb.cc)
    set(PB_H ${GENERATED_DIR}/${PROTO_NAME}.pb.h)
    set(GRPC_CC ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc)
    set(GRPC_H ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h)

    add_custom_command(
        OUTPUT ${PB_CC} ${PB_H} ${GRPC_CC} ${GRPC_H}
        COMMAND ${NEBULA_PROTOC_COMMAND}
        ARGS --grpc_out=${GENERATED_DIR}
             --cpp_out=${GENERATED_DIR}
             --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
             --plugin=protoc-gen-grpc=${NEBULA_GRPC_CPP_PLUGIN}
             ${PROTO_PATH}
        DEPENDS ${PROTO_PATH}
        COMMENT "Generating protobuf and gRPC sources for ${PROTO_FILE}"
        VERBATIM
    )

    list(APPEND GENERATED_SRCS ${PB_CC} ${GRPC_CC})
    list(APPEND GENERATED_HDRS ${PB_H} ${GRPC_H})
endforeach()

add_library(nebula_proto STATIC
    ${GENERATED_SRCS}
    ${GENERATED_HDRS}
)

target_include_directories(nebula_proto
    PUBLIC
        ${GENERATED_DIR}
)

target_link_libraries(nebula_proto
    PUBLIC
        ${NEBULA_PROTOBUF_LIB}
        ${NEBULA_GRPC_LIB}
)
