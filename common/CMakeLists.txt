find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQLCLIENT_LIB mysqlclient)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIB hiredis)
find_path(RDKAFKA_INCLUDE_DIR librdkafka/rdkafkacpp.h)
find_library(RDKAFKA_LIB rdkafka)
find_library(RDKAFKACPP_LIB rdkafka++)
find_package(OpenSSL QUIET)

if(MYSQL_INCLUDE_DIR AND MYSQLCLIENT_LIB AND HIREDIS_INCLUDE_DIR AND HIREDIS_LIB AND RDKAFKA_INCLUDE_DIR AND RDKAFKA_LIB AND RDKAFKACPP_LIB)
    set(NEBULA_ENABLE_STORAGE ON CACHE BOOL "Enable storage layer" FORCE)
else()
    set(NEBULA_ENABLE_STORAGE OFF CACHE BOOL "Enable storage layer" FORCE)
    message(WARNING "Storage dependencies missing; storage layer targets are skipped. Install default-libmysqlclient-dev libhiredis-dev librdkafka-dev.")
endif()

set(NEBULA_COMMON_SOURCES
    auth/PasswordHasher.cpp
    auth/TokenManager.cpp
    message/MessageIdGenerator.cpp
    message/ConversationId.cpp
    message/MessageDeduplicator.cpp
    message/MessageKafkaPayload.cpp
    push/OnlineStatusManager.cpp
    push/GatewayClient.cpp
    push/GatewayClientManager.cpp
    push/PushRetryManager.cpp
    push/PushDispatcher.cpp
    gateway/ConnectionContext.cpp
    gateway/ConnectionManager.cpp
    gateway/GatewayOnlineManager.cpp
    gateway/GatewayBackendClients.cpp
    gateway/GatewayPacketHelper.cpp
    gateway/GatewayRouter.cpp
    monitor/Metrics.cpp
    monitor/MetricsRegistry.cpp
    monitor/MetricsHttpServer.cpp
    websocket/WebSocketFrame.cpp
    websocket/WebSocketHandshake.cpp
    websocket/WebSocketCodec.cpp
    async/FutureCallback.cpp
    async/RpcExecutor.cpp
    ratelimit/TokenBucket.cpp
    ratelimit/RateLimiter.cpp
    circuitbreaker/CircuitBreaker.cpp
    circuitbreaker/CircuitBreakerManager.cpp
    trace/TraceId.cpp
    trace/TraceContext.cpp
    discovery/StaticServiceResolver.cpp
    health/HealthStatus.cpp
    security/AdminAuth.cpp
    log/Logger.cpp
    config/Config.cpp
    thread/ThreadPool.cpp
    net/Buffer.cpp
    protocol/Packet.cpp
    protocol/PacketHeader.cpp
    protocol/MessageType.cpp
    protocol/ProtocolError.cpp
    protocol/PacketCodec.cpp
    rpc/GrpcServer.cpp
    rpc/GrpcClient.cpp
    rpc/RpcMetadata.cpp
    rpc/GrpcTlsCredentials.cpp
    utils/TimeUtil.cpp
    error/ErrorCode.cpp
)

if(NEBULA_ENABLE_STORAGE)
    list(APPEND NEBULA_COMMON_SOURCES
        db/MySqlConnection.cpp
        db/MySqlResult.cpp
        db/MySqlConnectionPool.cpp
        db/MySqlTransaction.cpp
        redis/RedisClient.cpp
        kafka/KafkaProducer.cpp
        kafka/KafkaConsumer.cpp
        dao/UserDao.cpp
        dao/RelationDao.cpp
        dao/GroupDao.cpp
        dao/MessageDao.cpp
        dao/OfflineMessageDao.cpp
        dao/FriendRequestDao.cpp
        dao/MessageReceiptDao.cpp
        outbox/OutboxDao.cpp
        outbox/OutboxWorker.cpp
        conversation/ConversationDao.cpp
        conversation/ConversationServiceHelper.cpp
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND NEBULA_COMMON_SOURCES
        net/InetAddress.cpp
        net/Socket.cpp
        net/Channel.cpp
        net/EpollPoller.cpp
        net/EventLoop.cpp
        net/Acceptor.cpp
        net/TcpConnection.cpp
        net/TcpServer.cpp
        net/EventLoopThread.cpp
        net/EventLoopThreadPool.cpp
        net/Timer.cpp
        net/TimerQueue.cpp
    )
else()
    message(WARNING "Linux network library is skipped on ${CMAKE_SYSTEM_NAME}; epoll/eventfd/timerfd are Linux-only")
endif()

add_library(nebula_common STATIC
    ${NEBULA_COMMON_SOURCES}
)

target_include_directories(nebula_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(nebula_common
    PUBLIC
        Threads::Threads
)

if(OpenSSL_FOUND)
    target_link_libraries(nebula_common PUBLIC OpenSSL::Crypto)
else()
    message(FATAL_ERROR "OpenSSL::Crypto not found. Install libssl-dev.")
endif()

if(NEBULA_ENABLE_STORAGE)
    target_compile_definitions(nebula_common PUBLIC NEBULA_ENABLE_STORAGE)
    target_include_directories(nebula_common PUBLIC ${MYSQL_INCLUDE_DIR} ${HIREDIS_INCLUDE_DIR} ${RDKAFKA_INCLUDE_DIR})
    target_link_libraries(nebula_common PUBLIC ${MYSQLCLIENT_LIB} ${HIREDIS_LIB} ${RDKAFKACPP_LIB} ${RDKAFKA_LIB})
endif()

if(NEBULA_ENABLE_GRPC)
    target_compile_definitions(nebula_common PUBLIC NEBULA_ENABLE_GRPC)
    target_link_libraries(nebula_common
        PUBLIC
            nebula_proto
            ${NEBULA_GRPC_LIB}
            ${NEBULA_PROTOBUF_LIB}
    )
endif()
