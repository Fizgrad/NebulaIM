#include "common/config/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace nebula {

bool Config::loadFromFile(const std::string& file_path) {
    std::ifstream input(file_path);
    if (!input.is_open()) {
        std::cerr << "failed to open config file: " << file_path << std::endl;
        return false;
    }

    values_.clear();
    std::string line;
    size_t line_no = 0;
    while (std::getline(input, line)) {
        ++line_no;
        std::string text = trim(line);
        if (text.empty() || text.front() == '#') {
            continue;
        }

        const size_t pos = text.find('=');
        if (pos == std::string::npos) {
            std::cerr << "invalid config line " << line_no << ": " << line << std::endl;
            return false;
        }

        std::string key = trim(text.substr(0, pos));
        std::string value = trim(text.substr(pos + 1));
        if (key.empty()) {
            std::cerr << "empty config key at line " << line_no << std::endl;
            return false;
        }
        values_[key] = value;
    }

    return true;
}

bool Config::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

std::string Config::getString(const std::string& key, const std::string& default_value) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }
    return it->second;
}

int Config::getInt(const std::string& key, int default_value) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }

    try {
        size_t parsed = 0;
        int value = std::stoi(it->second, &parsed);
        if (parsed != it->second.size()) {
            return default_value;
        }
        return value;
    } catch (...) {
        return default_value;
    }
}

bool Config::getBool(const std::string& key, bool default_value) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }

    const std::string value = toLower(trim(it->second));
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return default_value;
}

std::string Config::trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (begin == s.end()) {
        return "";
    }
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string(begin, end);
}

std::string Config::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

}  // namespace nebula
