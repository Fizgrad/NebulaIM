#include "common/db/MySqlConnectionPool.h"

#include "common/log/Logger.h"

#include <chrono>

namespace nebula {

MySqlConnectionPool::ConnectionGuard::ConnectionGuard(MySqlConnectionPool* pool, MySqlConnection* conn)
    : pool_(pool),
      conn_(conn) {}

MySqlConnectionPool::ConnectionGuard::~ConnectionGuard() {
    if (pool_ != nullptr && conn_ != nullptr) {
        pool_->release(conn_);
    }
}

MySqlConnectionPool::ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : pool_(other.pool_),
      conn_(other.conn_) {
    other.pool_ = nullptr;
    other.conn_ = nullptr;
}

MySqlConnectionPool::ConnectionGuard& MySqlConnectionPool::ConnectionGuard::operator=(ConnectionGuard&& other) noexcept {
    if (this != &other) {
        if (pool_ != nullptr && conn_ != nullptr) {
            pool_->release(conn_);
        }
        pool_ = other.pool_;
        conn_ = other.conn_;
        other.pool_ = nullptr;
        other.conn_ = nullptr;
    }
    return *this;
}

MySqlConnection* MySqlConnectionPool::ConnectionGuard::operator->() {
    return conn_;
}

MySqlConnection& MySqlConnectionPool::ConnectionGuard::operator*() {
    return *conn_;
}

MySqlConnectionPool::ConnectionGuard::operator bool() const {
    return conn_ != nullptr;
}

MySqlConnectionPool::MySqlConnectionPool()
    : stopped_(false) {}

MySqlConnectionPool::~MySqlConnectionPool() {
    stop();
}

bool MySqlConnectionPool::init(const MySqlConfig& config, size_t pool_size) {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = false;
    config_ = config;
    if (pool_size == 0) {
        pool_size = 1;
    }
    for (size_t i = 0; i < pool_size; ++i) {
        auto conn = std::make_unique<MySqlConnection>();
        if (!conn->connect(config_)) {
            LOG_ERROR("MySQL pool init failed: " + conn->lastError());
            connections_.clear();
            while (!idle_.empty()) {
                idle_.pop();
            }
            stopped_ = true;
            return false;
        }
        idle_.push(conn.get());
        connections_.push_back(std::move(conn));
    }
    return true;
}

MySqlConnectionPool::ConnectionGuard MySqlConnectionPool::acquire(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool ok = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
        return stopped_ || !idle_.empty();
    });
    if (!ok || stopped_ || idle_.empty()) {
        return ConnectionGuard(nullptr, nullptr);
    }
    MySqlConnection* conn = idle_.front();
    idle_.pop();
    return ConnectionGuard(this, conn);
}

void MySqlConnectionPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    while (!idle_.empty()) {
        idle_.pop();
    }
    for (auto& conn : connections_) {
        conn->close();
    }
    connections_.clear();
    cv_.notify_all();
}

void MySqlConnectionPool::release(MySqlConnection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || conn == nullptr) {
        return;
    }
    if (!conn->isConnected()) {
        conn->reconnect();
    }
    idle_.push(conn);
    cv_.notify_one();
}

}  // namespace nebula
