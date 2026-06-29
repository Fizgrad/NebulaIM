#include "common/db/MySqlResult.h"

#include <cstdlib>
#include <mysql/mysql.h>

namespace nebula {

MySqlResult::MySqlResult(void* result)
    : result_(result),
      current_row_(nullptr),
      lengths_(nullptr) {
    buildFieldIndex();
}

MySqlResult::~MySqlResult() {
    if (result_ != nullptr) {
        mysql_free_result(static_cast<MYSQL_RES*>(result_));
    }
}

bool MySqlResult::next() {
    if (result_ == nullptr) {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(static_cast<MYSQL_RES*>(result_));
    current_row_ = row;
    lengths_ = row == nullptr ? nullptr : mysql_fetch_lengths(static_cast<MYSQL_RES*>(result_));
    return row != nullptr;
}

std::string MySqlResult::getString(const std::string& column) const {
    const char* v = value(column);
    if (v == nullptr) {
        return "";
    }
    auto it = field_index_.find(column);
    return std::string(v, lengths_[it->second]);
}

int MySqlResult::getInt(const std::string& column) const {
    return std::atoi(getString(column).c_str());
}

uint64_t MySqlResult::getUInt64(const std::string& column) const {
    return static_cast<uint64_t>(std::strtoull(getString(column).c_str(), nullptr, 10));
}

int64_t MySqlResult::getInt64(const std::string& column) const {
    return static_cast<int64_t>(std::strtoll(getString(column).c_str(), nullptr, 10));
}

double MySqlResult::getDouble(const std::string& column) const {
    return std::strtod(getString(column).c_str(), nullptr);
}

bool MySqlResult::isNull(const std::string& column) const {
    return value(column) == nullptr;
}

size_t MySqlResult::rowCount() const {
    if (result_ == nullptr) {
        return 0;
    }
    return static_cast<size_t>(mysql_num_rows(static_cast<MYSQL_RES*>(result_)));
}

void MySqlResult::buildFieldIndex() {
    if (result_ == nullptr) {
        return;
    }
    MYSQL_RES* result = static_cast<MYSQL_RES*>(result_);
    unsigned int count = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    for (unsigned int i = 0; i < count; ++i) {
        field_index_[fields[i].name] = i;
    }
}

const char* MySqlResult::value(const std::string& column) const {
    if (current_row_ == nullptr) {
        return nullptr;
    }
    auto it = field_index_.find(column);
    if (it == field_index_.end()) {
        return nullptr;
    }
    MYSQL_ROW row = static_cast<MYSQL_ROW>(current_row_);
    return row[it->second];
}

}  // namespace nebula
