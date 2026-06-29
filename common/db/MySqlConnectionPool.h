#pragma once

#include "common/db/MySqlConnection.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace nebula {

class MySqlConnectionPool {
public:
    class ConnectionGuard {
    public:
        ConnectionGuard(MySqlConnectionPool* pool, MySqlConnection* conn);
        ~ConnectionGuard();

        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;

        ConnectionGuard(ConnectionGuard&& other) noexcept;
        ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;

        MySqlConnection* operator->();
        MySqlConnection& operator*();
        explicit operator bool() const;

    private:
        MySqlConnectionPool* pool_;
        MySqlConnection* conn_;
    };

    MySqlConnectionPool();
    ~MySqlConnectionPool();

    bool init(const MySqlConfig& config, size_t pool_size);
    ConnectionGuard acquire(int timeout_ms = 3000);
    void stop();

private:
    void release(MySqlConnection* conn);

private:
    MySqlConfig config_;
    std::vector<std::unique_ptr<MySqlConnection>> connections_;
    std::queue<MySqlConnection*> idle_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_;
};

}  // namespace nebula
