#include "common/db/MySqlConnection.h"

#include "common/db/MySqlResult.h"
#include "common/log/Logger.h"

#include <mysql/mysql.h>

namespace nebula {

MySqlConnection::MySqlConnection()
    : mysql_(nullptr),
      connected_(false) {}

MySqlConnection::~MySqlConnection() {
    close();
}

bool MySqlConnection::connect(const MySqlConfig& config) {
    close();
    config_ = config;
    MYSQL* mysql = mysql_init(nullptr);
    if (mysql == nullptr) {
        last_error_ = "mysql_init failed";
        LOG_ERROR(last_error_);
        return false;
    }

    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, config.charset.c_str());
    MYSQL* result = mysql_real_connect(mysql,
                                      config.host.c_str(),
                                      config.user.c_str(),
                                      config.password.c_str(),
                                      config.database.c_str(),
                                      static_cast<unsigned int>(config.port),
                                      nullptr,
                                      CLIENT_MULTI_STATEMENTS);
    if (result == nullptr) {
        last_error_ = mysql_error(mysql);
        LOG_ERROR("MySQL connect failed: " + last_error_);
        mysql_close(mysql);
        return false;
    }

    mysql_ = mysql;
    connected_ = true;
    return true;
}

void MySqlConnection::close() {
    if (mysql_ != nullptr) {
        mysql_close(static_cast<MYSQL*>(mysql_));
        mysql_ = nullptr;
    }
    connected_ = false;
}

bool MySqlConnection::reconnect() {
    return connect(config_);
}

bool MySqlConnection::executeUpdate(const std::string& sql) {
    if (!connected_) {
        last_error_ = "MySQL connection is not connected";
        return false;
    }
    if (mysql_query(static_cast<MYSQL*>(mysql_), sql.c_str()) != 0) {
        last_error_ = mysql_error(static_cast<MYSQL*>(mysql_));
        LOG_ERROR("MySQL update failed: " + last_error_ + " sql=" + sql);
        return false;
    }
    do {
        MYSQL_RES* result = mysql_store_result(static_cast<MYSQL*>(mysql_));
        if (result != nullptr) {
            mysql_free_result(result);
        }
    } while (mysql_next_result(static_cast<MYSQL*>(mysql_)) == 0);
    return true;
}

std::unique_ptr<MySqlResult> MySqlConnection::executeQuery(const std::string& sql) {
    if (!connected_) {
        last_error_ = "MySQL connection is not connected";
        return nullptr;
    }
    if (mysql_query(static_cast<MYSQL*>(mysql_), sql.c_str()) != 0) {
        last_error_ = mysql_error(static_cast<MYSQL*>(mysql_));
        LOG_ERROR("MySQL query failed: " + last_error_ + " sql=" + sql);
        return nullptr;
    }
    MYSQL_RES* result = mysql_store_result(static_cast<MYSQL*>(mysql_));
    if (result == nullptr && mysql_field_count(static_cast<MYSQL*>(mysql_)) != 0) {
        last_error_ = mysql_error(static_cast<MYSQL*>(mysql_));
        LOG_ERROR("MySQL store result failed: " + last_error_);
        return nullptr;
    }
    return std::make_unique<MySqlResult>(result);
}

uint64_t MySqlConnection::lastInsertId() const {
    if (mysql_ == nullptr) {
        return 0;
    }
    return mysql_insert_id(static_cast<MYSQL*>(mysql_));
}

uint64_t MySqlConnection::affectedRows() const {
    if (mysql_ == nullptr) {
        return 0;
    }
    my_ulonglong rows = mysql_affected_rows(static_cast<MYSQL*>(mysql_));
    return rows == static_cast<my_ulonglong>(-1) ? 0 : static_cast<uint64_t>(rows);
}

bool MySqlConnection::beginTransaction() {
    return executeUpdate("BEGIN");
}

bool MySqlConnection::commit() {
    return executeUpdate("COMMIT");
}

bool MySqlConnection::rollback() {
    return executeUpdate("ROLLBACK");
}

std::string MySqlConnection::escapeString(const std::string& input) {
    if (mysql_ == nullptr) {
        return input;
    }
    std::string output;
    output.resize(input.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(static_cast<MYSQL*>(mysql_), output.data(), input.data(), input.size());
    output.resize(len);
    return output;
}

bool MySqlConnection::isConnected() const {
    return connected_;
}

std::string MySqlConnection::lastError() const {
    return last_error_;
}

}  // namespace nebula
