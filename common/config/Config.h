#pragma once

#include <string>
#include <unordered_map>

namespace nebula {

class Config {
public:
    bool loadFromFile(const std::string& file_path);

    bool has(const std::string& key) const;

    std::string getString(const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& key, int default_value = 0) const;
    bool getBool(const std::string& key, bool default_value = false) const;

private:
    std::unordered_map<std::string, std::string> values_;

    static std::string trim(const std::string& s);
    static std::string toLower(std::string s);
};

}  // namespace nebula
