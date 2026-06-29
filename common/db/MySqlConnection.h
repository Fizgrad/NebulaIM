#pragma once

#include <memory>
#include <string>

namespace nebula {

class MySqlResult;

struct MySqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "nebula";
    std::string password = "nebula";
    std::string database = "nebula_im";
    std::string charset = "utf8mb4";
};

class MySqlConnection {
public:
    MySqlConnection();
    ~MySqlConnection();

    MySqlConnection(const MySqlConnection&) = delete;
    MySqlConnection& operator=(const MySqlConnection&) = delete;

    bool connect(const MySqlConfig& config);
    void close();
    bool reconnect();

    bool executeUpdate(const std::string& sql);
    std::unique_ptr<MySqlResult> executeQuery(const std::string& sql);

    uint64_t lastInsertId() const;
    uint64_t affectedRows() const;

    bool beginTransaction();
    bool commit();
    bool rollback();

    std::string escapeString(const std::string& input);

    bool isConnected() const;
    std::string lastError() const;

private:
    void* mysql_;
    MySqlConfig config_;
    bool connected_;
    std::string last_error_;
};

}  // namespace nebula
