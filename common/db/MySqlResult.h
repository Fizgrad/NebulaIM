#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace nebula {

class MySqlResult {
public:
    explicit MySqlResult(void* result);
    ~MySqlResult();

    MySqlResult(const MySqlResult&) = delete;
    MySqlResult& operator=(const MySqlResult&) = delete;

    bool next();

    std::string getString(const std::string& column) const;
    int getInt(const std::string& column) const;
    uint64_t getUInt64(const std::string& column) const;
    int64_t getInt64(const std::string& column) const;
    double getDouble(const std::string& column) const;
    bool isNull(const std::string& column) const;

    size_t rowCount() const;

private:
    void buildFieldIndex();
    const char* value(const std::string& column) const;

private:
    void* result_;
    void* current_row_;
    unsigned long* lengths_;
    std::unordered_map<std::string, unsigned int> field_index_;
};

}  // namespace nebula
